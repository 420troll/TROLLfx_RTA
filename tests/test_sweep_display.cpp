#include "../src/sweep/sweep_display.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace trollfx::sweep;
    std::string error;
    ResponseCurve flat;
    for (int i = 0; i <= 1200; ++i) {
        const double hz = 20.0 * std::pow(1000.0, i / 1200.0);
        flat.frequencyHz.push_back(hz);
        flat.magnitude.push_back(0.5);
        flat.phaseRadians.push_back(0.25);
    }
    const auto original = flat.magnitude;
    SweepDisplayResponse display;
    for (unsigned n : {0u, 48u, 24u, 12u, 6u, 3u}) {
        assert(makeSweepDisplayResponse(flat, n, display, &error));
        for (std::size_t i = 0; i < display.led.size(); ++i) {
            assert(std::abs(display.rawRelativeDb[i]) < 1e-9);
            assert(std::abs(display.smoothedRelativeDb[i]) < 1e-9);
            assert(std::abs(display.led[i] - 20.0f) < 1e-5f);
        }
    }
    assert(flat.magnitude == original); // display transform is non-destructive

    ResponseCurve resonant = flat;
    for (std::size_t i = 0; i < resonant.magnitude.size(); ++i) {
        const double oct = std::log2(resonant.frequencyHz[i] / 5000.0);
        resonant.magnitude[i] += 0.45 * std::exp(-0.5 * std::pow(oct / 0.018, 2));
    }
    assert(makeSweepDisplayResponse(resonant, 0, display, &error));
    const double rawPeak = display.rawRelativeDb[24];
    double previous = rawPeak;
    for (unsigned n : {48u, 24u, 12u, 6u, 3u}) {
        assert(makeSweepDisplayResponse(resonant, n, display, &error));
        assert(display.rawRelativeDb[24] == rawPeak);
        assert(display.smoothedRelativeDb[24] < previous);
        previous = display.smoothedRelativeDb[24];
    }
    assert(display.led[24] >= 0.0f && display.led[24] <= 24.0f);

    ResponseCurve partial;
    partial.frequencyHz = {100.0, 500.0, 1000.0, 5000.0, 10000.0};
    partial.magnitude = {0.5, 0.5, 0.5, 1.0, 1.0};
    assert(makeSweepDisplayResponse(partial, 6, display, &error));
    assert(display.led[0] == 0.0f && display.led[30] == 0.0f);
    assert(display.smoothedRelativeDb[0] == -30.0 && display.smoothedRelativeDb[30] == -30.0);

    ResponseCurve noReference;
    noReference.frequencyHz = {100.0, 200.0, 500.0};
    noReference.magnitude = {0.5, 0.5, 0.5};
    assert(!makeSweepDisplayResponse(noReference, 6, display, &error));
    ResponseCurve malformed;
    malformed.frequencyHz = {100.0, 1000.0};
    malformed.magnitude = {0.5};
    assert(!makeSweepDisplayResponse(malformed, 6, display, &error));

    std::cout << "PASS saved sweep display transform: flat, smoothing, reference, range, immutability\n";
}
