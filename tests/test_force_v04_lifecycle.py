#!/usr/bin/env python3
"""Process ownership and lifecycle integration tests using a local fake API service."""
import os
import socket
import subprocess
import sys
import tempfile
import shutil

def unused_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]

def main():
    if len(sys.argv) != 4:
        print("Usage: test_force_v04_lifecycle.py <native-host-test> <plugin-so> <build-dir>", file=sys.stderr)
        return 2
    host, plugin, build_dir = sys.argv[1:]
    mock = os.path.join(build_dir, "mock-rta-service")
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "tests/mock_rta_service.c", "-o", mock], check=True)
    cases = [
        ("--service-lifecycle", {}),
        ("--service-recovery", {"TROLLFX_RTA_TEST_CRASH_FIRST": "1"}),
        ("--service-offline", {"TROLLFX_RTA_TEST_MIC_OFFLINE": "1"}),
        ("--service-manual", {}),
        ("--service-shutdown-starting", {"TROLLFX_RTA_TEST_NO_API": "1", "TROLLFX_RTA_TEST_RETRY_DELAY_MS": "5000"}),
        ("--service-timeout", {"TROLLFX_RTA_TEST_NO_API": "1", "TROLLFX_RTA_TEST_STARTUP_TIMEOUT_MS": "500", "TROLLFX_RTA_TEST_RETRY_DELAY_MS": "5000"}),
        ("--service-rapid", {}),
    ]
    for mode, extra in cases:
        with tempfile.TemporaryDirectory(prefix="trollfx-rta-v04-") as temp:
            port = unused_port()
            env = os.environ.copy()
            env.update({"TROLLFX_RTA_PORT": str(port),
                        "TROLLFX_RTA_LOCK_PATH": os.path.join(temp, "service.lock"),
                        "TROLLFX_RTA_TEST_PIDFILE": os.path.join(temp, "service.pids"),
                        "TROLLFX_RTA_SERVICE_EXECUTABLE": mock})
            env.update(extra)
            subprocess.run([host, plugin, mode], env=env, check=True, timeout=18)
    with tempfile.TemporaryDirectory(prefix="trollfx-rta-v04-location-") as temp:
        root = os.path.join(temp, "installed-plugin")
        vst = os.path.join(root, "VST")
        service = os.path.join(root, "Service")
        os.makedirs(vst); os.makedirs(os.path.join(service, "calibration"))
        shutil.copy2(plugin, os.path.join(vst, "rta-native.so"))
        shutil.copy2(mock, os.path.join(service, "trollfx-rta-v02-armv7"))
        for name in ("7033298.txt", "7033298_90deg.txt"):
            shutil.copy2(os.path.join("calibration", name), os.path.join(service, "calibration", name))
        port = unused_port()
        env = os.environ.copy()
        env.update({"TROLLFX_RTA_PORT": str(port),
                    "TROLLFX_RTA_LOCK_PATH": os.path.join(temp, "service.lock"),
                    "TROLLFX_RTA_TEST_PIDFILE": os.path.join(temp, "service.pids")})
        env.pop("TROLLFX_RTA_SERVICE_EXECUTABLE", None)
        subprocess.run([host, os.path.join(vst, "rta-native.so"), "--service-lifecycle"],
                       env=env, check=True, timeout=12)
    with tempfile.TemporaryDirectory(prefix="trollfx-rta-v04-port-") as temp:
        port = unused_port()
        blocker = socket.socket()
        blocker.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        blocker.bind(("127.0.0.1", port)); blocker.listen(8)
        env = os.environ.copy()
        env.update({"TROLLFX_RTA_PORT": str(port),
                    "TROLLFX_RTA_LOCK_PATH": os.path.join(temp, "service.lock"),
                    "TROLLFX_RTA_TEST_PIDFILE": os.path.join(temp, "service.pids"),
                    "TROLLFX_RTA_SERVICE_EXECUTABLE": mock,
                    "TROLLFX_RTA_TEST_RETRY_DELAY_MS": "5000"})
        try:
            subprocess.run([host, plugin, "--service-occupied"], env=env, check=True, timeout=8)
        finally:
            blocker.close()
    with tempfile.TemporaryDirectory(prefix="trollfx-rta-v04-missing-") as temp:
        env = os.environ.copy()
        env.update({"TROLLFX_RTA_PORT": str(unused_port()),
                    "TROLLFX_RTA_LOCK_PATH": os.path.join(temp, "service.lock"),
                    "TROLLFX_RTA_TEST_PIDFILE": os.path.join(temp, "service.pids"),
                    "TROLLFX_RTA_SERVICE_EXECUTABLE": os.path.join(temp, "missing-analyzer"),
                    "TROLLFX_RTA_TEST_RETRY_DELAY_MS": "100"})
        subprocess.run([host, plugin, "--service-missing"], env=env, check=True, timeout=8)
    print("PASS V0.4 lifecycle matrix: relative persistent service lookup, external preservation, sharing/final-stop, crash recovery, mic unavailable, manual controls, missing executable, port conflict, startup timeout/shutdown, rapid reload")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
