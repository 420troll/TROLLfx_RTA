# TROLLfx RTA V0.4 — automatic analyzer service

V0.4 extends the tested V0.3 native stereo VST2 effect. It keeps UID `TfRA`, the first 111 parameter identifiers and indices, the V0.2 ALSA/PFFFT measurement engine, both UMIK calibration curves, and all existing display controls. Four service controls are appended at indices 111–114: Service Mode, Analyzer START, Analyzer STOP, and Service Status.

## Lifecycle

One `SharedClient` worker exists per loaded VST library in an MPC host process. Plugin construction registers an instance; instance destruction unregisters it. Closing the touchscreen view does not destroy the effect, so service lifetime follows inserted plugin instances. `process()` remains a stereo memory copy for separate buffers and does no locks, service work, allocation, I/O, or waiting.

In AUTO mode, the worker probes the local API first. A valid existing V0.2 API is treated as external and is never signaled. If no API is available, a `flock` lock in `/tmp` coordinates different host processes, then the worker launches the bundled service with `posix_spawn`. The lock is released when that owned child exits. The service binds `127.0.0.1:8096` before it starts ALSA capture, preventing a losing port contender from opening the microphone. Runtime files are resolved relative to the loaded VST shared object with `dladdr`: `VST/../Service/` contains the executable and calibration directory; no media drive path or current working directory is assumed.

The worker keeps the child PID it received from `posix_spawn`, reaps it with `waitpid`, verifies `/proc/<pid>/exe` matches the bundled executable before signaling, sends SIGTERM and waits at most two seconds, then sends SIGKILL only after the same identity check. Startup is limited to ten seconds; failed starts use bounded backoff and stop after three attempts. A busy supervisor lock waits for the other host to bring the API up. Crashed owned processes are retried with backoff. The analyzer retries ALSA open/read after a two-second delay, so a disconnected UMIK can return without launching another daemon. Capture remains on `hw:U18dB,0`, S24_3LE, 48 kHz, and does not alter Force playback routing.

AUTO starts on first instance and stops an owned process when the final instance unloads. MANUAL does not start on load; START launches it. STOP terminates only a service this plugin owns. In AUTO, STOP suppresses automatic restart until START is pressed, Service Mode is changed, or all instances unload and the plugin is loaded again. In MANUAL, STOP leaves the service stopped until START. Externally started analyzers continue running in either mode. Service Status reports STOPPED, STARTING, ONLINE, STOPPING, ERROR, or EXTERNAL; microphone ONLINE is reported separately only when the API says capture is live.

## Native controls

The MPC Plugin Builder SDK offers parameter-bound touchscreen sliders/switches and status labels, but no arbitrary custom button API. DISPLAY includes the AUTO/MANUAL choice and large parameter-bound START/STOP actions; each Boolean action resets to false immediately after dispatch. Service Status is a parameter-bound readout. Q-Links 11–13 map to mode, start, and stop. Existing Q-Links and parameter order remain intact.

## Bundled service and calibration

The installable folder includes ARMv7 hard-float `Service/trollfx-rta-v02-armv7`, `Service/calibration/7033298.txt`, `Service/calibration/7033298_90deg.txt`, and the PFFFT license. Calibration orientation remains the service's `--orientation` selection. The plugin starts at 0° and does not invent an SPL offset; SPL remains provisional until the existing independent reference workflow is used.

The API and log are local to the Force. Logs go to `/tmp/trollfx-rta-v04.log`; the executable and calibration live persistently beside the installed VST. The package includes the MPC SDK notice and PFFFT notice.

## Build and tests

Use the pinned InMusic SDK revision `febd2ca5bd9eb16c4a412837196e145566bc981b` and Zig 0.17.1. The VST and service both target ARMv7 hard-float with GLIBC 2.31 compatibility. Set `MPC_SDK_DIR`, `SDK_ZIG_PYTHON`, and `PYTHONPATH`, then run:

```sh
bash scripts/build-force-v04.sh
```

This builds/validates ARM32 and ARM64 VST2 binaries, builds the ARMv7 analyzer, runs SDK host simulation, the V0.2 DSP/calibration suite, V0.3 display/JSON tests, stereo pass-through and VST integration tests, then lifecycle tests with local mock services. It loads a V0.3 saved-state fixture to verify old values and parameter IDs remain compatible. It creates `dist/force-native-v04-package/`. The mock tests cover shared ownership/final shutdown, external API preservation, crash recovery, offline microphone state, missing executable, port conflict, bounded startup, rapid unload, and manual mode/actions. They do not establish Force firmware or UI compatibility.

## Controlled installation and rollback

Confirm the actual mounted Force drive root and ensure it contains `VST2 Plugins/TROLLfx - VST - TROLLfx - VST - RTA` (the currently installed V0.3 folder). From the repository:

```sh
bash scripts/install-force-v04.sh /CONFIRMED/DRIVE/ROOT \
  "dist/force-native-v04-package/TROLLfx - VST - TROLLfx - VST - RTA"
```

The script verifies the package, stages it under `VST2 Plugins`, backs up the existing V0.3 folder, and prints the backup directory. Roll back with the exact path it prints:

```sh
bash scripts/rollback-force-v04.sh /CONFIRMED/DRIVE/ROOT \
  /CONFIRMED/DRIVE/ROOT/TROLLfx-RTA-backups/<printed-timestamp>
```

Neither command is run during build or CI. No `MPC.settings` changes are made.

## Hardware acceptance

The plugin needs real Force testing after controlled installation: VST browser discovery and load/unload behavior; insert/remove and GUI close while checking service ownership; touchscreen/START/STOP/AUTO/MANUAL and Q-Link response; plugin unload and Force shutdown with no zombie service; 31/16-band responsiveness; UMIK unplug/replug and capture recovery; preservation of Internal Audio playback and audio pass-through while other plugins run; external daemon detection; CPU/UI impact; orientation and known-SPL checks. Desktop mocks and cross-compilation do not prove these hardware behaviors.
