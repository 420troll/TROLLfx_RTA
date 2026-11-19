# V0.5.4 controlled Force acceptance

This test package is not auto-installed. Preserve Internal Audio as the playback device. Do not install while the RTA plugin is loaded in a project or restart/rescan MPC without first closing the active project in the normal UI.

## Verify and install

On the development host verify the archive:

```sh
cd dist
sha256sum -c force-native-v054-package.tar.gz.sha256
```

Extract the archive. On Force, confirm the actual SSD mount and existing folder. Keep `DRIVE` set to the confirmed mount root and `PLUGIN` to the extracted plugin folder containing `plugin.json`, `VST/`, `Service/`, `Plugin Skins/`, and the three V0.5.4 scripts:

```sh
PACKAGE='dist/force-native-v054-package.tar.gz'
scp "$PACKAGE" akai-force:/tmp/force-native-v054-package.tar.gz
ssh akai-force 'mkdir -p /tmp/trollfx-rta-v054-evaluation && tar -xzf /tmp/force-native-v054-package.tar.gz -C /tmp/trollfx-rta-v054-evaluation && find "/media" -maxdepth 3 -type d -name "VST2 Plugins" -print'
```

Set `DRIVE` to the actual mount root printed by `find`, and use the extracted plugin folder. The installer verifies files, backs up the current RTA to `TROLLfx-RTA-backups/<UTC timestamp>`, preserves the prior `Service/sessions` contents, and replaces only the exact TROLLfx RTA directory. It does not change `MPC.settings`, modify other plugins, rescan, restart MPC, or reboot.

```sh
PLUGIN='/tmp/trollfx-rta-v054-evaluation/force-native-v054-package/TROLLfx - VST - TROLLfx - VST - RTA'
DRIVE='/media/CONFIRMED_DRIVE'
ssh akai-force "bash '$PLUGIN/install-force-v054.sh' '$DRIVE' '$PLUGIN'"
```

## Touchscreen checks

1. Confirm V0.5.4, UID `TfRA`, Internal Audio playback, and transparent stereo pass-through.
2. Open SWEEP LED RESULTS and verify all 31 columns/labels fit with the Q-Link overlay visible. Load a saved flat response, then a peaked/notched saved response. Verify the default 48 dB window, 1 kHz reference, +12/−36 dB labels, and that a −6 dB change is three LED segments.
3. Change 24/36/48/60/72 dB spans and move Display Center. Verify the same measurement changes only its displayed scale, not its saved files or numerical dB outputs. Confirm above/below/no-data markers.
4. Open SWEEP GRAPH. Confirm a connected curve rather than meter columns, log frequency labels, dB ticks matching the displayed window, red line and subtle fill. Check all six smoothing modes and previous measurements. The graph should remain static until a saved result or view setting changes.
5. Try an older high-frame-count or partial-band session. Confirm no crash, no invented response outside bandwidth, and a specific error for corrupt/missing data.
6. Measure page open/update latency, CPU/memory while other plugins are active, touchscreen responsiveness, and audio dropouts. Record firmware/build ID and whether clipping, seams, or image decode limitations occur.
7. Hash original capture, reference, impulse, raw response, phase/calibration metadata before/after view changes; they must not change.
8. Confirm analyzer/microphone ownership and keep Force playback on Internal Audio throughout.

A successful ARM build or Pillow/SDK validation does not satisfy these hardware checks.

## Rollback

Use the exact backup path printed by the installer:

```sh
PLUGIN='/tmp/trollfx-rta-v054-evaluation/force-native-v054-package/TROLLfx - VST - TROLLfx - VST - RTA'
DRIVE='/media/CONFIRMED_DRIVE'
BACKUP='/media/CONFIRMED_DRIVE/TROLLfx-RTA-backups/YYYYMMDDTHHMMSSZ'
ssh akai-force "bash '$PLUGIN/rollback-force-v054.sh' '$DRIVE' '$BACKUP'"
```

Use paths accessible to the remote shell (for example `/tmp/trollfx-rta-v054-evaluation/rollback-force-v054.sh`), not the development host path. Rollback retains the failed V0.5.4 directory under `.trollfx-rta-v054-failed-*` and restores the backed-up plugin folder. No deployment/restart was performed to prepare this package.
