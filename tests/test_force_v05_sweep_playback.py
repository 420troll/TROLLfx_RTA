"""Exercise the real VST sweep worker/playback against a deterministic API mock."""
import http.server
import json
import os
import pathlib
import socket
import struct
import subprocess
import sys
import threading


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
    capture_wav = b""

    def log_message(self, *_args):
        pass

    def reply(self, body, content_type="application/json"):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/api/spectrum":
            levels = ",".join(["-30"] * 31)
            peaks = ",".join(["-25"] * 31)
            self.reply(json.dumps({"ok": True, "rms_dbfs": -20, "z_corrected_dbfs": -20,
                                   "a_corrected_dbfs": -20, "orientation": 0, "calibrated": False,
                                   "sensitivity_factor_db": 2.7673, "spl_z": None, "spl_a": None,
                                   "spl_reference_type": "none", "level": [float(x) for x in levels.split(",")],
                                   "peak": [float(x) for x in peaks.split(",")]}))
        elif self.path == "/api/sweep/status":
            self.reply('{"state":"CAPTURING","samples":100,"clipped_samples":0,"sample_rate_hz":48000}')
        elif self.path == "/api/sweep/capture.wav":
            self.reply(self.capture_wav, "audio/wav")
        else:
            self.send_error(404)

    def do_POST(self):
        if self.path.startswith("/api/sweep/arm?"):
            self.reply('{"ok":true,"state":"ARMED","sample_rate_hz":48000}')
        elif self.path == "/api/sweep/finish":
            self.reply('{"ok":true,"state":"READY","samples":100,"clipped_samples":0}')
        elif self.path == "/api/sweep/cancel":
            self.reply('{"ok":true,"state":"IDLE"}')
        else:
            self.send_error(404)


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: test_force_v05_sweep_playback.py HOST_TEST VST_SO CAPTURE_WAV")
    port = free_port()
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    Handler.capture_wav = pathlib.Path(sys.argv[3]).read_bytes()
    env = os.environ.copy()
    env["TROLLFX_RTA_PORT"] = str(port)
    try:
        result = subprocess.run([sys.argv[1], pathlib.Path(sys.argv[2]).resolve(), "--sweep-playback"],
                                env=env, timeout=20, check=False)
        if result.returncode:
            raise SystemExit(result.returncode)
        sessions = pathlib.Path(sys.argv[2]).resolve().parent.parent / "Service" / "sessions"
        found = sorted(sessions.glob("sweep-*/measurement.json"))
        if not found:
            raise SystemExit("VST worker completed without saving measurement.json")
        session = found[-1].parent
        for filename in ("reference.wav", "capture.wav", "impulse.wav", "frequency-response.csv",
                         "frequency-response.svg", "measurement.json", "calibration-used.txt"):
            if not (session / filename).is_file():
                raise SystemExit(f"completed sweep session is missing {filename}")
        metadata = json.loads((session / "measurement.json").read_text())
        if not metadata.get("marker_alignment_valid"):
            raise SystemExit("saved sweep session lacks valid two-marker alignment")
        print(f"PASS completed VST sweep saved aligned response, IR and graphs at {session}")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


if __name__ == "__main__":
    main()
