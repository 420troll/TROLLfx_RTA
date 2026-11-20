# TROLLfx Sweep Lab V0.5.1 — response smoothing

V0.5.1 adds fractional-octave smoothing as an offline visualization and export transform. It does not change V0.4 live RTA processing, sweep playback, capture, impulse recovery, clock alignment, phase, or calibration.

## Algorithm

For each measured center frequency, the full smoothing band is `fc * 2^(-1/(2N))` through `fc * 2^(+1/(2N))`, where N is 3, 6, 12, 24, or 48. Magnitude is integrated with trapezoids over log-frequency, using actual measurement coordinates and linear interpolation between adjacent samples in log-frequency. At the measured edges the interval is clipped and divided by its remaining log-width. This avoids fixed FFT-bin windows, dB averaging, and edge roll-off. `No Smoothing` copies input magnitudes exactly. The method is similar in intent to REW fractional-octave smoothing; bit-for-bit equivalence is not claimed.

Selectable modes are No Smoothing, 1/48, 1/24, 1/12, 1/6 (default), and 1/3 octave. The raw response is kept in `frequency-response.csv`; `frequency-response-smoothed.csv` contains the selected magnitude transformation and the unchanged phase samples. Reprocessing an existing session only updates the SVG, smoothed CSV, and smoothing fields in `measurement.json`; raw capture/reference/impulse WAVs and original response data remain untouched.

The SVG plots response shape relative to the smoothed 1 kHz magnitude and explicitly prints the absolute 1 kHz level used as reference. It includes a fine raw trace behind the selected smoothed curve. The logarithmic frequency grid spans the valid measured range clipped to 20 Hz–20 kHz. Frequency labels are high-contrast, bold, and placed below the graph to remain readable.

## Force UI constraints

The pinned MPC Plugin Builder skin format exposes parameter-bound image strips, sliders, and labels, but no supported arbitrary response-series graph or dynamically sourced SVG image. The `SWEEP RESULTS` page therefore renders a bounded 31-column response using appended read-only LED parameters. It is an overview; detailed inspection remains in the full-resolution SVG/CSV. Smoothing and saved-measurement selectors are appended after all V0.5 indices (122 and 123); read-only overview values use 124–154. The existing UID `TfRA` and parameter indices 0–121 remain unchanged. These choices trigger worker-thread session reprocessing only and never start capture or sweep playback.

At plugin startup the analyzer binary is copied by the background worker to a unique executable file under `/tmp` before `posix_spawn`. This supports Force removable drives mounted `noexec` (including exFAT) while the calibration files and saved sessions remain beside the installed plugin. The temporary copy is removed after the owned analyzer exits; it does not change ALSA or Force playback routing.

## CLI

`Service/trollfx-sweep-cli-armv7 reprocess SESSION [none|1/48|1/24|1/12|1/6|1/3] [reference_hz]`

The default is 1/6 octave and 1000 Hz. The command works with V0.5 sessions that contain the original `frequency-response.csv`; new metadata fields are added while unrelated existing JSON fields are retained.

## Testing and hardware acceptance

The host suite covers flat response, progressively attenuated narrow peaks, boundary normalization, raw-copy mode, legacy session reprocessing, metadata replacement, and preservation of raw CSV and impulse WAV. CLI tests exercise every choice and parse metadata. Existing RTA, V0.5 playback, saved-state, and pass-through suites remain required. The SDK validator checks the new result page and parameter wiring.

No Force installation is performed by the build. On hardware, confirm the plugin scans and loads, all six choices display, smoothing re-renders the latest session without playback/capture, the 31-column overview and bottom labels are legible at the Force viewing distance, and the SVG/CSV export matches the shown choice. Continue to keep Force playback on Internal Audio. The native page is a coarse 31-band overview; it is not a substitute for checking the exported response graph.
