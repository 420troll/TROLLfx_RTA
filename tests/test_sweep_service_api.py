"""Safety checks for the V0.5 capture extension without physical ALSA hardware."""
import json
import os
import pathlib
import socket
import subprocess
import tempfile
import time
import unittest
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0)); return s.getsockname()[1]


class TestSweepCaptureApi(unittest.TestCase):
    def build_daemon(self, output):
        subprocess.run(["gcc", "-O2", "-std=c11", "-Isrc", "-Isrc/third_party/pffft",
                        "src/trollfx_rta.c", "src/dsp.c", "src/third_party/pffft/pffft.c",
                        "src/third_party/pffft/pffft_common.c", "-lm", "-lpthread", "-ldl",
                        "-o", str(output)], cwd=ROOT, check=True)

    def test_missing_mic_never_arms_or_returns_a_recording(self):
        port = free_port()
        with tempfile.TemporaryDirectory() as td:
            exe = pathlib.Path(td) / "trollfx-rta"
            self.build_daemon(exe)
            proc = subprocess.Popen([str(exe), "--device", "trollfx-no-such-UMIK", "--port", str(port)],
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            try:
                url = f"http://127.0.0.1:{port}"
                ready = False
                for _ in range(100):
                    if proc.poll() is not None: self.fail("measurement service exited before API became ready")
                    try:
                        with urllib.request.urlopen(url + "/api/sweep/status", timeout=.2) as response:
                            status = json.loads(response.read()); ready = True; break
                    except Exception: time.sleep(.05)
                self.assertTrue(ready)
                self.assertIn(status["state"], ("IDLE", "ERROR"))
                req = urllib.request.Request(url + "/api/sweep/arm?seconds=1", method="POST")
                with urllib.request.urlopen(req, timeout=1) as response: arm = json.loads(response.read())
                self.assertFalse(arm["ok"])
                self.assertNotEqual(arm["state"], "CAPTURING")
                req = urllib.request.Request(url + "/api/sweep/finish", method="POST")
                with urllib.request.urlopen(req, timeout=1) as response: finish = json.loads(response.read())
                self.assertFalse(finish["ok"])
                with urllib.request.urlopen(url + "/api/sweep/capture.wav", timeout=1) as response:
                    capture = json.loads(response.read())
                self.assertFalse(capture["ok"])
            finally:
                proc.terminate()
                try: proc.wait(timeout=2)
                except subprocess.TimeoutExpired: proc.kill(); proc.wait(timeout=2)

    def test_capture_excludes_live_fft_and_reuses_single_alsa_owner(self):
        port = free_port()
        with tempfile.TemporaryDirectory() as td:
            td = pathlib.Path(td); exe = td / "trollfx-rta"; mock = td / "mock-alsa"
            mock.mkdir(); count_path = td / "alsa-opens"
            self.build_daemon(exe)
            subprocess.run(["gcc", "-shared", "-fPIC", "-O2", str(ROOT / "tests/mock_alsa.c"), "-o", str(mock / "libasound.so.2")], check=True)
            env = os.environ.copy(); env["LD_LIBRARY_PATH"] = str(mock) + ":" + env.get("LD_LIBRARY_PATH", ""); env["MOCK_ALSA_OPEN_COUNT"] = str(count_path)
            proc = subprocess.Popen([str(exe), "--device", "mock-UMIK", "--port", str(port)], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            url = f"http://127.0.0.1:{port}"
            try:
                spectrum = None
                for _ in range(100):
                    if proc.poll() is not None: self.fail("service exited with mock ALSA")
                    try:
                        with urllib.request.urlopen(url + "/api/spectrum", timeout=.2) as r: spectrum = json.loads(r.read())
                        if spectrum.get("ok"): break
                    except Exception: pass
                    time.sleep(.03)
                self.assertTrue(spectrum and spectrum["ok"], "mock capture never came online")
                arm_req = urllib.request.Request(url + "/api/sweep/arm?seconds=0.5", method="POST")
                with urllib.request.urlopen(arm_req, timeout=1) as r: arm = json.loads(r.read())
                self.assertTrue(arm["ok"])
                for _ in range(40):
                    with urllib.request.urlopen(url + "/api/sweep/status", timeout=1) as r: state = json.loads(r.read())
                    if state["state"] == "CAPTURING": break
                    time.sleep(.01)
                self.assertEqual(state["state"], "CAPTURING")
                duplicate = urllib.request.Request(url + "/api/sweep/arm?seconds=0.1", method="POST")
                with urllib.request.urlopen(duplicate, timeout=1) as r: self.assertFalse(json.loads(r.read())["ok"])
                with urllib.request.urlopen(url + "/api/spectrum", timeout=1) as r: frozen_rms = json.loads(r.read())["rms_dbfs"]
                time.sleep(.18)
                with urllib.request.urlopen(url + "/api/spectrum", timeout=1) as r: during = json.loads(r.read())
                self.assertEqual(during["rms_dbfs"], frozen_rms, "live FFT advanced while sweep recording was active")
                finish_req = urllib.request.Request(url + "/api/sweep/finish", method="POST")
                with urllib.request.urlopen(finish_req, timeout=2) as r: finish = json.loads(r.read())
                self.assertTrue(finish["ok"]); self.assertGreater(finish["samples"], 1000); self.assertEqual(finish["clipped_samples"], 0)
                with urllib.request.urlopen(url + "/api/sweep/capture.wav", timeout=2) as r: wav = r.read()
                self.assertEqual(wav[:4], b"RIFF"); self.assertEqual(wav[8:12], b"WAVE"); self.assertEqual(int.from_bytes(wav[20:22], "little"), 3)
                self.assertEqual(int.from_bytes(wav[24:28], "little"), 48000)
                frames = int.from_bytes(wav[40:44], "little") // 4
                self.assertEqual(len(wav), 44 + frames * 4); self.assertEqual(frames, finish["samples"])
                self.assertTrue(any(wav[44:]), "mock capture WAV has no data")
                with urllib.request.urlopen(url + "/api/sweep/status", timeout=1) as r: idle = json.loads(r.read())
                self.assertEqual(idle["state"], "IDLE")
                # A vanished controller is bounded by the requested duration,
                # then a 15 s artifact-download lease before live FFT resumes.
                arm_req = urllib.request.Request(url + "/api/sweep/arm?seconds=0.1", method="POST")
                with urllib.request.urlopen(arm_req, timeout=1) as r: self.assertTrue(json.loads(r.read())["ok"])
                for _ in range(100):
                    with urllib.request.urlopen(url + "/api/sweep/status", timeout=1) as r: state = json.loads(r.read())
                    if state["state"] == "READY": break
                    time.sleep(.02)
                self.assertEqual(state["state"], "READY")
                cancel_req = urllib.request.Request(url + "/api/sweep/cancel", method="POST")
                with urllib.request.urlopen(cancel_req, timeout=1) as r: cancel = json.loads(r.read())
                self.assertTrue(cancel["ok"]); self.assertEqual(cancel["state"], "IDLE")
                self.assertEqual(count_path.read_text().splitlines(), ["open"], "service opened more than one ALSA capture owner")
            finally:
                proc.terminate()
                try: proc.wait(timeout=2)
                except subprocess.TimeoutExpired: proc.kill(); proc.wait(timeout=2)


if __name__ == "__main__": unittest.main()
