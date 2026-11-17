#include "../plugins/TrollfxRta/src/spectrum_json.h"
#include <cmath>
#include <cstdio>
#include <string>

static int fail = 0;
static void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++fail;
}
int main() {
    std::string json = R"({"ok":true,"rms_dbfs":-20.0,"z_corrected_dbfs":-19.5,"a_corrected_dbfs":-20.1,"sensitivity_factor_db":2.7673,"orientation":0,"calibrated":false,"spl_z":null,"spl_a":null,"spl_reference_type":"none","level":[)";
    for (int i=0;i<31;i++) { if (i) json += ','; json += std::to_string(-90+i); }
    json += R"(],"peak":[)";
    for (int i=0;i<31;i++) { if (i) json += ','; json += std::to_string(-80+i); }
    json += "]}";
    trollfx::Snapshot s;
    check(trollfx::parseSpectrumJson(json,s), "parse valid 31-band provisional response");
    check(s.orientation==0 && !s.calibrated && s.referenceType==0 && s.level[30]==-60, "preserve angle and provisional status");
    check(std::fabs(s.sensitivityFactorDb-2.7673f)<1e-5f, "retain UMIK Sens Factor metadata without applying an absolute offset");
    std::string user=json; auto p=user.find("\"orientation\":0"); user.replace(p,15,"\"orientation\":90");
    p=user.find("\"spl_reference_type\":\"none\""); user.replace(p,27,"\"spl_reference_type\":\"user\"");
    user.replace(user.find("\"spl_z\":null"),12,"\"spl_z\":94.0");
    check(trollfx::parseSpectrumJson(user,s) && s.orientation==90 && s.referenceType==2 && !s.calibrated && s.splZ==94, "parse 90-degree USER REF without claiming verified calibration");
    check(!trollfx::parseSpectrumJson("{\"ok\":true}",s), "reject missing measurement fields");
    std::string shortArray=json; const auto at=shortArray.find("\"peak\""); shortArray.replace(at, shortArray.find(']',at)-at+1, "\"peak\":[-1]");
    check(!trollfx::parseSpectrumJson(shortArray,s), "reject malformed band length");
    std::array<float,31> in{}; std::array<float,16> out{}; in.fill(-120); in[0]=-6; in[1]=-6;
    trollfx::combine16Power(in,out);
    check(std::fabs(out[0]-(-2.989700))<1e-5, "16-band pair combines energy in power domain");
    in[0]=-6; in[1]=-120; trollfx::combine16Power(in,out);
    check(std::fabs(out[0]-(-6.0))<1e-4 && out[15]==-120, "16-band single-bin edge and final band");
    return fail ? 1 : 0;
}
