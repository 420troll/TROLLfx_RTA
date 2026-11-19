# TROLLfx RTA V0.5.4 — sweep response scaling and native graph

V0.5.4 preserves V0.5.3 live RTA, capture, sweep, session, and export code. It changes only the selected saved-measurement view transform and result skin, plus appended non-automatable output parameters.

## Shared 31-band level scale

The raw response CSV remains the source of truth. The selected response is smoothed at its original high-resolution frequency coordinates using the selected fractional-octave mode. The smoothed magnitude response is interpolated at 1 kHz and each band is reported as `20 log10(magnitude_band / magnitude_1k)`. No per-band normalization or live-RTA display gain is applied.

The default display window is +12 dB to −36 dB relative to the smoothed measurement at 1 kHz. Its midpoint is −12 dB and span is 48 dB. Each band uses the same mapping:

`segments = clamp(24 * (level_dB - window_low_dB) / window_span_dB, 0, 24)`

Thus 1 kHz is 18 segments, −6 dB is 15, −24 dB is 6, the lower limit is zero, and the upper limit is 24. The selectable spans are 24, 36, 48, 60, and 72 dB. Display Center moves the window without editing response values. Above-window bands get a bright upper chevron; below-window bands get a lower chevron; frequencies outside the measured bandwidth get a no-data mark. The unclamped relative values are retained in appended read-only output parameters and in the original high-resolution CSV files.

The dynamic tick readouts and window endpoint readouts follow the selected span/center. The LED page uses the same dB values and reference as the graph page. Its 31 stacks, frequency labels, red segments, and clip/no-data indicators fit in the chart area, with controls below it.

## Connected high-resolution graph

The graph samples the smoothed high-resolution curve at 256 logarithmically spaced frequencies over 20 Hz–20 kHz. Out-of-band samples become transparent gaps. It does not derive the graph from the 31 overview values. Each adjoining pair is drawn as a connected red line segment with low-alpha shading underneath. Vertical positions are represented at 129 levels across the selected dB window (about 0.375 dB per step for the default 48 dB window). The rendered graph therefore uses 256 frequency samples / 255 connected segments, with the pixel-width horizontal resolution limited by the display and the quantized vertical position limited by the film-strip frame atlas.

Smoothing is applied before the 256 graph samples are selected. All six choices remain: Off, 1/48, 1/24, 1/12, 1/6 (default), and 1/3 octave. Changing smoothing reprocesses the chosen saved measurement; changing display range/center only changes the native view parameters. It does not play a sweep, record, or modify the original capture/reference/impulse/raw response data. Full-resolution CSV/SVG exports remain intact.

## Force UI investigation and limits

The pinned MPC Plugin Builder SDK describes `Plotter` and `Path` as less tested. A Plotter is a value/history display, not an array-backed XY graph. Path takes a static path string and can scale it by a parameter; it has no documented dynamic path-data binding. The SDK documents parameter-bound vertical Image film strips as a well-trodden component. V0.5.3's regular image film strips already render the saved sweep's LED frames on the Force.

V0.5.4 uses that documented Image mechanism for the curve: a shared 4 × 1,065,088 PNG frame strip represents all 129 × 129 adjacent y-position pairs plus a transparent missing-data frame. 255 Image components select frames using appended integer parameters. This is a supported SDK component type and a bounded static asset; it does not add undocumented image upload APIs, redraw callbacks, browser use, or audio-thread work. The strip decodes to about 17.0 MB RGBA and 255 graph segment components redraw only after saved-measurement, smoothing, range, or center changes.

The source asset and a Pillow rendering prototype are validated locally and the skin is checked by the pinned SDK validator. The SDK host simulator does not draw touchscreen skins. The new high-frame-count film-strip graph has **not** been loaded on Force firmware in this task; its image-decoder limit, actual touch-page latency, and visual continuity are still hardware acceptance items. Do not treat the simulations as Force screenshots.

## Parameters and compatibility

VST UID remains `TfRA`. Parameters 0–249 and their saved-state meanings are unchanged. V0.5.4 appends:

- 250: Display Range; 251: Display Center.
- 252–253: current low/high window readouts.
- 254–258: dynamic dB tick values.
- 259–513: 255 connected-curve frame indices (0–16641; 16641 is the transparent missing-data frame).
- 514–544: 31 clip/no-data states.
- 545–575: unclamped smoothed relative dB at the 31 overview centers.
- 576–606: unclamped raw relative dB at the 31 overview centers.

Total: 607 parameters. The existing live RTA band meter gain, floor, attack, release, peak hold/fall, and service behavior are unchanged. Previous 155- and 218-parameter saved-state shapes continue loading with appended controls at defaults.

Absolute SPL remains provisional without an independently verified acoustic reference. Sweep graph dB is relative magnitude, not dB SPL.

## Validation assets

- `docs/images/v054-sweep-led-layout-simulation.png`
- `docs/images/v054-sweep-graph-layout-simulation.png`

Both images are Pillow simulations of the page layout and supported frame assets. Neither is from MPC or Akai Force firmware.
