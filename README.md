# TROLLfx RTA

Experimental Akai Force ARMv7 analyzer for the miniDSP UMIK-1. V0.1.2 remains on `main`; V0.2 work is on `feature/v02-measurement-engine`.

## V0.2 status

- Keeps the independent UMIK ALSA capture path: 48 kHz, stereo S24_3LE, explicit `snd_pcm_start()`, analyzed channel 0.
- 32,768-point Hann-window FFT, power-integrated one-third-octave bands, 4,096-sample hop, with 31-band and 16-band views.
- Includes the serial 7033298 0° and 90° calibration files. Select orientation at launch with `--orientation 0` or `--orientation 90`.
- Frequency-response correction and A/Z weighted computations use the selected curve. The calibration file's `Sens Factor = 2.7673 dB` is parsed and reported.
- Raw input dBFS is retained. SPL stays provisional until the Force ALSA/USB input sensitivity is checked against a known acoustic calibrator. The file's sensitivity factor alone is not used to claim calibrated SPL.
- Red segmented LED stacks, with defaults: +11 dB display gain, 50 ms attack, 33 dB/s LED fall, 200 ms peak hold, and 60 dB/s peak fall. Display floor and smoothing do not change acoustic measurements.

Keep the Force on Internal Audio. The analyzer does not change Force audio settings. The browser stays bound to 127.0.0.1:8096 and can be reached through SSH forwarding. No boot service is installed.

## Build, tests, and manual installation

On Ubuntu/WSL install `gcc-arm-linux-gnueabihf`, then run:

```bash
scripts/run-tests.sh
scripts/build-armv7.sh
```

The second command creates `build/trollfx-rta-v02-armv7` and the distributable `dist/trollfx-rta-v02-armv7`. Cross-compilation is not a hardware performance test.

Manual test install (requires the existing `akai-force` SSH alias):

```bash
scripts/install-test-v02.sh
ssh -N -L 8096:127.0.0.1:8096 akai-force
```

Open http://127.0.0.1:8096. Select the mic orientation with `AKAI_ORIENTATION=90 scripts/install-test-v02.sh` when the mic points upward. Roll back with `scripts/rollback-v02.sh`; the V0.1.2 executable is retained at `/tmp/trollfx-rta-v012`.

For calibrated SPL, first use a traceable 94 dB SPL 1 kHz calibrator on the UMIK-1 and measure the RMS dBFS from the exact Force capture path. Then run the app with `--sensitivity-dbfs-at-94 <measured dBFS>`. The reported equation is `SPL = 94 + corrected dBFS - measured dBFS at 94 SPL`. Do not use the display gain in this measurement. Without that hardware measurement, SPL fields are null and marked provisional.

## Files

- `src/trollfx_rta.c` — unchanged ALSA acquisition contract, HTTP API and red LED interface
- `src/dsp.c`, `src/dsp.h` — FFT, band power, weighting, calibration parser/interpolation and SPL conversion
- `calibration/` — individual UMIK 0° and 90° files
- `tests/` — deterministic DSP checks and V0.1.2 host benchmark
- `docs/V0.2-DSP.md` — design, source evaluation, verification limits and sensitivity measurement procedure
- `scripts/` — ARM build, tests, manual test install and rollback

PFFFT is bundled under its license in `src/third_party/pffft/`; no ProdMesh or Python soundlevelmeter source is copied. The project-wide license remains to be chosen by maintainers.

## V0.3 native plugin branch

`feature/force-native-rta-v03` builds the browser-independent MPC/Force VST2 display effect while keeping V0.2 as the separate UMIK capture daemon. It has an ARMv7 hard-float binary, an ARM64 binary, parameter-bound 31/16-band LED skins, a bounded shared localhost client, test and installation scripts. See [the V0.3 design/build guide](docs/FORCE-NATIVE-V03.md) and [automated build/test results](docs/FORCE-NATIVE-V03-TEST-RESULTS.md). This is experimental until the Force host, touch UI, audio playback, and acoustic SPL reference checks pass on hardware.

The build uses the pinned external InMusic MPC plugin SDK and Zig 0.17.1:

```bash
MPC_SDK_DIR=/path/to/mpc-plugin-builder-sdk \
SDK_ZIG_PYTHON=/path/to/python PYTHONPATH=/path/to/ziglang \
bash scripts/build-force-v03.sh
```

Build output and installer scripts are placed in `dist/force-native-v03-package/`. The build does not install to a Force.
