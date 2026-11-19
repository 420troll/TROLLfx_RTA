# V0.5.2 Force touchscreen acceptance

Do not install while the RTA library is loaded by MPC. Keep Force playback set to **Internal Audio**. Use the V0.5.2 installer only after its package verification and backup checks pass; no deployment is performed by the build.

1. Verify the package SHA256, then install V0.5.2 with `scripts/install-force-v052.sh` and keep the reported V0.5.1 backup path.
2. Use the Force's existing plugin-refresh procedure only when it is safe to close MPC; do not interrupt an active project. Confirm the RTA effect loads with UID `TfRA`, stereo pass-through remains transparent, and the plugin reports version/build `V0.5.2 · BUILD 502`.
3. Open **SWEEP RESULTS** with no stored sweep: status should read `NO MEASUREMENT`; curve and LEDs should be clear.
4. Select a prior saved measurement. Confirm the graph is static and uses that capture, not the live RTA. Compare its raw/dim and smoothed/red traces with its exported `frequency-response.csv` and `frequency-response-smoothed.csv`.
5. Change through No Smoothing, 1/48, 1/24, 1/12, 1/6 and 1/3 octave. Confirm the red curve/LEDs update and the raw trace remains stable; confirm no test tone or sweep playback starts and the capture WAV/IR/raw CSV are unchanged.
6. Select Previous 2 and a later measurement. Confirm the selected curves change to that measurement and remain fixed when live sound changes.
7. Load a missing/corrupt measurement case. Confirm the page reports `NO MEASUREMENT` or `INVALID`, and does not continue showing an earlier curve.
8. Observe scrolling/redraw cost with the LED page and several other plugins. Confirm frequency labels (including 20 Hz and 20 kHz) and dB grid remain readable with the Q-Link panel open.
9. Confirm live RTA still captures through the UMIK-1, the analyzer service can start as configured, and Force output remains Internal Audio.
10. If any graph component is blank or poorly rendered, switch acceptance to the 31-band LED overview and retain the SVG/CSV export path; do not claim native graph success until fixed and retested.

The SDK identifies `Plotter` as less-tested. Host tests validate skin schema, parameter bindings, data selection and offline DSP, not the Akai firmware renderer. Touchscreen visibility, graph rendering performance, actual sweep measurement, UMIK behavior and audio pass-through require this hardware test.
