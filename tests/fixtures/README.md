`v03-state-fixture.mpcp` is an MPCP version-2 saved-state chunk written by the V0.3 plugin from commit `b98356d26b0d80bb0c51833e947f892c3674259a` using the pinned MPC SDK. It includes non-default values for Display Gain (+15 dB) and Peak Fall (42 dB/s). V0.4's host fixture loads it and checks those values and the appended controls' defaults.

The generation helper is `tests/create_v03_state_fixture.cpp`; pass it a native V0.3 VST2 library and an output path.
