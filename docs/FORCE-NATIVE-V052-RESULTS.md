# TROLLfx RTA V0.5.2 — Sweep Results Page

## Native rendering investigation

The pinned MPC Plugin Builder SDK describes a native `Plotter` skin component. It binds one scalar plugin parameter and can draw a line; optional history mode is for scrolling traces. The SDK does not document a dynamic multi-point array, an SVG widget, an image refresh API, or a custom VST editor surface. The plugin therefore does not try to load an exported SVG into the skin or replace a PNG at runtime.

The V0.5.2 results page uses the documented static `Plotter` component in non-history mode. It places 31 fixed frequency tiles across a logarithmic 20 Hz–20 kHz axis. Each tile has a raw and a smoothed parameter-bound line, forming two step curves; both use the chosen saved capture's response values relative to its 1 kHz level. The 31 red LED stacks below the graph use the selected smoothed response. These parameter values change only after a capture completes, a saved capture is selected, or the smoothing choice changes. No live microphone spectrum is read by the results display.

The chart range is −30 to +6 dB relative to 1 kHz. Values outside the capture's valid measured bandwidth are placed at the display floor; the saved SVG and CSV retain the full-resolution valid data. The graph uses the logarithmic centers of the existing 31 bands. Labels include 20 Hz through 20 kHz major ticks and band centers beneath the LED stacks. The page exposes the existing smoothing selector (No Smoothing, 1/48, 1/24, 1/12, 1/6 and 1/3 octave) and saved-measurement selector (Latest, Previous 2–8).

The header identifies `V0.5.2 · BUILD 502`. The native graph is a bounded 31-step overview; exported SVG/CSV remain the detailed measurement. The SDK marks Plotter as less-tested. JSON/skin validation and the plugin host tests verify widget types, static bindings, parameter mapping and update behavior, but only the Force touchscreen can validate the firmware renderer and final visual legibility. No Force-host rendering result is claimed here.

## Compatibility and data handling

- VST UID remains `TfRA`; stereo pass-through and all V0.5.1 parameter indices 0–154 retain their meaning. V0.5.2 parameters are appended at 155–217.
- The graph and LED outputs are derived from `frequency-response.csv` for the selected session and selected smoothing mode.
- Raw captures, reference sweeps, impulse WAVs, phase data and original unsmoothed `frequency-response.csv` are not rewritten by the display conversion. Fractional smoothing reprocessing retains the existing V0.5.1 reversible-export behavior.
- Missing or invalid sessions clear the curve and LED parameters and show `NO MEASUREMENT` or `INVALID` status; they do not leave an earlier capture presented as current.
- No changes are made to the UMIK capture service, microphone ownership, Force audio routing or V0.4 live RTA processing.

## SDK evidence and limitations

The implementation uses the SDK skin API documented in `docs/ui-reference.md` sections 3, 4, 5 and 6.16 (`Plotter`, `drawHistory: false`, `dataHandle`, `dataMin`, `dataMax`, `lineDirection`). It does not use undocumented callbacks, a fake SVG component, or a VST editor API. The skin validator and generated TUI are included in the automated build checks. Hardware acceptance must confirm that firmware 5.0.18 renders the 62 tiled Plotter components smoothly and that the trace and 31 LED labels remain readable at normal viewing distance.

Build/package identity is version 0.5.2 and VST `PluginInfo.version=502`; the skin header shows the same build number. The package is a test build, not a claim of Force visual acceptance.
