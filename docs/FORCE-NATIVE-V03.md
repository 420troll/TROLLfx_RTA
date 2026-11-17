# TROLLfx RTA V0.3 — native Force plugin

## State and architecture

V0.3 is a stereo VST2 effect for the MPC/Force external-plugin host. It leaves the V0.2 daemon and ALSA capture path independent. The effect's process callback only copies separate input/output buffers when necessary; the host's normal in-place buffers pass through untouched. It does not capture audio, change Force routing, or delay the program audio.

```text
UMIK-1 -> unchanged V0.2 daemon / ALSA hw:U18dB,0 -> 127.0.0.1:8096 /api/spectrum
                                                    |
                                 one shared worker per host process
                                                    |
                         atomic VST parameters -> native MPC skin
```

One client worker is shared by all loaded RTA instances in a process. It requests JSON at 5 Hz with bounded localhost connect/read/write timeouts and a 32 KiB response cap. A malformed, missing, or offline response marks the microphone offline; polling continues so a daemon restart is recovered automatically. JSON decoding, sockets, display ballistics, and host display invalidation are all outside `processReplacing`. Audio callbacks do not acquire the worker mutex. `TROLLFX_RTA_PORT` is an optional test override; the production default remains 8096.

Refresh rate can be set to an integer from 1 to 10 Hz with `TROLLFX_RTA_REFRESH_HZ` before the host starts; the default is 5 Hz. The analysis-average control is service-global (there is one V0.2 analyzer) and the most recently changed instance setting is applied to that service.

The V0.2 service remains responsible for mic frequency-response correction, 0°/90° orientation, Z/A weighting, and SPL reference status. This plugin displays the reported orientation, the parsed `Sens Factor = 2.7673 dB` metadata, and `PROVISIONAL`, `USER REF`, or service-verified reference status. It does not treat the sensitivity field as an absolute SPL offset. Force ALSA input gain still needs an acoustic 94 dB reference measurement before absolute SPL can be labeled verified.

## Native display

The skin uses the supported MPC parameter-bound `Image` film-strip component rather than unsupported custom drawing. The generated red ladder art has 25 frames (0–24 lit segments); a second film strip displays the peak marker. The **31 BAND** page renders all 31 bands; **16 BAND** renders paired adjacent values using power summation. These are selectable function-key pages. Frequency labels, static panel art, and the LED states are local to the plugin skin.

The **DISPLAY** page provides gain +11 dB, floor -90 dBFS, smoothing 0 ms, average 1 frame, 50 ms attack, 33 dB/s fall, peak hold 200 ms, 60 dB/s peak fall, hold enable, and peak reset. A single worker applies these controls to LED parameters; raw dBFS and service LZ/LA values are not changed by display gain or floor. Peak reset posts to the V0.2 API. Analysis-average changes post the selected 1/2/4/8/16 frame value to the V0.2 service.

Q-Links 1–11 on DISPLAY map to its controls. Meter parameters are marked non-automatable and drawn by parameter-bound images. The **LEVEL** page displays raw dBFS, LZ/LA values, mic status, orientation, and reference status. The SDK's static scripted skin cannot provide arbitrary live text/graphics without host parameter reads; skin behavior and redraw cost still require Force-host acceptance.

### Orientation selection

The plug-in exposes the active service orientation as a readout. The V0.2 daemon selects its calibration file at startup (`--orientation 0` or `--orientation 90`); changing it requires restarting that daemon with the chosen value. The plugin intentionally does not show a fake orientation toggle that cannot change the existing service's calibration pointer. Both calibration curves and their shared `2.7673 dB` factor are preserved and tested in the V0.2 engine.

The V0.2 service also accepts an independently known Z-weighted level through `POST /api/spl/reference?db=<known-dB-SPL>` and labels this **USER REF**; the web control or service API can set it. For example, with a known 94 dB calibrator currently on the microphone, call `curl -X POST 'http://127.0.0.1:8096/api/spl/reference?db=94'` on the Force. It remains unverified by the software. Clear it with `curl -X POST 'http://127.0.0.1:8096/api/spl/clear'` after input-gain changes. The VST keeps this workflow service-side rather than presenting an unverified acoustic reference as a plugin knob.

## Source and SDK evaluation

The existing engine is retained as the single measurement source. Its design and comparative review of prodmesh-rta, PFFFT, and python-soundlevelmeter are documented in [V0.2-DSP.md](V0.2-DSP.md). PFFFT remains the only bundled FFT dependency; the plugin does not add a second analyzer or Python/Qt/NumPy/SciPy dependencies.

The plugin uses InMusic's MPC Plugin Builder SDK framework and declarative skin contract: [upstream SDK](https://github.com/inmusic-software/mpc-plugin-builder-sdk). V0.3 build was made against SDK revision `febd2ca5bd9eb16c4a412837196e145566bc981b` (MIT). This SDK supports ARM32/ARM64 VST2 entry points and parameter-bound `FilmStrip` images; it does not offer unrestricted live canvas rendering. See `plugins/TrollfxRta/THIRD_PARTY_NOTICES.txt`.

## Build and test

Obtain the pinned SDK revision and Zig 0.17.1. Set `MPC_SDK_DIR` to the SDK checkout. If Zig is provided as the `ziglang` Python package, set `PYTHONPATH` to its install directory and `SDK_ZIG_PYTHON` to that Python interpreter. Then run:

```sh
bash scripts/test-force-v03.sh
MPC_SDK_DIR=/path/to/mpc-plugin-builder-sdk \
  SDK_ZIG_PYTHON=/path/to/python PYTHONPATH=/path/to/ziglang \
  bash scripts/build-force-v03.sh
```

The build produces 32-bit ARM hard-float and 64-bit ARM VST2 binaries, runs the SDK validator, the SDK host simulator, and a local VST host fixture that checks two instances, transparent pass-through, shutdown, and offline-to-online recovery. V0.2 DSP tests include both calibration parsers, the 2.7673 dB metadata, orientation selection/interpolation, and SPL mapping. Host timings are not Force performance measurements.

The output package is under `dist/force-native-v03-package/`. For a hardware drive mounted on the development host, use the package's `install-force-v03.sh` with the explicit drive root and built plugin folder. It requires an existing `VST2 Plugins` folder and backs up only a pre-existing TROLLfx RTA plugin to the drive root before replacement. `rollback-force-v03.sh` removes/restores only that plugin. Neither script is run by the build.

## Hardware acceptance still required

- Confirm supported MPC/Force firmware and external-plugin enablement.
- Install to the confirmed media destination; confirm plugin browser name, UID, touch pages, Q-Link controls, film-strip mapping and redraw speed.
- With Force playback on **Internal Audio**, run program audio and verify bit-transparent effect pass-through, no glitches, and no changes to `MPC.settings` or global audio routing.
- Verify worker reconnect after daemon stop/restart and UMIK unplug/replug; verify no duplicate ALSA capture and only one API poller with multiple plugin instances.
- Test 31 and 16-band rendering, meter ballistics and resets while running three Doppler instances plus Chakra. Measure Force CPU and UI cost directly.
- Compare 0° and 90° curves and validate the USB capture sensitivity using a traceable acoustic calibrator or independently verified reference. Until then label levels provisional or user-reference as appropriate.

No Force-host behavior, hardware CPU use, USB capture behavior, or calibrated absolute SPL is claimed from cross-compilation or desktop simulation.
