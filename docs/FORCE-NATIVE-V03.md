# TROLLfx RTA — native Akai Force touchscreen V0.3 (experimental)

## Target

Create a VST2 **effect** using the Akai MPC Plugin SDK / Open MPC skin conventions. The Force should display the analyzer **inside MPC's plugin view**, not in an external browser. Preserve the separate V0.2 capture service and measurement corrections as the trusted measurement source.

## Current implementation

This branch contains a standalone, testable C adapter in `src/force/force_rta_display.[ch]`:
- Converts 31 response-corrected measurement bands and peak levels into 24-step red LED values.
- Produces **31** or **16** visible bands. The 16-band mode sums *power*, not decibels, for adjacent measurements.
- Uses the established +11 dB display gain and -90 dBFS floor as configurable inputs.
- Does not touch Force ALSA, internal-audio routing, or the MPC host; safe to test on desktop.
- Test: `bash scripts/test-force-display.sh`.

**Not yet a loadable VST binary.** The Force-native plugin wrapper, artwork, live parameter transport and ARM host integration remain to be implemented and validated. Do not install this branch as an MPC plugin.

## Architecture

```
UMIK-1 USB -> standalone V0.2 ARM measurement daemon (port 8096)
             -> 31-band and peak measurements, LZ / LA, validity
             -> localhost reader (separate worker, no audio-thread I/O)
             -> native VST2 UI parameters -> Force skin (LED strips, labels)
```

Keep USB capture out of the VST audio callback. A failed/missing daemon must display **MIC OFFLINE** rather than hang MPC. Never poll HTTP, parse JSON, allocate, lock, or call ALSA in `processReplacing`. Use a bounded-rate worker and atomic snapshots. Avoid mutating `MPC.settings` without backups. Keep plugin UID distinct from existing TROLLfx effects.

The native Force skin supports bitmaps, knobs, parameter-bound graphics and static labels, but **not arbitrary live-drawn FFT plots**. Visualize LEDs with discrete, pre-rendered image states or parameter graphics; measure UI thread cost on actual Force hardware before using 31 animated strips. Try 16-band as lightweight fallback.

## Planned native UI

- **ANALYZER** page: 31/16 red segmented stacks; optional peak LEDs; mic/connection status.
- **DISPLAY** page: gain, floor, LED attack 50 ms, LED fall 33 dB/s, peak hold 200 ms, peak fall 60 dB/s.
- **LEVEL** page: broadband raw dBFS; LZ and LA levels. Display **SPL PROVISIONAL** until actual acoustic reference is supplied. Distinguish **USER REF**.
- Optional native buttons for reset peaks, calibration orientation 0°/90°, hold and mode.

## Hardware acceptance gates

1. Force's **Internal Audio** remains selected; reconnect UMIK without disrupting playback.
2. Native effect appears in plugin browser and loads without changes to other TROLLfx effects.
3. 31/16 stack and peak levels track the localhost analyzer, without a browser/tunnel.
4. Plug/unplug UMIK and stop/restart daemon: effect shows offline and recovers gracefully.
5. Run three Doppler instances plus Chakra while watching CPU, display refresh and audio glitches.
6. Calibration remains provisional until a known 94 dB SPL reference or measured REW comparison is applied.

This avoids claiming that a browser web page can simply be embedded as a native VST on Akai Force.
