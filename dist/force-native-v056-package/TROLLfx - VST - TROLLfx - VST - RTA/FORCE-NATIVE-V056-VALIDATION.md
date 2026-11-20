# TROLLfx RTA V0.5.6 validation

## Design and compatibility

- UID remains `TfRA`; the 607 VST parameter slots retain their V0.5.5 order
  and names. Parameters 122, 250, and 251 remain hidden from the skin and no
  longer affect result processing.
- Saved-session display processing reads raw `frequency-response.csv` and
  applies the existing logarithmic fractional-octave magnitude smoothing at
  fixed N=6. It computes one 1 kHz-relative range from the complete finite
  smoothed response over 20 Hz–20 kHz. The range receives a minimum 1 dB or 5%
  of span headroom on each side, whichever is larger. LED and graph mapping
  share these limits; stored measurement values are not clipped or modified.
- The graph uses 256 log-spaced points and maps each adjacent pair to one of
  255 connected red line segments. The filmstrip contains 31×31 endpoint
  combinations and one transparent missing-data frame: 962 frames total, in
  a 4×61,568 RGBA PNG. Vertical endpoint quantization is 31 levels. It is a
  continuous connected polyline, but the 31-level vertical quantization and
  firmware image decoding must be judged on the Force.
- The pinned MPC SDK documents Image filmstrips as vertically stacked and the
  skin validator accepts these bindings. It does not document maximum image
  dimensions or prove the Force runtime renderer. The reduced atlas is not
  claimed to work until physical firmware testing.
- `Q-Links.json` assigns Q-Link 1 to the existing saved-measurement parameter
  123 on both result pages. This configuration is included in strict SDK
  validation; actual Q-Link hardware interaction still needs Force testing.
- Live RTA, capture/service, sweep playback, file formats, and stereo
  pass-through are unchanged. Saved measurements load from raw CSV; there is
  no display-triggered reprocessing or rewrite of prior session data.

## Automated checks

The V0.5.6 build runs the SDK validator, ARM32/ARM64 build, VST host tests,
V0.2–V0.5 DSP/service regressions, sweep host playback/cancel tests, smoothing
reference tests, skin checks, and install/rollback simulation. A release report
must record actual command output and distinguish host checks from Force
verification.

The focused skin test checks 31 unclipped band bounds, five auto-range dB
ticks, no result control components, no stale smoothing/range Q-Link binding,
eight saved-session buttons, 255 graph line widgets, and the 962-frame atlas.
The C++ sweep display test checks fixed N=6, the full range calculation,
common band/graph mapping, flat response, peaks, and no mutation to the source
phase/magnitude arrays. Host tests keep parameter 122 state compatibility and
verify it cannot change result values.

## Hardware acceptance checklist

1. Keep Force playback on **Internal Audio** and confirm no VST binary is
   mapped before installation. Do not restart MPC during this test.
2. Confirm the all-31-band view fits without clipping and that automatic dB
   limits include the low and high valid response values with visible headroom.
3. Confirm START SWEEP and CANCEL / FADE work once per tap.
4. Confirm saved sweep buttons load the corresponding previous sessions and
   do not modify source WAV/CSV/metadata files.
5. Open SWEEP GRAPH. Record whether the 4×61,568 PNG decodes, whether the
   connected red curve is visible, and page-open time/UI responsiveness. If it
   fails, record the firmware symptom; do not substitute meter columns.
6. Confirm saved-measurement switching updates both result pages while idle.
7. Test normal playback and recording in a representative project with the
   analyzer service available; check for audio dropouts and UI stalls.
8. If MPC needs plugin refresh/restart to load the replaced binary, stop and
   obtain approval before restarting.
