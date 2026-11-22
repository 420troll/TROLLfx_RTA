# V1.0 multi-layer renderer evaluation

The SDK's documented `Image` + `FilmStrip` component binds one frame selector
parameter to one image. Its `colour` field is a background fill, not an image
tint. The prior `Plotter` component did not render on the tested Force setup,
and the SDK documents no dynamic-image path. V1.0 therefore uses eight
pre-colored film-strip images and the documented parameter-bound Image
components.

Each layer reuses the same 962-frame (31 × 31 endpoint pairs + transparent
missing frame) geometry and 255 connected x-segments. This produces:

- 2,040 graph Image components (8 × 255), not 16,640-frame per-color atlases.
- 8 decoded atlas images, each 4 × 61,568 RGBA pixels (985,088 bytes, about
  0.94 MiB); 7.5 MiB total decoded pixel storage if
  the host retains all eight simultaneously.
- 2,040 appended integer frame parameters, 8 appended persistent layer toggles,
  and 5 shared graph-axis tick parameters. Total plugin parameter count: 2,660.
- At most 2,040 frame parameter writes when a layer mask or new measurement
  changes; saved-measurement selection alone refreshes only the primary LED
  view. No graph work occurs per audio block.

The frame transform is bounded to 8 × 256 output points plus fractional-octave
smoothing of each selected CSV. This is computationally small compared with
loading the assets and rendering thousands of UI components. The local SDK
validator/build and host state tests can verify schema and parameter binding,
but cannot establish firmware image-cache behavior, touchscreen frame time,
memory retention, or audio dropouts. Those remain Force hardware acceptance
items; V1.0 must not be called hardware-complete until they pass.

All curves are independently referenced to each measurement's smoothed 1 kHz
response, then share one auto-fit graph dB range derived from the union of all
enabled valid response values. The LED page remains a single selected sweep.
