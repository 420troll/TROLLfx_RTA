# Sweep Lab V0.5 — DSP, capture and native sweep workflow

V0.5 adds a portable host-side/ARMv7 offline analyzer, opt-in recording API,
and appended native VST controls for safe sweep playback. The VST UID `TfRA`
and all V0.4 parameters (0–114) remain unchanged; V0.5 adds controls at 115–121.
No sweep runs automatically. Normal live RTA behavior and Force routing remain
independent.

The existing service now has an opt-in exclusive recording API
(`POST /api/sweep/arm?seconds=N`, `GET /api/sweep/status`,
`POST /api/sweep/finish`, `GET /api/sweep/capture.wav`). It records from the
already-open UMIK stream into a bounded 30-second float buffer, pauses live FFT
updates while ARMED/CAPTURING/READY, returns one float WAV, then resumes live
analysis. A second capture is rejected, clipping count is included, and the API
refuses to arm unless ALSA capture is live. This avoids a second ALSA owner.
If a controller disappears mid-run, capture is bounded by its requested sample
limit; a completed recording is released back to live analysis after a 15-second
download window. Explicit cancel also releases READY/ERROR sessions.
Automated tests exercise missing-mic rejection, mock capture, paused live FFT,
float WAV validity, and a single ALSA open. The V0.5 native worker waits for a
live analyzer, exclusively arms recording, then marks the audio-thread sweep
buffer active only after capture reports CAPTURING. The audio callback performs
no file/network/FFT work; playback replaces both effect outputs with the same
precomputed signal, at a default -30 dBFS peak (parameter range -60 to -12 dBFS),
then resumes passthrough. CANCEL fades the sweep to passthrough over 10 ms.
Worker-side marker fitting, recording download, response/IR analysis, calibration
correction and persistent artifact writing begin after the playback buffer ends.
An invalid capture is visibly ERROR and releases capture ownership.

## Sweep and analysis

The generator implements the handoff's time-domain exponential sine sweep:

`L = T / ln(f1/f0)` and `x(t) = A sin(2π f0 L (exp(t/L) - 1))`.

It uses a linear edge fade (default 10 ms), 48 kHz, 20 Hz–20 kHz, 10 seconds,
and -30 dBFS peak. The native VST generates at the Force host rate, then uses a
32-tap windowed-sinc resampler to form the 48 kHz analysis reference expected by
the UMIK service. The playback output itself is never resampled in the callback.
Sweep stop frequency is capped to 0.45 of the host rate. Limits
are 8–192 kHz, 0.1–30 seconds, -60 to -6 dBFS and 8 million samples per input.
Generator and WAV values are finite and bounded. Mono and multichannel WAV input
is supported; channels are arithmetically averaged.

The offline analyzer resamples a capture by an explicitly supplied clock offset
in ppm, then zero pads to at least `reference length + capture length`. It
computes `H = Y conj(X) / (|X|² + λ)` with `λ = 1e-14 max(|X|²)` inside the
requested sweep band. Bins outside that band and near-zero reference bins are
excluded. The inverse FFT yields a linear, anti-wrap-padded impulse response.
Clipping at or above -0.009 dBFS (default threshold 0.999) rejects a measurement
by default. No gate or arbitrary noise suppression is applied. Memory is
bounded; the implementation reuses FFT storage for the response pass.

The generator can add two deterministic chirp markers around the sweep.
`locateClockMarkers` uses FFT correlation with local full-rate refinement to
find both markers in a captured recording, then estimates the capture/playback
clock ratio. A single marker only gives fixed delay. The locator has synthetic
drift coverage but still needs acoustic validation through speakers, room
reflections and the UMIK. Phase/group-delay results remain unverified until
that hardware check.
Absolute SPL is always marked `PROVISIONAL`; UMIK sensitivity does not establish
the Force output / ADC gain reference.

## Output and comparison

```sh
build/trollfx-sweep-cli generate-test reference.wav 48000 10 20 20000 -30
build/trollfx-sweep-cli analyze reference.wav capture.wav session-001 0 20 20000
build/trollfx-sweep-cli analyze reference.wav capture.wav session-001-calibrated 0 20 20000 \
  calibration/7033298_90deg.txt 90
python3 scripts/compare-sweep.py session-001/frequency-response.csv \
  session-002/frequency-response.csv --output comparison
```

An analyzed session preserves `reference.wav`, `capture.wav`, `impulse.wav`,
`frequency-response.csv`, `frequency-response.svg`, and `measurement.json`.
CSV stores linear magnitude, dB and phase; the SVG is a log-frequency dB graph.
When a calibration file is supplied, its inverse frequency response is applied
to the transfer spectrum and recovered impulse response. Orientation and the
2.7673 dB sensitivity factor are recorded; the factor remains metadata and is
never converted into an absolute SPL anchor. Both original calibration files
are included in the ARM DSP candidate.
The VST stores each completed session beneath `Service/sessions/` beside the
bundled service and calibration data. The comparison script can be used from a
development computer to interpolate both saved curves on 512 log-spaced points
and writes A/B SVG and difference CSV. These are local artifacts, not a runtime
graph in the MPC plugin UI.

## Numerical verification

`scripts/test-sweep-dsp.sh` compiles and runs portable C++ tests, including
44.1↔48 kHz resampler tone-amplitude checks. The synthetic
two-tap fixture has a 0.5 direct path and 0.125 reflection separated by 280
samples. Tests check peak timing, relative gain, pure-gain response, 100 ppm
two-marker drift estimation/correction, clipping rejection, bounded invalid
inputs, deterministic noise, both calibration files and sensitivity metadata,
orientation-specific response interpolation, and artifacts. `tests/test_sweep_pyfar.py` runs the same WAV through pyfar's
independent regularized `dsp.deconvolve` and compares the recovered main lobes.

pyfar is a **test-only** host dependency pinned by
`requirements-sweep-reference.txt`; it is not linked or bundled. The package is
MIT licensed and includes NumPy, SciPy, Matplotlib, sofar and other host-side
dependencies. The tested `dsp.deconvolve` function implements regularized
spectral inversion and is deprecated in favor of pyfar's
`RegularizedSpectrumInversion` API. No pyfar source is copied. The kernel itself
is original portable C++ and uses a radix-2 FFT; it adds no runtime dependency
beyond libc/libm. See [pyfar license](https://github.com/pyfar/pyfar/blob/main/LICENSE),
[pyfar dependencies](https://github.com/pyfar/pyfar/blob/main/pyproject.toml),
and [deconvolution documentation](https://pyfar.readthedocs.io/en/latest/modules/pyfar.dsp.html).

The current output matches pyfar's two-tap locations and main-lobe values
within 2% for the deterministic PCM16 fixture. Small ringing differences away
from direct/reflection peaks follow the different out-of-band regularization.
This synthetic comparison is not hardware performance or acoustic validation.

On the development host (Intel Core i7-10750H, x86_64), a 10-second 48 kHz
same-sweep recording analyzed in 1.79 seconds with peak resident memory of
74,160 KiB. This is an offline host benchmark; it is not an ARMv7 or Force
measurement. The existing live V0.2 DSP benchmark still reports separately in
`scripts/test-force-v04.sh`.

Build the full ARM plugin package with `scripts/build-force-v05.sh`. It builds
the SDK-native and ARM32/ARM64 VSTs, bundles the ARMv7 analyzer and CLI, verifies
UID/ABI/parameters/assets, runs host and package rollback tests, then writes
`dist/force-native-v05-package.tar.gz` plus its checksum. This package is for
controlled hardware installation and is not deployed automatically. Hardware
gates are listed in [SWEEP-LAB-V05-HARDWARE-ACCEPTANCE.md](SWEEP-LAB-V05-HARDWARE-ACCEPTANCE.md).

## Remaining integration gates

- Verify explicit playback and capture timing on Force through both common
  project sample rates; automated mocks verify host-rate output and passthrough
  but not analog routing or clocks.
- Verify cancellation and analyzer recovery under real audio load.
- Confirm `Service/sessions/` is writable and persistent on installed media.
- The pinned [MPC SDK Plotter documentation](https://github.com/inmusic-software/mpc-plugin-builder-sdk/blob/febd2ca5bd9eb16c4a412837196e145566bc981b/docs/ui-reference.md#616-plotter-less-tested)
  marks Plotter less tested and binds it to one parameter value; it has no
  arbitrary multi-bin series interface. The SDK host simulator exercises VST
  processing/parameters, not native skin rendering, so dynamic graph behavior
  is not verified. No dynamic graph is claimed; full plots are saved SVG/CSV.
- Build and test the integrated VST/daemon package on the Force; this branch
  makes no real Force installation or performance claim.
