#include "spectrum_json.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace trollfx {
namespace {
const char* keyValue(const std::string& s, const char* key) {
    const std::string needle = std::string("\"") + key + "\"";
    size_t p = s.find(needle);
    if (p == std::string::npos) return nullptr;
    p = s.find(':', p + needle.size());
    if (p == std::string::npos) return nullptr;
    ++p;
    while (p < s.size() && (s[p] == ' ' || s[p] == '\n' || s[p] == '\r' || s[p] == '\t')) ++p;
    return p < s.size() ? s.c_str() + p : nullptr;
}
bool number(const std::string& s, const char* key, float& value) {
    const char* p = keyValue(s, key);
    if (!p || std::strncmp(p, "null", 4) == 0) return false;
    char* end = nullptr;
    const float v = std::strtof(p, &end);
    if (end == p || !std::isfinite(v)) return false;
    value = v;
    return true;
}
bool boolean(const std::string& s, const char* key, bool& value) {
    const char* p = keyValue(s, key);
    if (!p) return false;
    if (std::strncmp(p, "true", 4) == 0) { value = true; return true; }
    if (std::strncmp(p, "false", 5) == 0) { value = false; return true; }
    return false;
}
bool array31(const std::string& s, const char* key, std::array<float, 31>& a) {
    const char* p = keyValue(s, key);
    if (!p || *p++ != '[') return false;
    for (size_t i = 0; i < a.size(); ++i) {
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
        char* end = nullptr;
        const float v = std::strtof(p, &end);
        if (end == p || !std::isfinite(v) || v < -200.0f || v > 100.0f) return false;
        a[i] = v;
        p = end;
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
        if (i + 1 < a.size()) { if (*p++ != ',') return false; }
    }
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
    return *p == ']';
}
}

bool parseSpectrumJson(const std::string& json, Snapshot& out) {
    Snapshot s;
    if (!boolean(json, "ok", s.ok) || !array31(json, "level", s.level) ||
        !array31(json, "peak", s.peak) || !number(json, "rms_dbfs", s.rawDbfs)) return false;
    float v;
    if (number(json, "spl_z", v)) s.splZ = v;
    if (number(json, "spl_a", v)) s.splA = v;
    if (number(json, "sensitivity_factor_db", v)) s.sensitivityFactorDb = v;
    if (number(json, "orientation", v)) s.orientation = static_cast<int>(v);
    if (s.orientation != 0 && s.orientation != 90) return false;
    bool b;
    if (!boolean(json, "calibrated", b)) return false;
    s.calibrated = b;
    const char* ref = keyValue(json, "spl_reference_type");
    if (!ref || *ref++ != '"') return false;
    if (std::strncmp(ref, "cli\"", 4) == 0) s.referenceType = 1;
    else if (std::strncmp(ref, "user\"", 5) == 0) s.referenceType = 2;
    else if (std::strncmp(ref, "none\"", 5) == 0) s.referenceType = 0;
    else return false;
    if (s.calibrated && s.referenceType != 1) return false;
    out = s;
    return true;
}

void combine16Power(const std::array<float, 31>& in, std::array<float, 16>& out) {
    for (size_t i = 0; i < 15; ++i) {
        const float hi = std::fmax(in[2*i], in[2*i+1]);
        const float lo = std::fmin(in[2*i], in[2*i+1]);
        out[i] = hi + 10.0f * std::log10(1.0f + std::pow(10.0f, (lo-hi)/10.0f));
    }
    out[15] = in[30];
}
}
