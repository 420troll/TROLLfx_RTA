#include "sweep_display.hpp"

#include <algorithm>
#include <cmath>

namespace trollfx::sweep {
namespace {
constexpr std::array<double, kSweepDisplayBands> kCenters = {
     20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500,
    630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000,
    10000, 12500, 16000, 20000
};

double relativeDb(const std::vector<double>& frequency,
                  const std::vector<double>& magnitude,
                  double hz, double referenceDb) {
    const double value = responseDbAt(frequency, magnitude, hz);
    return std::isfinite(value) ? value - referenceDb : NAN;
}
}

bool makeSweepDisplayResponse(const ResponseCurve& raw, unsigned smoothingN,
                              SweepDisplayResponse& display, std::string* error) {
    auto fail = [error](const char* message) {
        if (error) *error = message;
        return false;
    };
    if (raw.frequencyHz.size() < 2 || raw.frequencyHz.size() != raw.magnitude.size())
        return fail("invalid or incomplete frequency response");
    if (raw.frequencyHz.front() > 1000.0 || raw.frequencyHz.back() < 1000.0)
        return fail("measurement does not include the 1 kHz display reference");

    std::vector<double> smoothed;
    if (!fractionalOctaveSmooth(raw.frequencyHz, raw.magnitude, smoothingN,
                                smoothed, error)) return false;
    const double rawReference = responseDbAt(raw.frequencyHz, raw.magnitude, 1000.0);
    const double smoothReference = responseDbAt(raw.frequencyHz, smoothed, 1000.0);
    if (!std::isfinite(rawReference) || !std::isfinite(smoothReference))
        return fail("invalid 1 kHz response reference");

    SweepDisplayResponse next;
    for (std::size_t i = 0; i < kCenters.size(); ++i) {
        const double hz = kCenters[i];
        if (hz < raw.frequencyHz.front() || hz > raw.frequencyHz.back()) {
            next.rawRelativeDb[i] = -30.0;
            next.smoothedRelativeDb[i] = -30.0;
            next.led[i] = 0.0f;
            continue;
        }
        const double rawDb = relativeDb(raw.frequencyHz, raw.magnitude, hz, rawReference);
        const double smoothDb = relativeDb(raw.frequencyHz, smoothed, hz, smoothReference);
        if (!std::isfinite(rawDb) || !std::isfinite(smoothDb))
            return fail("non-finite response value");
        next.rawRelativeDb[i] = rawDb;
        next.smoothedRelativeDb[i] = smoothDb;
        // LED range is -30..+6 dB relative to 1 kHz, independent of live-RTA
        // display gain and floor settings.
        next.led[i] = static_cast<float>(std::clamp((smoothDb + 30.0) * (24.0 / 36.0), 0.0, 24.0));
    }
    display = next;
    if (error) error->clear();
    return true;
}

} // namespace trollfx::sweep
