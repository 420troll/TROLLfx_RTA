#pragma once

#include "sweep_dsp.hpp"

#include <array>
#include <string>

namespace trollfx::sweep {

constexpr std::size_t kSweepDisplayBands = 31;

struct SweepDisplayResponse {
    std::array<double, kSweepDisplayBands> rawRelativeDb{};
    std::array<double, kSweepDisplayBands> smoothedRelativeDb{};
    std::array<float, kSweepDisplayBands> led{};
};

// Reversible view transform for the Force result page. Raw response data are
// never modified. Curves are referenced to the measured 1 kHz magnitude and
// limited to the actual valid sweep bandwidth.
bool makeSweepDisplayResponse(const ResponseCurve& raw, unsigned smoothingN,
                              SweepDisplayResponse& display,
                              std::string* error = nullptr);

} // namespace trollfx::sweep
