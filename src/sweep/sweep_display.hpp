#pragma once

#include "sweep_dsp.hpp"

#include <array>
#include <string>

namespace trollfx::sweep {

constexpr std::size_t kSweepDisplayBands = 31;
constexpr std::size_t kSweepGraphSamples = 256;
constexpr std::size_t kSweepGraphSegments = kSweepGraphSamples - 1;
constexpr unsigned kSweepGraphVerticalSteps = 129;
constexpr unsigned kSweepGraphMissingFrame = kSweepGraphVerticalSteps * kSweepGraphVerticalSteps;

struct SweepDisplayResponse {
    std::array<double, kSweepDisplayBands> rawRelativeDb{};
    std::array<double, kSweepDisplayBands> smoothedRelativeDb{};
    std::array<float, kSweepDisplayBands> led{};
    std::array<int, kSweepDisplayBands> clipping{}; // 0=within range, 1=above, 2=below, 3=outside measurement bandwidth
    std::array<double, kSweepGraphSamples> graphRelativeDb{};
    std::array<unsigned, kSweepGraphSegments> graphSegmentFrame{};
    double displayRangeDb = 48.0;
    double displayMinimumDb = 0.0;
    double displayMaximumDb = 0.0;
};

// Reversible view transform for the Force result page. Raw response data are
// never modified. Curves are referenced to the measured 1 kHz magnitude and
// limited to the actual valid sweep bandwidth.
bool makeSweepDisplayResponse(const ResponseCurve& raw, unsigned smoothingN,
                              SweepDisplayResponse& display,
                              std::string* error = nullptr,
                              double displayRangeDb = 48.0,
                              double displayCenterDb = -12.0);

} // namespace trollfx::sweep
