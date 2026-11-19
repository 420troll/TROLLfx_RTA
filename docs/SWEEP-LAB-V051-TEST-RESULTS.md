# V0.5.1 build and test record

Build command: `MPC_SDK_DIR=/path/to/mpc-plugin-builder-sdk SDK_ZIG_PYTHON=/usr/bin/python3 PYTHONPATH=/path/to/ziglang bash scripts/build-force-v051.sh`.

The pinned MPC Plugin Builder SDK strict validator completed with zero errors and zero warnings. It verified all parameter bindings are within 0–154, six tabs and Q-Link maps, all image assets, UID `TfRA`, stereo I/O, and plugin metadata. The ARM32 VST exports `VSTPluginMain`, is ARMv7 with VFP-register hard-float ABI, and uses GLIBC no newer than 2.30. The packaged ARM32 analyzer and sweep CLI are ARM hard-float ELF executables. The CLI has no `libstdc++` runtime dependency.

Passing automated checks:

- Sweep DSP, generated fixtures, sweep comparison, CLI, service API, clock alignment, clipping, and existing sweep regression tests.
- Fractional smoothing tests: flat response, monotonic narrowing-peak attenuation across all five bandwidths, trend/edge behavior, exact raw-copy mode, metadata update without duplicates, clipped-session rejection, unchanged raw CSV and impulse WAV, and matching SVG trace point count.
- Independent dense NumPy log-frequency quadrature matches the C++ smoother within 2e-6 magnitude.
- pyfar two-tap deconvolution reference test passed using pyfar 0.8.1.
- V0.4 lifecycle and V0.2/V0.3 compatibility suite passed, including transparent stereo pass-through and saved V0.3 state loading.
- Mocked native VST sweep test passed: one arm/start, aligned result and IR saved, smoothing and Previous 2 selection re-rendered sessions without another sweep request.
- Package verification and simulated V0.5.0 install/backup/rollback passed. `MPC.settings` was not changed.

One host re-render of a saved ~108k-row response took about 0.49 s on the development machine. This is a host measurement, not a Force performance claim. RTA host benchmarks vary by run; the test harness compares V0.1.2 and V0.2 live DSP. No real Force installation or touchscreen measurement was performed.

Package: `dist/force-native-v051-package.tar.gz`

Remaining hardware checks are listed in `SWEEP-LAB-V051-SMOOTHING.md`: Force scan/load, displayed frequency-label legibility, LED overview behavior, selector-driven rendering and timing, and playback remaining on Internal Audio.
