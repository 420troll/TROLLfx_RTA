#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <limits>
#include <cstdint>

namespace trollfx::sweep {

struct SweepOptions {
    unsigned sampleRate = 48000;
    double durationSeconds = 10.0;
    double startHz = 20.0;
    double stopHz = 20000.0;
    double peakDbfs = -30.0;
    double fadeSeconds = 0.01;
};

struct AnalysisOptions {
    unsigned sampleRate = 48000;
    double startHz = 20.0;
    double stopHz = 20000.0;
    // Capture clock minus playback clock, in parts per million. Positive means
    // capture contains more samples per second than the rendered reference.
    double clockPpm = 0.0;
    bool rejectClipping = true;
    double clipThreshold = 0.999;
};

struct AnalysisResult {
    std::vector<double> impulse;
    std::vector<double> frequencyHz;
    std::vector<double> magnitude;
    std::vector<double> magnitudeDb;
    std::vector<double> phaseRadians;
    std::size_t impulsePeak = 0;
    std::size_t clippedSamples = 0;
    double inputPeak = 0.0;
    double residualClockPpm = 0.0;
    int calibrationOrientationDeg = -1;
    double calibrationSensitivityFactorDb = std::numeric_limits<double>::quiet_NaN();
    std::uint64_t calibrationHashFnv1a64 = 0;
    std::string calibrationSourcePath;
    bool markerAlignmentValid = false;
    std::size_t capturedFirstMarker = 0;
    std::size_t capturedSecondMarker = 0;
    double markerConfidence = 0.0;
};

struct CalibrationPoint { double frequencyHz = 0.0; double responseDb = 0.0; };
struct CalibrationCurve {
    std::vector<CalibrationPoint> points;
    double sensitivityFactorDb = std::numeric_limits<double>::quiet_NaN();
    int orientationDeg = -1;
    std::string sourcePath;
    std::uint64_t hashFnv1a64 = 0;
};

struct MeasurementSignal {
    std::vector<double> samples;
    std::vector<double> marker;
    std::size_t markerStartSample = 0;
    std::size_t markerEndSample = 0;
};

struct ClockAlignment {
    std::size_t firstMarkerSample = 0;
    std::size_t secondMarkerSample = 0;
    double clockPpm = 0.0;
    double firstConfidence = 0.0;
    double secondConfidence = 0.0;
};

struct ResponseCurve {
    std::vector<double> frequencyHz;
    std::vector<double> magnitude;
    std::vector<double> gainDb;
    std::vector<double> phaseRadians;
};

bool validate(const SweepOptions& options, std::string* error = nullptr);
bool resampleMono(const std::vector<double>& input, unsigned inputRate,
                  unsigned outputRate, std::vector<double>& output,
                  std::string* error = nullptr);
std::vector<double> generate(const SweepOptions& options);
MeasurementSignal generateMeasurementSignal(const SweepOptions& options,
                                             double preRollSeconds = 0.25,
                                             double postRollSeconds = 0.25,
                                             double gapSeconds = 0.05,
                                             double markerSeconds = 0.08);
bool locateClockMarkers(const std::vector<double>& marker,
                        const std::vector<double>& capture,
                        double expectedSeparation,
                        ClockAlignment& alignment,
                        std::string* error = nullptr);
bool analyze(const std::vector<double>& reference,
             const std::vector<double>& capture,
             const AnalysisOptions& options,
             AnalysisResult& result,
             std::string* error = nullptr);
bool readCalibration(const std::string& path, int orientationDeg,
                     CalibrationCurve& curve, std::string* error = nullptr);
double calibrationDb(const CalibrationCurve& curve, double frequencyHz);
bool applyCalibration(AnalysisResult& result, const CalibrationCurve& curve,
                      std::string* error = nullptr);

// Detect absolute drift from two independent marker observations. Values are
// sample indices in their respective clocks; expected separation is in the
// playback clock. A single marker cannot identify clock drift.
bool estimateClockPpm(double expectedSeparation,
                      double observedCaptureSeparation,
                      double& ppm);

bool readWav(const std::string& path, std::vector<double>& mono,
             unsigned& sampleRate, std::string* error = nullptr);
bool writeFloatWav(const std::string& path, const std::vector<double>& mono,
                   unsigned sampleRate, std::string* error = nullptr);
bool writeArtifacts(const std::string& directory, const AnalysisResult& result,
                    unsigned sampleRate, double startHz, double stopHz,
                    std::string* error = nullptr,
                    unsigned smoothingOctaveDenominator = 6);
// Smoothing denominator is 0 for raw values, or 3, 6, 12, 24, or 48.
// The average is magnitude-domain and uniform in log-frequency, with the
// requested band clipped and renormalized at valid measurement edges.
bool fractionalOctaveSmooth(const std::vector<double>& frequencyHz,
                            const std::vector<double>& magnitude,
                            unsigned octaveDenominator,
                            std::vector<double>& smoothed,
                            std::string* error = nullptr);
bool readResponseCsv(const std::string& path, ResponseCurve& response,
                     std::string* error = nullptr);
bool writeSmoothedArtifacts(const std::string& directory,
                            const ResponseCurve& raw,
                            unsigned octaveDenominator,
                            double referenceHz = 1000.0,
                            bool includeRawTrace = true,
                            std::string* error = nullptr);
// Re-render an existing session from its unsmoothed frequency-response.csv.
// Raw WAV/CSV/IR/phase data are read-only; only graph, smoothed CSV and
// smoothing metadata are updated.
bool reprocessSession(const std::string& directory,
                      unsigned octaveDenominator,
                      double referenceHz = 1000.0,
                      bool includeRawTrace = true,
                      std::string* error = nullptr);
double responseDbAt(const std::vector<double>& frequencyHz,
                    const std::vector<double>& magnitude,
                    double queryHz);

} // namespace trollfx::sweep
