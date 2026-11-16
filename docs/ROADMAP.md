# TROLLfx RTA — Roadmap

## V0.1.2 baseline
- Working independent UMIK-1 capture on Akai Force at 48 kHz, stereo packed 24-bit
- Red 31-stack browser interface at 127.0.0.1:8096
- Preserved on `main` and available for rollback

## V0.2 measurement engine
- Added on `feature/v02-measurement-engine`
- PFFFT float transform, 32,768 sample window, power-integrated third-octave bands
- Selectable 16-band view and configurable spectrum power averaging
- Both serial 7033298 calibration curves and orientation selection
- A/Z-weighted level calculations and provisional SPL reporting until Force input sensitivity is hardware-verified
- Cross-build, deterministic synthetic DSP tests, host benchmark, manual install and rollback scripts

## Hardware verification still required
- Validate UMIK Sens Factor and ALSA PCM level relationship with a traceable 94 dB SPL calibrator on the Force
- Confirm real Force runtime cost and ALSA continuity while Force playback is active
- Confirm browser UI and both microphone orientations on the Force

No Force audio settings are changed, and V0.2 does not install a boot service.
