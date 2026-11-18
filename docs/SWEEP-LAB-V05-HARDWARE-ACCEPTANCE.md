# Sweep Lab V0.5 hardware acceptance gates

## Package status

`force-native-v05-dsp-candidate.tar.gz` is an ARMv7 offline DSP test utility
and calibration-file bundle. It is **not** an installable VST update. Do not
replace the installed V0.4 plugin with this candidate. The V0.4 plugin and its
normal live RTA path remain unchanged.

## Required checks after native playback integration

- Verify the target's current plugin path before installation. Back up V0.4 and
  confirm rollback before any replacement.
- Keep Akai Force playback on **Internal Audio**. Confirm no code changes global
  device/routing settings.
- Verify the plugin sends the test signal only after explicit START, at the
  capped -30 dBFS default, and silences it on CANCEL, host stop, and unload.
- Measure the actual output level at the selected Force route; confirm the
  start/end marker is audible and does not create feedback in the test room.
- Confirm the V0.2 capture API reuses one ALSA owner while RTA is paused and
  resumes normal 31-band RTA after completion, cancel, USB disconnect and
  timeout. Check the UMIK remains `hw:U18dB,0`, 48 kHz packed 24-bit stereo.
- Record a real UMIK sweep. Confirm no clipping, dropout, or missing tail; inspect
  both marker confidence values and the measured clock ratio.
- Compare repeated sweeps and a known electrical/acoustic loopback. Validate
  direct arrival, reflection delay, phase and magnitude against a reference.
- Repeat with 0° and 90° calibration selected. Verify the orientation and
  2.7673 dB factor are retained as metadata; absolute SPL remains provisional
  until ADC/input sensitivity is independently referenced.
- Load two plugin instances. Verify one microphone owner, no duplicate session,
  safe cancel/unload behavior, and no audio callback stalls or audible changes
  while the sweep is inactive.
- Open exported response SVG/CSV and session artifacts off-device; repeat A/B
  comparison and check metadata/calibration files are preserved.

The current source implements and host-tests the offline analyzer and exclusive
service recording API. It does not yet implement the VST START/CANCEL controls,
Force-host sweep playback, or their cross-thread handshake, so those tests are
not currently runnable on Force hardware.
