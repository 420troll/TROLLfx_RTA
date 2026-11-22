#pragma once

#include "sweep_dsp.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace trollfx::sweep {

constexpr std::size_t kSweepDisplayBands = 31;
constexpr std::size_t kSweepGraphSamples = 256;
constexpr std::size_t kSweepGraphSegments = kSweepGraphSamples - 1;
constexpr std::size_t kSweepDisplayLayers = 8;
// The Force skin renderer can only select a vertical film-strip frame. Keep
// this pair-coded segment atlas below 64 KiB tall while retaining 31 display
// levels; the previous 129x129 lookup produced a 1,065,088 px image that the
// Force firmware failed to load.
constexpr unsigned kSweepGraphVerticalSteps = 31;
constexpr unsigned kSweepGraphMissingFrame = kSweepGraphVerticalSteps * kSweepGraphVerticalSteps;
constexpr unsigned kSweepDisplaySmoothingN = 6;

struct SweepDisplayResponse {
    std::array<double, kSweepDisplayBands> rawRelativeDb{};
    std::array<double, kSweepDisplayBands> smoothedRelativeDb{};
    std::array<float, kSweepDisplayBands> led{};
    std::array<int, kSweepDisplayBands> clipping{}; // 0=within range, 1=above, 2=below, 3=outside measurement bandwidth
    std::array<double, kSweepGraphSamples> graphRelativeDb{};
    std::array<unsigned, kSweepGraphSegments> graphSegmentFrame{};
    double displayRangeDb = 2.0;
    double displayMinimumDb = 0.0;
    double displayMaximumDb = 0.0;
};

// Multi-measurement graph transform. Each curve uses its own 1 kHz-relative
// reference, while every enabled curve shares one common auto-fitted dB axis.
// It contains only fixed-size graph output; CSV parsing and smoothing happen
// on the plugin's sweep worker, never in the audio callback.
struct SweepOverlayDisplay {
    std::array<std::array<unsigned, kSweepGraphSegments>, kSweepDisplayLayers> graphSegmentFrame{};
    std::uint8_t validLayerMask = 0;
    double displayMinimumDb = 0.0;
    double displayMaximumDb = 0.0;
    double displayRangeDb = 0.0;
};

// Reversible view transform for the Force result pages. Always uses 1/6-octave
// smoothing on original high-resolution response data. The common dB window is
// auto-fit to the full valid smoothed response with 5% or 1 dB headroom.
// Raw response and phase arrays are never modified.
bool makeSweepDisplayResponse(const ResponseCurve& raw,
                              SweepDisplayResponse& display,
                              std::string* error = nullptr);

bool makeSweepOverlayDisplay(
    const std::array<const ResponseCurve*, kSweepDisplayLayers>& measurements,
    std::uint8_t enabledMask,
    SweepOverlayDisplay& display,
    std::string* error = nullptr);

} // namespace trollfx::sweep
