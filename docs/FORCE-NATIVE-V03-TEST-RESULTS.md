# V0.3 build and automated test results

Build performed on x86-64 Ubuntu using InMusic MPC Plugin Builder SDK revision `febd2ca5bd9eb16c4a412837196e145566bc981b` and Zig `0.17.1`.

## Build and ABI

- SDK native build and parameter dump: pass.
- ARMv7 hard-float VST2: ELF32, ARM v7, EABI5, VFP argument ABI; about 218 KiB.
- ARM64 VST2: AArch64 ELF64; about 273 KiB.
- Both export `VSTPluginMain`; UID `TfRA`, name `TROLLfx - VST - RTA`, vendor `TROLLfx`, stereo effect I/O verified against the binary manifest.
- Maximum required GLIBC symbol version: 2.30. The SDK validator reported only libc/libm/libpthread/libdl and the ARM loader as dependencies.
- SDK strict package validation: 0 errors, 0 warnings. Four skin tabs, all parameter handles valid, all film-strip/background images included.

## Host and deterministic tests

- MPC SDK host simulator: 20 passed, 0 warnings, 0 failures. Covers VST load/init, prepare, block-size changes, parameter sweep/state restore, in-place pass-through, finite output, silence and teardown.
- Native VST fixture: pass for two instances sharing one poller, initial HTTP 503 to HTTP/1.1 online recovery, 31-band snapshot and service peak-marker updates, 16-band power aggregation, control defaults/ranges, UID/I/O, bit-transparent separate-buffer pass-through, shutdown and unload.
- Spectrum API tests: valid/provisional and user-reference JSON, 0°/90° orientation, `2.7673 dB` sensitivity metadata, malformed/missing arrays, and 16-band power combination pass.
- V0.2 deterministic DSP/calibration tests: all passed, including 50 Hz–10 kHz band location, normalization/leakage, broadband/pink noise, both supplied calibration curves, sensitivity factor, interpolation/orientation and SPL/A-weight calculations.
- Installation/rollback scripts: temporary-media dry run passed; no Force path was used.

## Desktop measurements

- SDK host-sim audio callback timing (x86-64, 128-frame block): 0.00% average, 0.03% worst in the latest run. The effect callback does only pass-through; this is not Akai Force CPU evidence.
- Existing V0.2 engine benchmark in the same test run: V0.1.2 8192-point FFT 0.449 ms/frame; V0.2 32768-point FFT and band integration 0.846 ms/frame on this host (1.88× work per frame for 4× samples). These figures vary with host load and are not hardware measurements.

## Package contents

`dist/force-native-v03-package/TROLLfx - VST - TROLLfx - VST - RTA/` contains plugin metadata, version, TUI/Q-Link skins, generated art, third-party notices and both ARM binaries. The package root contains explicit-target install, verification and rollback scripts plus this hardware-test guide.

## Not tested here

No Akai Force or UMIK-1 was attached. Firmware plugin discovery, actual Force UI refresh/film-strip appearance, Q-Link behavior, hardware CPU/audio glitches with Doppler and Chakra, UMIK disconnect/reconnect against real ALSA capture, and traceable absolute SPL calibration remain hardware acceptance tests. Do not infer these from the host simulator or cross-build.
