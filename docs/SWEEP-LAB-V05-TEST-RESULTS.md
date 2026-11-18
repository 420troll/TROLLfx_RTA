# Sweep Lab V0.5 build and test results

**Candidate branch:** `feature/v05-sweep-lab-prework`
**Build:** pinned MPC Plugin Builder SDK `febd2ca5bd9eb16c4a412837196e145566bc981b`, Zig 0.17.1
**Host:** x86_64 Linux; ARM build target: ARMv7-A hard-float / glibc 2.29 baseline

## Passed automated checks

- Offline sweep generator, regularized deconvolution, delay/reflection recovery,
  gain normalization, clipping rejection, deterministic noise, saved graph/CSV/
  WAV/JSON artifacts, clock drift estimation/correction, calibration parsing and
  44.1↔48 kHz windowed-sinc resampling.
- Independent pyfar 0.8.1 `dsp.deconvolve` comparison against the deterministic
  two-tap fixture. Direct/reflection main lobes and locations agree within 2%.
  pyfar emits an upstream deprecation warning for `dsp.deconvolve`; the pinned
  test remains passing and no pyfar runtime code is bundled.
- Mock ALSA/API tests verify exclusive sweep capture, no duplicate ALSA opens,
  live FFT pause/resume, mic-offline rejection and valid float WAV transfer.
- Native VST host tests verify `TfRA`, stereo transparent pass-through, V0.3
  saved-state loading, parameter indices 0–114 unchanged, controls appended at
  115–121, and V0.4 automatic analyzer lifecycle regressions.
- End-to-end VST worker mock at 44.1 kHz: bounded stereo sweep output, 48 kHz
  synthetic capture, two-marker alignment, completed analysis, calibration
  preservation, response/IR artifacts, and return to normal pass-through.
- SDK validator: native, ARM32 and ARM64 package variants pass identity, I/O,
  exported entry point, plugin metadata, five-page TUI, Q-Link handles and asset
  checks. ARM32 VST is hard-float, exports `VSTPluginMain`, and needs glibc no
  newer than 2.30. ARMv7 analyzer and DSP CLI are hard-float and the CLI depends
  only on libc/libm (no libstdc++ runtime).
- Dedicated native VST host harness passes pass-through, saved-state compatibility,
  lifecycle and completed-sweep tests. The SDK's generic host simulator
  round-trip is not used as a gate because it requires the read-only runtime
  Service Status to remain constant while the asynchronous worker updates it.
- Controlled install simulation validates a V0.4 target, stages V0.5, preserves
  V0.4, then restores V0.4 with the rollback script. No device was installed.
- Package `SHA256SUMS` and archive checksum pass.

## Development host measurements

One V0.2 live FFT benchmark run measured 0.456 ms/frame for V0.1.2 and 0.659
ms/frame for the 32768-point V0.2 engine (1.45× host CPU/frame, 4× FFT samples).
One 10-second 48 kHz offline analysis measured 1.79 s and 74,160 KiB peak RSS
on an Intel Core i7-10750H host. These are development-machine measurements;
neither predicts Force CPU headroom.

## Not verified by these tests

No real Force installation, touchscreen rendering, audio routing, UMIK acoustic
clock drift, output/ADC latency, loopback response, SPL reference, or live-project
CPU/dropout test has been performed. Follow
`SWEEP-LAB-V05-HARDWARE-ACCEPTANCE.md` before treating acoustic measurements or
Force-host behavior as accepted. Absolute SPL remains provisional.
