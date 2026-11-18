# TROLLfx RTA V0.5 controlled Force installation and acceptance

This package is a hardware-test candidate. Cross-compilation and desktop host
tests do not establish compatibility with the Force touchscreen, audio graph,
or real UMIK-1 capture. Do not change `MPC.settings` or global Force audio
routing. Keep Internal Audio selected.

## Install

1. Quit MPC and safely eject the media used by the Force.
2. On the development machine, mount the Force media and identify its actual
   mount root. Confirm that it contains `VST2 Plugins/` and the installed V0.4
   RTA directory below. The script refuses a missing/unknown installation.
3. Verify/extract the package and install:

```sh
sha256sum -c dist/force-native-v05-package.tar.gz.sha256
tar -xzf dist/force-native-v05-package.tar.gz -C /tmp
cd /tmp/force-native-v05-package
bash "TROLLfx - VST - TROLLfx - VST - RTA/verify-force-v05.sh" \
  "TROLLfx - VST - TROLLfx - VST - RTA"
bash "TROLLfx - VST - TROLLfx - VST - RTA/install-force-v05.sh" \
  /media/<confirmed-force-media-root> \
  "$(pwd)/TROLLfx - VST - TROLLfx - VST - RTA"
```

The script makes a timestamped V0.4 backup under
`TROLLfx-RTA-backups/`. Copy the printed rollback command before safely
ejecting media. After install, reconnect the media and launch MPC. Never run
the script against a system root or an unconfirmed drive.

## Rollback

Quit MPC and mount the same media, then use the exact backup directory printed
by the installer:

```sh
bash /path/to/package/TROLLfx\ -\ VST\ -\ TROLLfx\ -\ VST\ -\ RTA/rollback-force-v05.sh \
  /media/<confirmed-force-media-root> \
  /media/<confirmed-force-media-root>/TROLLfx-RTA-backups/<timestamp>
```

The rollback script only replaces the current TfRA V0.5 plugin with a TfRA
V0.4 backup. It does not touch `MPC.settings` or other plugin directories.

## On-device acceptance

- Confirm MPC detects **TROLLfx - VST - RTA**, UID `TfRA`, with the existing
  saved project and V0.4 settings intact.
- Confirm the live 31-band RTA and service AUTO lifecycle work as before.
- On a safe monitor path at low level, insert the effect and check sweep state
  moves through STARTING, ARMED, CAPTURING, PROCESSING, READY. Confirm both
  output channels emit the same sweep and return to normal pass-through after
  the measurement; verify CANCEL fades out and returns to pass-through.
- Sweep playback is generated at the host sample rate; the reference is
  resampled to 48 kHz to match the UMIK service. Test with Force project rates
  supported by the host and inspect marker drift/confidence in each saved
  session. Confirm the acoustic loopback remains stable at intended level.
- Verify no clipping, no monitor feedback, no analyzer contention, and no
  change to Force global playback routing. Confirm the saved session contains
  capture/reference/IR/CSV/SVG/JSON and the used calibration curve.
- Compare a known acoustic loopback against a calibrated reference instrument.
  Absolute SPL remains PROVISIONAL until microphone sensitivity, USB input gain,
  and reference SPL have been independently verified.
- Measure audio dropouts, plugin CPU, UI responsiveness and timing with the
  complete live project. Roll back immediately if playback or capture is
  unstable.

The native Force UI exposes START/CANCEL and sweep setup parameters. The SDK
has no tested dynamic plot component with multiple traces; the full response
graph is saved as an SVG and CSV in each session folder for later viewing.
