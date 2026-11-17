#ifndef TROLLFX_RTA_SPECTRUM_JSON_H
#define TROLLFX_RTA_SPECTRUM_JSON_H

#include <array>
#include <string>

namespace trollfx {
struct Snapshot {
    bool ok = false;
    std::array<float, 31> level{};
    std::array<float, 31> peak{};
    float rawDbfs = -120.0f;
    float splZ = -120.0f;
    float splA = -120.0f;
    float sensitivityFactorDb = 2.7673f;
    int orientation = 0;
    int referenceType = 0; // 0 provisional, 1 independently measured CLI, 2 user reference
    bool calibrated = false;
};

bool parseSpectrumJson(const std::string& json, Snapshot& out);
void combine16Power(const std::array<float, 31>& in, std::array<float, 16>& out);
}
#endif
