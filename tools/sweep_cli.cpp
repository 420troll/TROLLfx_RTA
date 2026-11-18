#include "../src/sweep/sweep_dsp.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

static bool fieldSize(const std::string& json,const char* name,std::size_t& value) {
    const std::string key=std::string("\"")+name+"\"";const auto p=json.find(key);if(p==std::string::npos)return false;const auto colon=json.find(':',p+key.size());if(colon==std::string::npos)return false;const char*begin=json.c_str()+colon+1;char*end=nullptr;unsigned long long n=std::strtoull(begin,&end,10);if(end==begin)return false;value=static_cast<std::size_t>(n);return true;
}

int main(int argc,char**argv) {
    using namespace trollfx::sweep;
    if(argc==2&&std::string(argv[1])=="--version"){std::cout<<"TROLLfx Sweep Lab DSP 0.5.0\n";return 0;}
    if(argc>=2&&std::string(argv[1])=="generate") {
        if(argc!=9){std::cerr<<"usage: sweep-cli generate FILE FS SECONDS START_HZ STOP_HZ PEAK_DBFS FADE_SECONDS\n";return 2;}
        SweepOptions o; o.sampleRate=static_cast<unsigned>(std::strtoul(argv[3],nullptr,10));o.durationSeconds=std::strtod(argv[4],nullptr);o.startHz=std::strtod(argv[5],nullptr);o.stopHz=std::strtod(argv[6],nullptr);o.peakDbfs=std::strtod(argv[7],nullptr);o.fadeSeconds=std::strtod(argv[8],nullptr);
        std::string error;if(!validate(o,&error)){std::cerr<<error<<'\n';return 2;}auto x=generate(o);if(!writeFloatWav(argv[2],x,o.sampleRate,&error)){std::cerr<<error<<'\n';return 1;}return 0;
    }
    if(argc>=2&&std::string(argv[1])=="generate-test") {
        if(argc!=8){std::cerr<<"usage: sweep-cli generate-test FILE FS SECONDS START_HZ STOP_HZ PEAK_DBFS\n";return 2;}
        SweepOptions o;o.sampleRate=static_cast<unsigned>(std::strtoul(argv[3],nullptr,10));o.durationSeconds=std::strtod(argv[4],nullptr);o.startHz=std::strtod(argv[5],nullptr);o.stopHz=std::strtod(argv[6],nullptr);o.peakDbfs=std::strtod(argv[7],nullptr);
        std::string error;if(!validate(o,&error)){std::cerr<<error<<'\n';return 2;}auto signal=generateMeasurementSignal(o);if(signal.samples.empty()){std::cerr<<"invalid test signal settings\n";return 2;}
        if(!writeFloatWav(argv[2],signal.samples,o.sampleRate,&error)){std::cerr<<error<<'\n';return 1;}
        std::ofstream sidecar(std::string(argv[2])+".json");if(!sidecar){std::cerr<<"cannot write marker metadata\n";return 1;}
        sidecar<<"{\"format\":\"TROLLfx Sweep Lab test signal\",\"sample_rate_hz\":"<<o.sampleRate<<",\"duration_s\":"<<o.durationSeconds<<",\"start_hz\":"<<o.startHz<<",\"stop_hz\":"<<o.stopHz<<",\"peak_dbfs\":"<<o.peakDbfs<<",\"marker_start_sample\":"<<signal.markerStartSample<<",\"marker_end_sample\":"<<signal.markerEndSample<<",\"marker_length_samples\":"<<signal.marker.size()<<"}\n";
        return sidecar?0:1;
    }
    if(argc>=2&&std::string(argv[1])=="analyze") {
        if(argc<5||argc>10||argc==7){std::cerr<<"usage: sweep-cli analyze REFERENCE.wav CAPTURE.wav OUTPUT_DIR [CLOCK_PPM [START_HZ STOP_HZ [CALIBRATION.txt [0|90]]]]\n";return 2;}
        std::vector<double> reference,capture;unsigned fs1=0,fs2=0;std::string error;
        if(!readWav(argv[2],reference,fs1,&error)){std::cerr<<error<<'\n';return 1;}
        if(!readWav(argv[3],capture,fs2,&error)){std::cerr<<error<<'\n';return 1;}
        if(fs1!=fs2){std::cerr<<"reference and capture sample rates differ\n";return 2;}
        AnalysisOptions o;o.sampleRate=fs1;o.stopHz=std::min(20000.0,0.45*fs1);if(argc>=6)o.clockPpm=std::strtod(argv[5],nullptr);if(argc>=8){o.startHz=std::strtod(argv[6],nullptr);o.stopHz=std::strtod(argv[7],nullptr);}
        ClockAlignment alignment;bool autoAligned=false;std::ifstream sidecar(std::string(argv[2])+".json");
        if(sidecar){std::string metadata((std::istreambuf_iterator<char>(sidecar)),{});std::size_t markerStart=0,markerEnd=0,markerLength=0;
            if(!fieldSize(metadata,"marker_start_sample",markerStart)||!fieldSize(metadata,"marker_end_sample",markerEnd)||!fieldSize(metadata,"marker_length_samples",markerLength)||markerStart>reference.size()||markerLength>reference.size()-markerStart||markerEnd<markerStart){std::cerr<<"invalid reference marker metadata\n";return 2;}
            std::vector<double> marker(reference.begin()+markerStart,reference.begin()+markerStart+markerLength);
            if(!locateClockMarkers(marker,capture,static_cast<double>(markerEnd-markerStart),alignment,&error)){std::cerr<<error<<'\n';return 1;}o.clockPpm=alignment.clockPpm;autoAligned=true;
        }
        AnalysisResult result;if(!analyze(reference,capture,o,result,&error)){std::cerr<<error<<'\n';return 1;}
        if(autoAligned){result.markerAlignmentValid=true;result.capturedFirstMarker=alignment.firstMarkerSample;result.capturedSecondMarker=alignment.secondMarkerSample;result.markerConfidence=std::min(alignment.firstConfidence,alignment.secondConfidence);}
        if(argc>=9){const int orientation=argc==10?std::atoi(argv[9]):0;CalibrationCurve curve;if(!readCalibration(argv[8],orientation,curve,&error)||!applyCalibration(result,curve,&error)){std::cerr<<error<<'\n';return 1;}}
        if(!writeArtifacts(argv[4],result,fs1,o.startHz,o.stopHz,&error)){std::cerr<<error<<'\n';return 1;}
        std::error_code ec;
        if(argc>=9){std::filesystem::copy_file(argv[8],std::string(argv[4])+"/calibration-used.txt",std::filesystem::copy_options::overwrite_existing,ec);if(ec){std::cerr<<"cannot preserve microphone calibration: "<<ec.message()<<'\n';return 1;}}
        std::filesystem::copy_file(argv[2],std::string(argv[4])+"/reference.wav",std::filesystem::copy_options::overwrite_existing,ec);
        if(ec){std::cerr<<"cannot preserve reference sweep: "<<ec.message()<<'\n';return 1;}
        std::filesystem::copy_file(argv[3],std::string(argv[4])+"/capture.wav",std::filesystem::copy_options::overwrite_existing,ec);
        if(ec){std::cerr<<"cannot preserve raw capture: "<<ec.message()<<'\n';return 1;}
        if(autoAligned){std::filesystem::copy_file(std::string(argv[2])+".json",std::string(argv[4])+"/reference.wav.json",std::filesystem::copy_options::overwrite_existing,ec);if(ec){std::cerr<<"cannot preserve marker metadata: "<<ec.message()<<'\n';return 1;}}
        std::cout<<"analyzed "<<capture.size()<<" samples; IR peak at "<<result.impulsePeak<<"; clipped="<<result.clippedSamples;
        if(autoAligned)std::cout<<"; estimated clock="<<alignment.clockPpm<<" ppm confidence="<<result.markerConfidence;
        std::cout<<"\n";return 0;
    }
    std::cerr<<"TROLLfx Sweep Lab DSP 0.5.0\ncommands: generate, generate-test, analyze\n";return 2;
}
