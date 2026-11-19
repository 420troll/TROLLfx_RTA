#include "sweep_display.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

bool validRange(double range) {
    return range == 24.0 || range == 36.0 || range == 48.0 || range == 60.0 || range == 72.0;
}

double graphFrequency(std::size_t i) {
    return 20.0 * std::pow(1000.0, static_cast<double>(i) /
                                      static_cast<double>(kSweepGraphSamples - 1));
}

unsigned graphY(double db, double minimum, double range) {
    const double topDown = (minimum + range - db) *
                           (kSweepGraphVerticalSteps - 1) / range;
    return static_cast<unsigned>(std::clamp(std::lround(topDown), 0L,
                       static_cast<long>(kSweepGraphVerticalSteps - 1)));
}
}

bool makeSweepDisplayResponse(const ResponseCurve& raw, unsigned smoothingN,
                              SweepDisplayResponse& display, std::string* error,
                              double displayRangeDb, double displayCenterDb) {
    auto fail = [error](const char* message) {
        if (error) *error = message;
        return false;
    };
    if (raw.frequencyHz.size() < 2 || raw.frequencyHz.size() != raw.magnitude.size())
        return fail("invalid or incomplete frequency response");
    if (!validRange(displayRangeDb) || !std::isfinite(displayCenterDb))
        return fail("invalid sweep display range or center");
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
    next.displayRangeDb = displayRangeDb;
    next.displayMinimumDb = displayCenterDb - displayRangeDb * 0.5;
    next.displayMaximumDb = displayCenterDb + displayRangeDb * 0.5;
    for (std::size_t i = 0; i < kCenters.size(); ++i) {
        const double hz = kCenters[i];
        if (hz < raw.frequencyHz.front() || hz > raw.frequencyHz.back()) {
            next.rawRelativeDb[i] = std::numeric_limits<double>::quiet_NaN();
            next.smoothedRelativeDb[i] = std::numeric_limits<double>::quiet_NaN();
            next.led[i] = 0.0f;
            next.clipping[i] = 3;
            continue;
        }
        const double rawDb = relativeDb(raw.frequencyHz, raw.magnitude, hz, rawReference);
        const double smoothDb = relativeDb(raw.frequencyHz, smoothed, hz, smoothReference);
        if (!std::isfinite(rawDb) || !std::isfinite(smoothDb))
            return fail("non-finite response value");
        next.rawRelativeDb[i] = rawDb;
        next.smoothedRelativeDb[i] = smoothDb;
        // One shared dB window is used for every band. Stored response values
        // remain unclamped; only this visual conversion saturates.
        next.led[i] = static_cast<float>(std::clamp(
            24.0 * (smoothDb - next.displayMinimumDb) / displayRangeDb, 0.0, 24.0));
        next.clipping[i] = smoothDb >= next.displayMaximumDb ? 1 :
                           smoothDb <= next.displayMinimumDb ? 2 : 0;
    }

    // The native Force graph uses 256 log-spaced points from the high
    // resolution smoothed response. Each adjacent pair maps to one connected
    // segment frame in the Image film strip; no reduction through the 31 LED
    // centers occurs here.
    for (std::size_t i = 0; i < kSweepGraphSamples; ++i) {
        const double hz = graphFrequency(i);
        double value = std::numeric_limits<double>::quiet_NaN();
        if (hz >= raw.frequencyHz.front() && hz <= raw.frequencyHz.back())
            value = relativeDb(raw.frequencyHz, smoothed, hz, smoothReference);
        next.graphRelativeDb[i] = value;
    }
    for (std::size_t i = 0; i < kSweepGraphSegments; ++i) {
        const double a = next.graphRelativeDb[i], b = next.graphRelativeDb[i + 1];
        if (!std::isfinite(a) || !std::isfinite(b)) {
            next.graphSegmentFrame[i] = kSweepGraphMissingFrame;
            continue;
        }
        const unsigned ya = graphY(a, next.displayMinimumDb, displayRangeDb);
        const unsigned yb = graphY(b, next.displayMinimumDb, displayRangeDb);
        next.graphSegmentFrame[i] = ya * kSweepGraphVerticalSteps + yb;
    }
    display = next;
    if (error) error->clear();
    return true;
}

} // namespace trollfx::sweep
