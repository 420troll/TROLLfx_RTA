# TROLLfx RTA

Experimental **Akai Force ARMv7** real-time analyzer for the **miniDSP UMIK-1**. The working prototype displays 31 red LED-style 1/3-octave bands in a local browser UI, with peak hold and adjustable *display-only* gain.

## Current status (v0.1.2)

- Tested on stock Akai Force: independent UMIK-1 ALSA capture at **48,000 Hz, S24_3LE, stereo** and live browser RTA.
- Keep the Force on **Internal Audio**. Do not select the UMIK-1 as the main interface.
- USB capture requires explicit `snd_pcm_start()` before reads.
- RTA HTTP server binds only to `127.0.0.1:8096`; use an SSH tunnel.
- Level values are relative **dBFS**, not calibrated dB SPL.

## Build and launch

Requires WSL Ubuntu with `gcc-arm-linux-gnueabihf` and SSH alias `akai-force`.

```bash
sudo apt install gcc-arm-linux-gnueabihf
bash scripts/build-and-run.sh
```

Then in a second terminal:

```bash
ssh -N -L 8096:127.0.0.1:8096 akai-force
```

Open http://127.0.0.1:8096/ in the Windows browser.

To stop: `bash scripts/stop.sh`.

## Structure

- `src/trollfx_rta.c` — ALSA capture, FFT spectrum analyzer, red LED web UI
- `tests/` — standalone UMIK capture and recording diagnostics
- `scripts/` — build and deployment helpers
- `docs/ROADMAP.md` — performance and calibration work

The microphone path is independent from the Force's regular music engine. No boot service is installed and the browser uses an SSH tunnel.

## Licensing

No third-party library source is bundled; system ALSA is loaded dynamically. The project needs an explicit license selection before redistribution.
