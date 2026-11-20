# TROLLfx RTA V0.5.5 evaluation package

V0.5.5 adds touchscreen START SWEEP, CANCEL / FADE, and a dedicated SAVED
SWEEPS selector page. Sweep captures continue saving automatically. The
selectors choose Latest or one of the seven preceding sweep sessions.

The package preserves the V0.5.4 result pages, measurement engine, saved data,
plugin UID `TfRA`, and all parameter indices. It includes ARM32 hard-float and
ARM64 VST binaries, the ARMv7 analyzer service, both UMIK-1 orientation
calibration files, license notices, generated skin art, and integrity checks.

## Package verification

On the development host, verify the archive and extracted plugin folder:

```bash
cd dist
sha256sum -c force-native-v055-package.tar.gz.sha256
tar -xzf force-native-v055-package.tar.gz
bash force-native-v055-package/verify-force-v055.sh \
  "force-native-v055-package/TROLLfx - VST - TROLLfx - VST - RTA"
```

## Controlled installation

Extract the package to a path accessible to the Force shell. Confirm the
actual SSD/USB mount root and that the existing RTA folder is the plugin being
replaced. Set `DRIVE` to that confirmed mount and `PLUGIN` to the extracted
folder that contains `plugin.json`, then run:

```bash
DRIVE='/confirmed/mount/root'
PLUGIN='/path/to/TROLLfx - VST - TROLLfx - VST - RTA'
bash "$PLUGIN/install-force-v055.sh" "$DRIVE" "$PLUGIN"
```

The installer verifies the package, stages a new directory, preserves
`Service/sessions`, backs up the previous RTA folder and checksums it before
replacement. It does not touch other plugins or `MPC.settings`. Do not run it
while the plugin is loaded or a project is active. Preserve Internal Audio
routing. Do not reboot or restart MPC as part of the initial test.

The installer prints the backup path and rollback command. The rollback form
is:

```bash
bash "$PLUGIN/rollback-force-v055.sh" "$DRIVE" '/drive/TROLLfx-RTA-backups/<timestamp>'
```

## Hardware status

The SDK schema/build and native VST host tests pass. The Force touchscreen has
not been used to verify the new buttons or saved-session radio group. The
existing graph asset remains especially unverified: first test a reduced
three-segment decoder probe, then test the full graph only after confirming
image loading and UI responsiveness. Refer to `FORCE-NATIVE-V055-VALIDATION.md`
for exact acceptance steps and limitations.
