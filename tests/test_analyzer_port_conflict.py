#!/usr/bin/env python3
"""Verify a busy API port is rejected before the analyzer opens ALSA."""
import socket
import subprocess
import sys

def main():
    if len(sys.argv) != 2:
        print("Usage: test_analyzer_port_conflict.py <native-analyzer>", file=sys.stderr)
        return 2
    with socket.socket() as blocker:
        blocker.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        blocker.bind(("127.0.0.1", 0)); blocker.listen(1)
        port = blocker.getsockname()[1]
        result = subprocess.run([sys.argv[1], "--device", "missing-device", "--port", str(port),
                                 "--cal-0", "calibration/7033298.txt",
                                 "--cal-90", "calibration/7033298_90deg.txt"],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=3)
        if result.returncode == 0 or "bind/listen" not in result.stderr:
            print("FAIL busy port was not rejected at the listener before capture", result.stderr)
            return 1
        print("PASS busy API port exits before opening ALSA capture")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
