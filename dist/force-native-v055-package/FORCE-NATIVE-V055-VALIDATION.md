# TROLLfx RTA V0.5.5 hardware UI repair validation

## Scope

V0.5.5 adds native touchscreen controls around the existing sweep workflow. It
preserves plugin UID `TfRA`, all 607 existing parameter positions, automatic
session saving, the 31-band and 16-band live RTA pages, saved sweep data, and
the V0.5.4 result calculations. The plugin reports build 505. No Force install
or MPC restart is part of this package.

## Button behavior and saved-session mapping

The pinned MPC Plugin Builder SDK revision used here is
`febd2ca5bd9eb16c4a412837196e145566bc981b`. Its UI reference documents a
parameter-bound `Button` as either a toggle (`numButtonsInGroup: 1`, lit step
`buttonId: 1`) or a radio group whose N buttons bind one choice parameter with
IDs 0 through N−1. The button images are native-size, so the generated image
dimensions match their touch bounds. See the SDK's
[Button UI reference](https://github.com/inmusic-software/mpc-plugin-builder-sdk/blob/main/docs/ui-reference.md).

- **START SWEEP**: button ID 1 on boolean parameter 115.
- **CANCEL / FADE**: button ID 1 on boolean parameter 116.
- **SAVED SWEEPS**: eight radio buttons on existing choice parameter 123:
  0 = Latest, 1 = Previous 2, …, 7 = Previous 8.
- The SWEEP and result-page Q-Link/knob controls remain available.
- A tap sets the bool parameter to one. `parameterChanged` immediately sets it
  back to zero; the zero callback does not meet the action threshold and does
  not enqueue a second action. The audio callback only observes the cancel
  request atomically. CANCEL now fades the held last sweep sample to exactly
  zero over 480 samples (10 ms at 48 kHz), spanning host blocks, then returns
  to ordinary stereo pass-through.

Saved sweeps remain automatically stored in `Service/sessions`; the eight
buttons select the newest eight available session folders. They do not create
named save slots. The choices reprocess/reload the chosen existing measurement
and never arm playback or capture a new sweep.

The SDK's host simulator tests VST parameters and audio but does not draw MPC
touchscreen skins. The added skin test checks component type, handles, group
IDs, images, bounds, and stable parameter numbers. The VST host tests invoke
parameters directly, so they establish the plugin action behavior but do not
prove that a Force touchscreen tap is dispatched as expected.

## Graph asset constraint assessment

The SDK describes a vertical image filmstrip and requires the strip height to
divide evenly by its frame count. The pinned validator checks component schema,
asset presence, and parameter bindings. Neither its UI reference nor validator
documents a maximum PNG width/height, maximum filmstrip frame count, image
decoder memory limit, GPU texture limit, or Force skin-renderer timing limit.
The SDK host simulator does not render the skin. Consequently, the SDK build
and validator cannot establish that the Force firmware can decode or draw the
V0.5.4 graph atlas.

The current atlas is 4 × 1,065,088 pixels RGBA with 16,642 frames. At four
bytes per decoded pixel that is 17,041,408 bytes (about 16.25 MiB) before any
renderer-side copies or caches. Its compressed PNG size is much smaller; that
does not reduce decoded texture/image memory. There is no verified Force
decoder limit available in the SDK contract, so this asset remains a hardware
compatibility risk. It must not be described as Force-verified. Keep the
three-segment firmware prototype as the first controlled device test; only
proceed to the full 255-segment page if the image loads and the Force remains
responsive. No plugin install or MPC restart was performed here.

The full graph continues to use the 256 logarithmic response samples and the
selected saved measurement's high-resolution smoothed curve. This handoff does
not replace it with 31 bars. Reducing atlas dimensions may require a deliberate
trade-off in vertical quantization or a different verified rendering path.

## Validation run

The full build was run from the V0.5.5 branch with the pinned SDK
`febd2ca5bd9eb16c4a412837196e145566bc981b`, Zig 0.17.1, and pyfar 0.8.1.
The SDK built both ARM targets and accepted the skin with zero errors and
warnings. The full V0.2/V0.3/V0.4/V0.5 regression scripts, pyfar reference,
eight-session selection test, CANCEL ramp test, and package install/rollback
simulation passed. These are host/SDK results; they do not render the skin on
Force firmware.

Reproduce the package build with:

```bash
PYTHONPATH=/tmp/trollfx-zig-python \
MPC_SDK_DIR=/tmp/mpc-plugin-builder-sdk \
SDK_ZIG_PYTHON=/usr/bin/python3 \
PYFAR_PYTHONPATH=/tmp/trollfx-v054-pyfar-packages \
bash scripts/build-force-v055.sh
```

The save-selector host test uses distinct deterministic response levels in
eight old session folders, verifies the expected response values for Previous
2 through Previous 8, restores Latest, and confirms these changes do not arm
another capture. The cancel test verifies the 480-sample ramp across four
128-frame audio callbacks, button reset, one API cancel, and restored
pass-through.

## Force acceptance still required

1. Install only after confirming the target plugin directory and backing up
   V0.5.4; keep Force playback on Internal Audio.
2. Verify button press/release states for START and CANCEL on the touchscreen.
3. Confirm CANCEL has a clean 10 ms fade and no repeated API cancel.
4. Verify Latest/Previous 2–8 select the matching saved measurements and do
   not start playback or recording.
5. Test the three-segment graph prototype first. Record firmware, decoder
   result, page-open time, UI responsiveness, memory/CPU, and audio behavior.
6. Only after that succeeds, test all 255 graph segments, smoothing changes,
   result controls, the full-screen LED page, and realistic project load.

Do not reboot the Force, change global audio settings, or restart MPC as part
of this acceptance procedure.
