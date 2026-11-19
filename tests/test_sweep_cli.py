"""End-to-end CLI test for marked sweep generation and automatic drift fit."""
import json
import os
import pathlib
import struct
import subprocess
import tempfile
import unittest
import wave

ROOT = pathlib.Path(__file__).resolve().parents[1]
CLI = pathlib.Path(os.environ.get("TROLLFX_SWEEP_CLI", ROOT / "build/trollfx-sweep-cli"))


def write_float_wav(path, samples, fs):
    with wave.open(str(path), "wb") as fp:
        fp.setnchannels(1); fp.setsampwidth(4); fp.setframerate(fs)
        fp.setcomptype("NONE", "not compressed")
        fp.writeframes(b"".join(struct.pack("<f", x) for x in samples))


class TestSweepCLI(unittest.TestCase):
    def test_two_markers_find_100ppm_and_save_session(self):
        with tempfile.TemporaryDirectory() as td:
            td = pathlib.Path(td); reference = td / "reference.wav"
            subprocess.run([str(CLI), "generate-test", str(reference), "8000", "1", "40", "3000", "-30"], check=True, capture_output=True)
            meta = json.loads(pathlib.Path(str(reference) + ".json").read_text())
            raw = reference.read_bytes()[44:]
            samples = list(struct.unpack("<" + "f" * (len(raw) // 4), raw))
            ratio, lead = 1.0001, 317
            capture = [0.0] * (lead + int(len(samples) * ratio) + 100)
            for i in range(len(capture) - lead):
                at = i / ratio; j = int(at); frac = at - j
                if j + 1 < len(samples): capture[lead + i] = samples[j] * (1 - frac) + samples[j + 1] * frac
            capture_path = td / "capture.wav"; write_float_wav(capture_path, capture, 8000)
            output = td / "measurement"
            cal = ROOT / "calibration/7033298_90deg.txt"
            completed = subprocess.run([str(CLI), "analyze", str(reference), str(capture_path), str(output), "0", "40", "3000", str(cal), "90"], check=True, text=True, capture_output=True)
            result = json.loads((output / "measurement.json").read_text())
            self.assertIn("estimated clock=", completed.stdout)
            self.assertTrue(result["marker_alignment_valid"])
            self.assertLess(abs(result["clock_correction_ppm"] - 100), 20)
            self.assertGreater(result["marker_confidence"], .5)
            self.assertEqual(result["captured_first_marker_sample"] - meta["marker_start_sample"], lead)
            self.assertTrue(result["microphone_frequency_correction_applied"])
            self.assertEqual(result["calibration_orientation_deg"], 90)
            self.assertEqual(result["sensitivity_factor_db_metadata_only"], 2.7673)
            self.assertIsNotNone(result["calibration_hash_fnv1a64"])
            self.assertEqual((output / "calibration-used.txt").read_bytes(), cal.read_bytes())
            for name in ("reference.wav", "capture.wav", "impulse.wav", "frequency-response.csv", "frequency-response.svg", "measurement.json", "reference.wav.json"):
                self.assertTrue((output / name).exists(), name)
            raw_before = (output / "frequency-response.csv").read_bytes()
            for choice, denominator in (("none", None), ("1/48", 48), ("1/24", 24), ("1/12", 12), ("1/6", 6), ("1/3", 3)):
                subprocess.run([str(CLI), "reprocess", str(output), choice], check=True, capture_output=True)
                self.assertEqual((output / "frequency-response.csv").read_bytes(), raw_before)
                settings = json.loads((output / "measurement.json").read_text())
                self.assertEqual(settings["response_smoothing_octaves"], denominator)
                self.assertEqual(settings["smoothing_algorithm_version"], 1)
                self.assertTrue((output / "frequency-response-smoothed.csv").exists())


if __name__ == "__main__": unittest.main()
