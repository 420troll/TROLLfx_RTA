# TROLLfx RTA V1.0.0 — Akai Force quick manual

Keep Force playback on **Internal Audio**. The plugin is a transparent stereo
effect except while START SWEEP intentionally emits the configured test sweep.
Sweep capture, CSV parsing, 1/6-octave smoothing, and graph-frame updates run on
the plugin's background worker, not in the audio callback.

## SWEEP

Tap **START SWEEP** to play the configured logarithmic sweep and record the
UMIK-1 through the separate analyzer service. The status moves through
STARTING, ARMED, CAPTURING, PROCESSING, then READY or ERROR. **CANCEL / FADE**
fades the generated signal before returning to normal pass-through. Set the
sweep level conservatively and stop other playback from reaching the analyzer
input during a measurement.

## SAVED SWEEPS

The eight buttons independently show or hide Latest and Previous 2–8 on the
graph. The order is automatic and newest first; these are not named save slots.
The button artwork uses red, blue, green, amber, purple, cyan, orange, and white
in that order. Empty or invalid positions draw no curve. The Q-Links select
layer toggles on this page. The primary sweep shown by SWEEP LED RESULTS is
chosen with Q-Link 1 on either results page, or Q-Link 5 on the SWEEP page.

## SWEEP GRAPH

Displays up to eight connected 256-sample logarithmic curves. Every visible
measurement is referenced to its own smoothed 1 kHz value and uses a shared
auto-fit dB axis spanning all valid visible curves. The graph is **relative
frequency response**, not calibrated dB SPL. Fixed 1/6-octave smoothing is
applied to raw saved response data. The small legend gives each layer's color.

## SWEEP LED RESULTS

Shows the selected primary measurement as 31 red bands with 1/6-octave
smoothing and automatic range. It intentionally shows only one measurement;
multi-measurement comparison belongs on SWEEP GRAPH.

## Saving and compatibility

Completed sweep files remain in `Service/sessions`. V1.0 adds the layer-mask
and graph-frame parameters after all V0.5.6 parameters; UID `TfRA` and every
existing parameter index keep their previous meaning. Old projects default to
Latest enabled and all other overlays disabled.

## Hardware acceptance status

The native SDK skin validator and ARM builds are package checks, not proof of
Force rendering performance. Before a performance, verify all eight curves,
touch/Q-Link toggles, an empty slot, graph readability, and sweep capture on the
target firmware. Do not replace a loaded plugin or restart MPC during an active
project.
