# Sweep Lab V0.5 — Codex prework / source evaluation

Base: the working V0.4 Force native RTA (`feature/force-native-rta-v04-auto-service`). This prework branch is **not a finished sweep implementation**.

## User-approved design

Two distinct modes, not one replacement:
- **Live RTA:** retain V0.4 UMIK-1 continuous capture, 16/31-band red LED meters, automatic daemon start/stop and current Force effect behavior.
- **Offline Sweep Lab:** on a deliberate button press, route a safe exponential sine sweep to chosen Force output and record UMIK-1; **after the sweep finishes**, compute, render, store and compare frequency-response and impulse-response graphs. Never continually analyze sweep capture for display.

Keep UID `TfRA`, all existing parameter indices, and Force Internal Audio playback. Do not alter V0.4 installation during implementation.

## Evaluated open-source reference projects

- [loudifier/chirplab](https://github.com/loudifier/chirplab): MIT; open-loop log chirp, recording, analysis, noise-floor estimation. Closest end-to-end measurement workflow; Python/Qt not suitable for direct Force embedding.
- [pyfar/pyfar](https://github.com/pyfar/pyfar): MIT; sweep generation, inverse filtering, regularized frequency-domain deconvolution, impulse-response analysis. Use as numerical ground-truth fixture generator and validation.
- [Open Sound Meter](https://opensoundmeter.com/): GPLv3; transfer functions, coherence, IR, file exports. UI/measurement inspiration; **do not copy GPL code without explicit license review**.
- [AntonioEscamilla/Criteria](https://github.com/AntonioEscamilla/Criteria): C++/JUCE swept-sine IR and room acoustic parameter implementation. Investigate algorithms and license.
- [alex-russo/rconvolve](https://github.com/alex-russo/rconvolve): Rust deconvolution and swept-sine IR extraction. Investigate license and porting costs.
- [t2ow/RT60-Room-Impulse-Response](https://github.com/t2ow/RT60-Room-Impulse-Response): logged sweep IR and room-decay/RT60 estimation. Reference only until acoustic validity proven.

The current V0.4 service uses PFFFT and ALSA `hw:U18dB,0`; avoid importing Python/Qt or a second heavy FFT library into the real-time Force audio callback.

## Capture/clock boundaries

The Force plugin produces the excitation in the host audio path with controlled output routing and a conservative **-30 dBFS peak default**, fades, hard cancel and no autoplay on state restore. A worker records UMIK-1 independently. The Force output and UMIK capture run on **different clocks**: start offset must be determined via correlation/preamble, with pre/post-roll; actual drift requires more than a single alignment point. Persist the reference sweep, recording, sample-rate metadata and drift diagnostics.

The microphone must have **one owner**. Coordinate V0.4 RTA capture handoff explicitly and guarantee recovery (success, cancel, error, power interruption); do not kill another process or change MPC's audio device globally. Consider a capture interface within the existing daemon to avoid ALSA contention.

## Minimum deliverable

1. Offline exponential sweep generator and inverse/deconvolution implementation in portable C/C++ with bounded memory.
2. Automated synthetic tests: gain/latency impulse, peaks/dips, reflections, drift, noise, clipping, truncated capture.
3. Safe explicit Force sweep playback and mic recording state machine: IDLE, ARMED, CAPTURING, PROCESSING, READY/ERROR/CANCELLED. No I/O, locks, memory allocation, or deconvolution in audio callback.
4. Measured impulse response and calibrated-frequency-response correction with selectable gating and 1/48–1/3 octave smoothing. SPL stays provisional until referenced.
5. Saved WAV reference/response, impulse WAV, CSV and graphical result, A/B comparison.
6. Explore real MPC TUI graph support: static image reloading may not be supported. Verify before promising high-resolution graphs in the native skin; provide a documented local viewer or truthful limited representation if needed.
7. Build ARMv7, preserve existing V0.4 tests, provide packaged installer/rollback, and **do not deploy** without separate user-directed installation.

## Codex execution order

First verify licensing, input/output routing and TUI graph capabilities. Next build offline DSP and tests; then integrate the shared UMIK service and Force playback; only then package native UI and hardware acceptance. Test on desktop with pyfar-generated known references. Do not report the system complete based only on mock data or UI scaffolding.
