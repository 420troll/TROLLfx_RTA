# TROLLfx RTA V0.5.6 evaluation package

V0.5.6 simplifies saved sweep results. The LED and graph pages use fixed
1/6-octave smoothing on the original high-resolution frequency response and
share one auto-fit dB range covering all finite measured values in 20 Hz to
20 kHz, with modest headroom. Neither display normalizes each band separately.
Result pages have no lower control strip. The SWEEP page retains START/CANCEL;
the SAVED SWEEPS page retains Latest/Previous 2–8. The old smoothing, range,
and center parameters remain in their original indices for saved-project
compatibility and are ignored by the result view.
Q-Link 1 on each result page selects the saved measurement without adding a
visible selector or control strip. The saved-measurement page retains its
own assignment as well.

The graph uses 256 logarithmically sampled response values and 255 connected
SDK Image filmstrip segments. Its compact pair atlas is 4 × 61,568 RGBA pixels
(about 0.94 MiB decoded), substantially smaller than V0.5.5's 4 × 1,065,088
atlas. The SDK specifies vertical filmstrips but does not specify Force
firmware image-size limits. Physical graph decoding/rendering remains untested.

The package preserves UID `TfRA`, 607 parameter positions, V0.5.5 live RTA and
service behavior, and saved measurement files. Selecting a saved measurement
loads its original `frequency-response.csv`; it does not rerun a sweep or
rewrite captures, impulse responses, phase, raw CSV, or metadata. New sweeps
continue to produce the existing full-resolution exports with 1/6-octave
smoothed CSV/SVG.

## Build

```bash
PYTHONPATH=/tmp/trollfx-zig-python \
MPC_SDK_DIR=/tmp/mpc-plugin-builder-sdk \
SDK_ZIG_PYTHON=/usr/bin/python3 \
PYFAR_PYTHONPATH=/tmp/trollfx-v054-pyfar-packages \
bash scripts/build-force-v056.sh
```

## Controlled install

The install script checks package hashes, the existing RTA UID/version, and
whether either RTA binary is mapped by a running process. It refuses to replace
an active plugin. Set the drive root to the mount actually confirmed on the
Force; do not change Internal Audio or restart MPC.

```bash
DRIVE='/confirmed/SSD/mount'
PLUGIN='/path/to/TROLLfx - VST - TROLLfx - VST - RTA'
bash "$PLUGIN/install-force-v056.sh" "$DRIVE" "$PLUGIN"
```

The previous RTA folder is checksummed under
`$DRIVE/TROLLfx-RTA-backups/<UTC timestamp>/`. Existing
`Service/sessions` are copied into the staged package before replacement.
Rollback uses the exact printed backup directory:

```bash
bash "$PLUGIN/rollback-force-v056.sh" "$DRIVE" \
  "$DRIVE/TROLLfx-RTA-backups/<UTC timestamp>"
```

No scanner or MPC restart is run by these scripts. On Force configurations
without MockbaMod, a plugin scanner may not be present; verify the supported
plugin-refresh path for the installed firmware. A restart is not part of the
install and requires separate approval.

## Verification status

Host DSP, saved-state, skin-schema, VST, SDK, ARM ABI, and package transaction
tests are reported in `FORCE-NATIVE-V056-VALIDATION.md`. No claim is made that
the Force firmware has rendered the filmstrip graph. On hardware, test graph
loading, all 31 LED bands/full dB limits, START/CANCEL, all saved selectors, and
audio behavior separately.
