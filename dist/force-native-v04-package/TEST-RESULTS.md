# V0.4 build and test results

Development build: 2026-11-17, MPC Plugin Builder SDK revision `febd2ca5bd9eb16c4a412837196e145566bc981b`, Zig 0.17.1, ARMv7 hard-float target `arm-linux-gnueabihf.2.31`.

## Automated results

- MPC SDK strict plugin validation: 0 errors, 0 warnings. All skin parameter handles resolve through index 114.
- SDK host simulator: 20 passed, 0 warnings, 0 failures. Stereo pass-through, parameter/state round-trip, initialization/shutdown, and block-size checks passed.
- Existing V0.2/V0.3 tests: passed, including both serial 7033298 calibration files, 2.7673 dB sensitivity metadata, orientation interpolation, SPL mapping, spectral bands, parser validation, and display rendering.
- Native VST host fixture: passed UID `TfRA`, stereo 2-in/2-out, transparent pass-through, unchanged V0.3 parameter order through index 110, new controls at 111–114, loading a V0.3 saved-state chunk with retained non-default values, external API reconnect and external-service preservation.
- Service lifecycle mocks: passed shared startup, keep-alive after one instance closes, final owned-process shutdown, crash/reconnect, unavailable microphone without respawn loop, AUTO/MANUAL and momentary actions, missing executable status, occupied port, startup timeout, host shutdown during startup, and rapid load/unload.
- Analyzer port-conflict test: passed; a busy API port is rejected before ALSA is opened.
- Package install/rollback dry run: passed on a temporary fake drive. It backed up a V0.3 marker, staged V0.4, restored V0.3, and did not create or modify `MPC.settings`.
- ARM package verification: VST is ELF32 ARM EABI5, ARMv7, VFP-register hard-float; exports `VSTPluginMain`; UID `TfRA`; service is ELF32 ARM EABI5, ARMv7 hard-float. Package has both original calibration files and PFFFT license.

The SDK host simulator's desktop timing is recorded in the CI log only as an orientation measurement. It does not establish Akai Force CPU use, firmware compatibility, UI redraw performance, or microphone calibration. No physical Force was used for this build.

## Remaining hardware acceptance

Install only through the controlled installation script and verify plugin discovery, AUTO/MANUAL/START/STOP and Q-Link touch behavior, external-service detection, 31/16-band display updates, UMIK unplug/replug recovery, no duplicate ALSA capture, clean final unload/Force shutdown, Internal Audio playback and pass-through under a multi-plugin load, orientation, and a known acoustic SPL reference. Do not label SPL independently verified until the reference measurement is completed on the Force capture path.
