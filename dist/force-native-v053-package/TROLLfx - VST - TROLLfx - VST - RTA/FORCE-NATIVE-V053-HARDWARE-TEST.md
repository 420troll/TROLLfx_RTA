# V0.5.3 Force hardware acceptance

This is a controlled evaluation package. Do not install while MPC has the RTA plugin loaded or while a performance project is active. Keep playback on **Internal Audio**. The V0.5.3 build does not change global audio routing and is not installed by the build script.

1. Verify the archive SHA256 and run the packaged `verify-force-v053.sh` against the plugin folder.
2. Confirm the actual SSD mount and plugin folder. The package installer backs up the installed RTA folder and carries the existing `Service/sessions` directory forward byte-for-byte. No unrelated plugin folder is targeted. Example after extraction and confirming the mount:

   ```sh
   PLUGIN='/path/to/extracted/TROLLfx - VST - TROLLfx - VST - RTA'
   DRIVE='/media/SSD250g'
   bash "$PLUGIN/install-force-v053.sh" "$DRIVE" "$PLUGIN"
   ```
3. Follow the Force's safe plugin refresh procedure only when the current MPC session can be closed. Do not force-close MPC or reboot as part of this check.
4. Load the effect. Confirm UID `TfRA`, stereo pass-through, the version/build header, and no change to Internal Audio playback.
5. Open **SWEEP LED RESULTS**. Confirm all 31 bands and labels fit below their tops and that the Q-Link panel does not obscure them. Switch the selected measurement and smoothing; the display should remain static between changes and should not follow live RTA samples.
6. Open **SWEEP GRAPH**. Confirm the saved sweep appears on the log 20 Hz–20 kHz axis with red filled response and peak trace, readable dB/frequency markings, and the selected 1/6-octave default. Check all six smoothing choices, missing/invalid measurements, and switching among prior sessions.
7. Capture and process a sweep. Confirm specific diagnostics on failure and the expected capture/reference/impulse/raw CSV/smoothed CSV/SVG files. Confirm the capture and playback do not seize Force Internal Audio.
8. Compare original raw capture/reference/impulse/raw response file checksums before and after changing smoothing; they must remain unchanged.
9. Observe CPU/UI responsiveness with a realistic project and multiple effects. Record any firmware renderer differences; this is required before claiming Force compatibility.

Rollback command (replace paths with the exact values printed by the installer):

```sh
PLUGIN='/path/to/extracted/TROLLfx - VST - TROLLfx - VST - RTA'
DRIVE='/media/SSD250g'
BACKUP="$(cat "$DRIVE/TROLLfx-RTA-backups/LAST_BACKUP")"
bash "$PLUGIN/rollback-force-v053.sh" "$DRIVE" "$BACKUP"
```

The verification script and SDK validator cannot replace real Force UI, plugin scan, live capture, audio routing, or performance acceptance. Do not report those as passed until observed on hardware.
