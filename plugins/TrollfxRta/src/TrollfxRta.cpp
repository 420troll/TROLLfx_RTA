#include "mpc_plugin.h"
#include "spectrum_json.h"
#include "../../../src/sweep/sweep_dsp.hpp"
#include "../../../src/sweep/sweep_display.hpp"

#include <array>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <spawn.h>
extern char **environ;
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace {
enum : int {
    kBand = 0, kPeak = 31, kRaw = 62, kLz, kLa, kOrientation, kMic, kCalibration,
    kSensitivity = 68, kBand16 = 69, kPeak16 = 85, kGain = 101, kFloor, kSmoothing, kAverage, kPeakEnable, kPeakReset,
    kAttack, kFall, kPeakHold, kPeakFall,
    kServiceMode, kServiceStart, kServiceStop, kServiceState,
    kSweepStart, kSweepCancel, kSweepState, kSweepDuration, kSweepStartHz, kSweepStopHz, kSweepLevel, kSweepSmoothing,
    kSweepMeasurement, kSweepResponse0, kSweepSmoothDb0=kSweepResponse0+31,
    kSweepRawDb0=kSweepSmoothDb0+31, kSweepResultStatus=kSweepRawDb0+31,
    kSweepGraphPeak0=kSweepResultStatus+1, kSweepDiagnostic=kSweepGraphPeak0+31,
    kSweepDisplayRange=kSweepDiagnostic+1, kSweepDisplayCenter, kSweepDisplayMinimum,
    kSweepDisplayMaximum, kSweepGraphTick0, kSweepGraphTickCount=5,
    kSweepGraphSegment0=kSweepGraphTick0+kSweepGraphTickCount, kSweepGraphSegmentCount=255,
    kSweepClipState0=kSweepGraphSegment0+kSweepGraphSegmentCount,
    kSweepActualSmoothDb0=kSweepClipState0+31, kSweepActualRawDb0=kSweepActualSmoothDb0+31,
    kParamCount=kSweepActualRawDb0+31
};
constexpr int kUid = MPC_FOURCC('T', 'f', 'R', 'A');
enum ServiceState : int { SERVICE_STOPPED, SERVICE_STARTING, SERVICE_ONLINE, SERVICE_STOPPING, SERVICE_ERROR, SERVICE_EXTERNAL };
enum SweepUiState : int { SWEEP_IDLE, SWEEP_STARTING, SWEEP_ARMED, SWEEP_CAPTURING, SWEEP_PROCESSING, SWEEP_READY, SWEEP_CANCELLED, SWEEP_ERROR };
enum SweepResultState : int { RESULT_NONE, RESULT_LOADING, RESULT_READY, RESULT_INVALID };
static void servicePathAnchor() {}

bool writeAll(int fd, const char* p, size_t n) {
    while (n) {
        const ssize_t w = ::send(fd, p, n, MSG_NOSIGNAL);
        if (w <= 0) return false;
        p += w; n -= static_cast<size_t>(w);
    }
    return true;
}

bool request(const char* path, const char* method, std::string& body, size_t maxBody = 32768, int timeoutMs = 400) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    timeval tv{static_cast<time_t>(timeoutMs/1000),static_cast<suseconds_t>((timeoutMs%1000)*1000)};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    const int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    int port=8096;
    if (const char* setting=std::getenv("TROLLFX_RTA_PORT")) { char* end=nullptr; long v=std::strtol(setting,&end,10); if (end!=setting && v>0 && v<65536) port=static_cast<int>(v); }
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int r = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (r < 0 && errno != EINPROGRESS) { close(fd); return false; }
    if (r < 0) {
        fd_set f; FD_ZERO(&f); FD_SET(fd, &f); timeval wait{0, 160000};
        if (select(fd + 1, nullptr, &f, nullptr, &wait) <= 0) { close(fd); return false; }
        int err = 0; socklen_t len = sizeof(err);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) || err) { close(fd); return false; }
    }
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    char header[256];
    const int n = std::snprintf(header, sizeof(header), "%s %s HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n", method, path);
    if (n <= 0 || !writeAll(fd, header, static_cast<size_t>(n))) { close(fd); return false; }
    char buf[16384]; std::string response; response.reserve(std::min<size_t>(maxBody+1024,32768));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeoutMs);
    const size_t maxResponse=maxBody+1024;
    while (response.size() < maxResponse) {
        const auto remaining=std::chrono::duration_cast<std::chrono::microseconds>(deadline-std::chrono::steady_clock::now()).count();
        if (remaining<=0) break;
        fd_set readable; FD_ZERO(&readable); FD_SET(fd,&readable);
        timeval wait{static_cast<time_t>(remaining/1000000),static_cast<suseconds_t>(remaining%1000000)};
        if (select(fd+1,&readable,nullptr,nullptr,&wait)<=0) break;
        const size_t room=maxResponse-response.size();
        const ssize_t got = ::recv(fd, buf, std::min(sizeof(buf),room), 0);
        if (got <= 0) break;
        response.append(buf, static_cast<size_t>(got));
    }
    close(fd);
    const size_t split = response.find("\r\n\r\n");
    const size_t statusSpace=response.find(' ');
    if (split == std::string::npos || response.compare(0,5,"HTTP/")!=0 || statusSpace==std::string::npos ||
        response.compare(statusSpace+1,3,"200")!=0) return false;
    body.assign(response.data() + split + 4, response.size() - split - 4);
    return true;
}

class TrollfxRta;
class SharedClient {
public:
    static SharedClient& instance() { static SharedClient c; return c; }
    ~SharedClient() {
        std::thread join;
        { std::lock_guard<std::mutex> lock(mutex_); stop_=true; cv_.notify_all(); if(thread_.joinable()) join=std::move(thread_); }
        if(join.joinable()) join.join();
    }
    void add(TrollfxRta* p);
    void remove(TrollfxRta* p);
    void queueReset() { reset_.store(true, std::memory_order_release); }
    void queueAverage(int index) { averageIndex_.store(mpc::clamp(index,0,4), std::memory_order_release); }
    void command(int command) { command_.store(command, std::memory_order_release); cv_.notify_all(); }
    void queueServiceMode(int mode) { desiredMode_.store(mpc::clamp(mode,0,1),std::memory_order_release); modePending_.store(true,std::memory_order_release); cv_.notify_all(); }
private:
    void run();
    std::mutex lifecycleMutex_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<TrollfxRta*> clients_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<int> averageIndex_{0};
    std::atomic<bool> reset_{false};
    std::atomic<int> command_{0};
    std::atomic<int> desiredMode_{0};
    std::atomic<bool> modePending_{false};
    std::atomic<int> serviceState_{SERVICE_STOPPED};
    pid_t childPid_ = -1;
    int lockFd_ = -1;
    std::chrono::steady_clock::time_point childStartedAt_{};
    std::string executable_;
    std::string temporaryExecutable_;
    std::atomic<int> autoSuppressed_{0};
};

class TrollfxRta final : public mpc::Plugin {
public:
    TrollfxRta() : mpc::Plugin(makeInfo()) {
        static const char* bands[31] = {"20","25","31","40","50","63","80","100","125","160","200","250","315","400","500","630","800","1k","1k2","1k6","2k","2k5","3k1","4k","5k","6k3","8k","10k","12k","16k","20k"};
        for (int i=0;i<31;i++) { auto p=mpc::floatParam((std::string("band_")+std::to_string(i)).c_str(), bands[i], 0, 24, 0, "LED", 0); p.automatable=false; addParam(p); }
        for (int i=0;i<31;i++) { auto p=mpc::floatParam((std::string("peak_")+std::to_string(i)).c_str(), (std::string("P")+bands[i]).c_str(), 0, 24, 0, "LED", 0); p.automatable=false; addParam(p); }
        addParam(mpc::floatParam("raw_dbfs", "Raw dBFS", -120, 0, -120, "dBFS", 1));
        addParam(mpc::floatParam("spl_lz", "LZ SPL", -120, 140, -120, "dB", 1));
        addParam(mpc::floatParam("spl_la", "LA SPL", -120, 140, -120, "dB", 1));
        auto orientation=mpc::choiceParam("orientation", "Mic Angle", {"0 deg", "90 deg"}, 0); orientation.automatable=false; addParam(orientation);
        auto mic=mpc::boolParam("mic_live", "Mic Live", false); mic.automatable=false; addParam(mic);
        auto calibration=mpc::choiceParam("spl_status", "SPL Status", {"Provisional", "Verified", "User Ref"}, 0); calibration.automatable=false; addParam(calibration);
        auto sensitivity=mpc::floatParam("mic_sens_factor", "Sens Factor", -20, 20, 2.7673f, "dB", 4); sensitivity.automatable=false; addParam(sensitivity);
        for (int i=0;i<16;i++) { auto p=mpc::floatParam((std::string("band16_")+std::to_string(i)).c_str(), (std::string("16-")+std::to_string(i+1)).c_str(), 0, 24, 0, "LED", 0); p.automatable=false; addParam(p); }
        for (int i=0;i<16;i++) { auto p=mpc::floatParam((std::string("peak16_")+std::to_string(i)).c_str(), (std::string("P16-")+std::to_string(i+1)).c_str(), 0, 24, 0, "LED", 0); p.automatable=false; addParam(p); }
        addParam(mpc::floatParam("display_gain", "Display Gain", -12, 36, 11, "dB", 1));
        addParam(mpc::floatParam("display_floor", "Display Floor", -120, -40, -90, "dBFS", 0));
        addParam(mpc::floatParam("display_smooth", "Smoothing", 0, 500, 0, "ms", 0));
        addParam(mpc::choiceParam("analysis_avg", "Analysis Avg", {"1 frame","2 frames","4 frames","8 frames","16 frames"}, 0));
        addParam(mpc::boolParam("peak_enable", "Hold Enable", true));
        addParam(mpc::boolParam("peak_reset", "Reset Peaks", false));
        addParam(mpc::floatParam("led_attack", "LED Attack", 0, 400, 50, "ms", 0));
        addParam(mpc::floatParam("led_fall", "LED Fall", 2, 60, 33, "dB/s", 0));
        addParam(mpc::floatParam("peak_hold", "Peak Time", 0, 3000, 200, "ms", 0));
        addParam(mpc::floatParam("peak_fall", "Peak Fall", 2, 60, 60, "dB/s", 0));
        addParam(mpc::choiceParam("service_mode", "Service Mode", {"AUTO", "MANUAL"}, 0));
        auto start=mpc::boolParam("analyzer_start", "Analyzer START", false); start.automatable=false; addParam(start);
        auto stop=mpc::boolParam("analyzer_stop", "Analyzer STOP", false); stop.automatable=false; addParam(stop);
        auto state=mpc::choiceParam("service_status", "Service Status", {"STOPPED", "STARTING", "ONLINE", "STOPPING", "ERROR", "EXTERNAL"}, 0); state.automatable=false; addParam(state);
        auto sweepStart=mpc::boolParam("sweep_start", "SWEEP START", false);sweepStart.automatable=false;addParam(sweepStart);
        auto sweepCancel=mpc::boolParam("sweep_cancel", "SWEEP CANCEL", false);sweepCancel.automatable=false;addParam(sweepCancel);
        auto sweepState=mpc::choiceParam("sweep_status","Sweep Status",{"IDLE","STARTING","ARMED","CAPTURING","PROCESSING","READY","CANCELLED","ERROR"},0);sweepState.automatable=false;addParam(sweepState);
        addParam(mpc::floatParam("sweep_duration","Sweep Duration",0.5f,20.0f,10.0f,"s",1));
        addParam(mpc::floatParam("sweep_start_hz","Sweep Start Hz",20.0f,1000.0f,20.0f,"Hz",0));
        addParam(mpc::floatParam("sweep_stop_hz","Sweep Stop Hz",1000.0f,20000.0f,20000.0f,"Hz",0));
        addParam(mpc::floatParam("sweep_level","Sweep Peak",-60.0f,-12.0f,-30.0f,"dBFS",0));
        addParam(mpc::choiceParam("sweep_smoothing","Sweep Smoothing",{"None","1/48 octave","1/24 octave","1/12 octave","1/6 octave","1/3 octave"},4));
        addParam(mpc::choiceParam("sweep_measurement","Saved Measurement",{"Latest","Previous 2","Previous 3","Previous 4","Previous 5","Previous 6","Previous 7","Previous 8"},0));
        static const char* responseNames[31]={"R20","R25","R31","R40","R50","R63","R80","R100","R125","R160","R200","R250","R315","R400","R500","R630","R800","R1k","R1k2","R1k6","R2k","R2k5","R3k1","R4k","R5k","R6k3","R8k","R10k","R12k","R16k","R20k"};
        for(int i=0;i<31;i++){auto p=mpc::floatParam((std::string("sweep_response_")+std::to_string(i)).c_str(),responseNames[i],0,24,0,"LED",0);p.automatable=false;addParam(p);}
        static const char* plotNames[31]={"P20","P25","P31","P40","P50","P63","P80","P100","P125","P160","P200","P250","P315","P400","P500","P630","P800","P1k","P1k2","P1k6","P2k","P2k5","P3k1","P4k","P5k","P6k3","P8k","P10k","P12k","P16k","P20k"};
        for(int i=0;i<31;i++){auto p=mpc::floatParam((std::string("sweep_smooth_db_")+std::to_string(i)).c_str(),(std::string("S")+plotNames[i]).c_str(),-30,6,-30,"dB",1);p.automatable=false;addParam(p);}
        for(int i=0;i<31;i++){auto p=mpc::floatParam((std::string("sweep_raw_db_")+std::to_string(i)).c_str(),(std::string("Raw")+plotNames[i]).c_str(),-30,6,-30,"dB",1);p.automatable=false;addParam(p);}
        auto resultStatus=mpc::choiceParam("sweep_result_status","Result Status",{"NO MEASUREMENT","LOADING","READY","INVALID"},0);resultStatus.automatable=false;addParam(resultStatus);
        static const char* graphPeakNames[31]={"GP20","GP25","GP31","GP40","GP50","GP63","GP80","GP100","GP125","GP160","GP200","GP250","GP315","GP400","GP500","GP630","GP800","GP1k","GP1k2","GP1k6","GP2k","GP2k5","GP3k1","GP4k","GP5k","GP6k3","GP8k","GP10k","GP12k","GP16k","GP20k"};
        for(int i=0;i<31;i++){auto p=mpc::floatParam((std::string("sweep_graph_peak_")+std::to_string(i)).c_str(),graphPeakNames[i],0,24,0,"LED",0);p.automatable=false;addParam(p);}
        auto diagnostic=mpc::choiceParam("sweep_diagnostic","Sweep Diagnostic",{"OK","LEGACY CSV AXIS REBUILT","CLIPPED INPUT","SWEEP FILE MISSING","CSV / RESPONSE INVALID","EXPORT WRITE FAILED","CALIBRATION FAILED","CAPTURE / API FAILED","PROCESSING FAILED"},0);diagnostic.automatable=false;addParam(diagnostic);
        auto range=mpc::choiceParam("sweep_display_range","Display Range",{"24 dB","36 dB","48 dB","60 dB","72 dB"},2);range.automatable=false;addParam(range);
        auto center=mpc::intParam("sweep_display_center","Display Center",-60,60,-12,"dB rel 1k");center.automatable=false;addParam(center);
        auto minimum=mpc::floatParam("sweep_display_minimum","Window Low",-120,120,-36,"dB rel 1k",1);minimum.automatable=false;addParam(minimum);
        auto maximum=mpc::floatParam("sweep_display_maximum","Window High",-120,120,12,"dB rel 1k",1);maximum.automatable=false;addParam(maximum);
        for(int i=0;i<kSweepGraphTickCount;i++){
            auto p=mpc::floatParam((std::string("sweep_graph_tick_")+std::to_string(i)).c_str(),(std::string("T")+std::to_string(i)).c_str(),-120,120,0,"dB",0);p.automatable=false;addParam(p);
        }
        for(int i=0;i<kSweepGraphSegmentCount;i++){
            auto p=mpc::intParam((std::string("sweep_graph_segment_")+std::to_string(i)).c_str(),(std::string("G")+std::to_string(i)).c_str(),0,static_cast<int>(trollfx::sweep::kSweepGraphMissingFrame),static_cast<int>(trollfx::sweep::kSweepGraphMissingFrame));p.automatable=false;addParam(p);
        }
        for(int i=0;i<31;i++){
            auto p=mpc::intParam((std::string("sweep_clip_state_")+std::to_string(i)).c_str(),(std::string("C")+std::to_string(i)).c_str(),0,3,3);p.automatable=false;addParam(p);
        }
        for(int i=0;i<31;i++){
            auto p=mpc::floatParam((std::string("sweep_actual_smooth_db_")+std::to_string(i)).c_str(),(std::string("Actual S")+std::to_string(i)).c_str(),-240,120,-120,"dB rel 1k",2);p.automatable=false;addParam(p);
        }
        for(int i=0;i<31;i++){
            auto p=mpc::floatParam((std::string("sweep_actual_raw_db_")+std::to_string(i)).c_str(),(std::string("Actual R")+std::to_string(i)).c_str(),-240,120,-120,"dB rel 1k",2);p.automatable=false;addParam(p);
        }
        addProgram("RTA", defaults());
        SharedClient::instance().add(this);
        sweepThread_=std::thread(&TrollfxRta::sweepWorker,this);
        reprocessRequested_.store(true,std::memory_order_release);sweepCv_.notify_one();
    }
    ~TrollfxRta() override { sweepShutdown_.store(true,std::memory_order_release);sweepCancel_.store(true,std::memory_order_release);sweepCv_.notify_all();if(sweepThread_.joinable())sweepThread_.join();SharedClient::instance().remove(this); }

    void prepare(double sampleRate, int) override {
        if (std::isfinite(sampleRate) && sampleRate >= 32000.0 && sampleRate <= 96000.0)
            hostSampleRate_.store(static_cast<unsigned>(std::lround(sampleRate)),std::memory_order_release);
    }

    void process(mpc::ProcessContext& ctx) override {
        const int channels = mpc::clamp(ctx.numOutputs, 0, 2);
        audioReaders_.fetch_add(1,std::memory_order_acquire);
        const bool playing=sweepActive_.load(std::memory_order_acquire);
        const float* sweep=playing?sweepData_.load(std::memory_order_acquire):nullptr;
        const size_t sweepSize=playing?sweepSize_.load(std::memory_order_acquire):0;
        if(playing&&sweep&&sweepSize){
            size_t index=sweepIndex_;
            for(int frame=0;frame<ctx.numFrames;frame++){
                if(audioCancelFadeRequested_.exchange(false,std::memory_order_acq_rel)){
                    audioCancelFadeStart_=lastSweepOutputAudio_;
                    audioCancelFadeRemaining_=480;
                }
                if(audioCancelFadeRemaining_>0){
                    const float fade=static_cast<float>(audioCancelFadeRemaining_-1)/479.0f;
                    const float value=audioCancelFadeStart_*fade;
                    for(int ch=0;ch<channels;ch++)if(ctx.outputs&&ctx.outputs[ch])ctx.outputs[ch][frame]=value;
                    --audioCancelFadeRemaining_;
                    if(audioCancelFadeRemaining_==0){
                        sweepActive_.store(false,std::memory_order_release);
                        sweepAudioFinished_.store(true,std::memory_order_release);
                        for(int rest=frame+1;rest<ctx.numFrames;rest++)for(int ch=0;ch<channels;ch++)if(ctx.outputs&&ctx.outputs[ch]&&ctx.inputs&&ch<ctx.numInputs&&ctx.inputs[ch])ctx.outputs[ch][rest]=ctx.inputs[ch][rest];
                        break;
                    }
                    continue;
                }
                if(index>=sweepSize){
                    sweepActive_.store(false,std::memory_order_release);sweepAudioFinished_.store(true,std::memory_order_release);
                    for(int rest=frame;rest<ctx.numFrames;rest++)for(int ch=0;ch<channels;ch++)if(ctx.outputs&&ctx.outputs[ch]&&ctx.inputs&&ch<ctx.numInputs&&ctx.inputs[ch])ctx.outputs[ch][rest]=ctx.inputs[ch][rest];
                    break;
                }
                const float value=sweep[index++];lastSweepOutputAudio_=value;for(int ch=0;ch<channels;ch++)if(ctx.outputs&&ctx.outputs[ch])ctx.outputs[ch][frame]=value;
                if(index>=sweepSize){sweepActive_.store(false,std::memory_order_release);sweepAudioFinished_.store(true,std::memory_order_release);for(int rest=frame+1;rest<ctx.numFrames;rest++)for(int ch=0;ch<channels;ch++)if(ctx.outputs&&ctx.outputs[ch]&&ctx.inputs&&ch<ctx.numInputs&&ctx.inputs[ch])ctx.outputs[ch][rest]=ctx.inputs[ch][rest];break;}
            }
            sweepIndex_=index;
            audioReaders_.fetch_sub(1,std::memory_order_release);return;
        }
        for (int ch=0; ch<channels; ++ch) {
            if (!ctx.outputs || !ctx.outputs[ch] || !ctx.inputs || ch >= ctx.numInputs || !ctx.inputs[ch] || ctx.outputs[ch] == ctx.inputs[ch]) continue;
            std::memcpy(ctx.outputs[ch], ctx.inputs[ch], static_cast<size_t>(ctx.numFrames) * sizeof(float));
        }
        audioReaders_.fetch_sub(1,std::memory_order_release);
    }
    void parameterChanged(int index, float value) override {
        if (index == kPeakReset && value >= 0.5f) SharedClient::instance().queueReset();
        if (index == kAverage) SharedClient::instance().queueAverage(static_cast<int>(std::lround(value)));
        if (index == kServiceMode) SharedClient::instance().queueServiceMode(static_cast<int>(std::lround(value)));
        if (index == kServiceStart && value >= 0.5f) { SharedClient::instance().command(1); set(kServiceStart,0.0f); }
        if (index == kServiceStop && value >= 0.5f) { SharedClient::instance().command(2); set(kServiceStop,0.0f); }
        if (index == kSweepStart && value>=0.5f) { set(kSweepStart,0.0f);if(!sweepRunning_.load(std::memory_order_acquire))sweepStart_.store(true,std::memory_order_release);sweepCv_.notify_one(); }
        if (index == kSweepCancel && value>=0.5f) { if(sweepActive_.load(std::memory_order_acquire))audioCancelFadeRequested_.store(true,std::memory_order_release);set(kSweepCancel,0.0f);sweepCancel_.store(true,std::memory_order_release);sweepCv_.notify_one(); }
        if (index == kSweepSmoothing) { reprocessRequested_.store(true,std::memory_order_release);sweepCv_.notify_one(); }
        if (index == kSweepMeasurement) { reprocessRequested_.store(true,std::memory_order_release);sweepCv_.notify_one(); }
        if (index == kSweepDisplayRange || index == kSweepDisplayCenter) { displayRefreshRequested_.store(true,std::memory_order_release);sweepCv_.notify_one(); }
    }
    bool formatValue(int index, float v, std::string& out) override {
        if (index == kOrientation) { out = v < 0.5f ? "0 deg" : "90 deg"; return true; }
        if ((index == kLz || index == kLa) && get(kCalibration) < 0.5f) { out = "N/A"; return true; }
        if (index == kCalibration) {
            out = v < 0.5f ? "PROVISIONAL" : (v < 1.5f ? "VERIFIED" : "USER REF"); return true;
        }
        if (index == kMic) { out = v >= 0.5f ? "ONLINE" : "OFFLINE"; return true; }
        if (index == kServiceState) { static const char* names[]={"STOPPED","STARTING","ONLINE","STOPPING","ERROR","EXTERNAL"}; int n=mpc::clamp(static_cast<int>(v),0,5); out=names[n]; return true; }
        if (index == kSweepState) { static const char* names[]={"IDLE","STARTING","ARMED","CAPTURING","PROCESSING","READY","CANCELLED","ERROR"};int n=mpc::clamp(static_cast<int>(v),0,7);out=names[n];return true; }
        if (index == kSweepResultStatus) { static const char* names[]={"NO MEASUREMENT","LOADING","READY","INVALID"};int n=mpc::clamp(static_cast<int>(v),0,3);out=names[n];return true; }
        if (index == kSweepDiagnostic) { static const char* names[]={"OK","LEGACY CSV AXIS REBUILT","CLIPPED INPUT","SWEEP FILE MISSING","CSV / RESPONSE INVALID","EXPORT WRITE FAILED","CALIBRATION FAILED","CAPTURE / API FAILED","PROCESSING FAILED"};int n=mpc::clamp(static_cast<int>(v),0,8);out=names[n];return true; }
        if (index == kSweepDisplayRange) { static const char* names[]={"24 dB","36 dB","48 dB","60 dB","72 dB"};int n=mpc::clamp(static_cast<int>(v),0,4);out=names[n];return true; }
        return false;
    }
    void update(const trollfx::Snapshot& s) {
        set(kPeakReset, 0.0f);
        if (!s.ok) { offline(); return; }
        const auto now=std::chrono::steady_clock::now();
        const double dt=lastUpdate_.time_since_epoch().count()==0 ? 0.2 : std::chrono::duration<double>(now-lastUpdate_).count();
        lastUpdate_=now;
        const float gain=get(kGain), floorDb=get(kFloor), smooth=get(kSmoothing);
        const float attack=get(kAttack)/1000.0f, fall=get(kFall), holdSeconds=get(kPeakHold)/1000.0f, peakFall=get(kPeakFall);
        std::array<float,31> displayed{};
        for (int i=0;i<31;i++) {
            const float target=ledValue(s.level[i]+gain, floorDb);
            const float coeff=smooth<=0 ? 1.0f : static_cast<float>(1.0-std::exp(-dt/(smooth/1000.0f)));
            smoothState_[i]+=coeff*(target-smoothState_[i]);
            const float shown=smoothState_[i];
            if (shown>meterState_[i]) {
                const float a=attack<=0 ? 1.0f : static_cast<float>(1.0-std::exp(-dt/attack));
                meterState_[i]+=a*(shown-meterState_[i]);
            } else meterState_[i]=std::max(shown,meterState_[i]-static_cast<float>(fall*dt*24.0/(-floorDb)));
            displayed[i]=meterState_[i];
            const float sourcePeak=ledValue(s.peak[i]+gain,floorDb);
            if (sourcePeak < sourcePeakState_[i]) { peakState_[i]=displayed[i]; peakTime_[i]=now; }
            if (sourcePeak > sourcePeakState_[i] || displayed[i] >= peakState_[i]) peakState_[i]=std::max(sourcePeak,displayed[i]);
            if (sourcePeak > sourcePeakState_[i] || displayed[i] >= peakState_[i]) peakTime_[i]=now;
            sourcePeakState_[i]=sourcePeak;
            if (std::chrono::duration<double>(now-peakTime_[i]).count()>holdSeconds)
                peakState_[i]=std::max(displayed[i],peakState_[i]-static_cast<float>(peakFall*dt*24.0/(-floorDb)));
            set(kBand+i, displayed[i]); set(kPeak+i, getBool(kPeakEnable) ? peakState_[i] : 0.0f);
        }
        std::array<float,16> levels16{}, sourcePeaks16{};
        trollfx::combine16Power(s.level, levels16); trollfx::combine16Power(s.peak, sourcePeaks16);
        for (int i=0;i<16;i++) {
            const float target=ledValue(levels16[i]+gain,floorDb);
            const float coeff=smooth<=0 ? 1.0f : static_cast<float>(1.0-std::exp(-dt/(smooth/1000.0f)));
            smoothState16_[i]+=coeff*(target-smoothState16_[i]);
            const float shown=smoothState16_[i];
            if (shown>meterState16_[i]) {
                const float a=attack<=0 ? 1.0f : static_cast<float>(1.0-std::exp(-dt/attack));
                meterState16_[i]+=a*(shown-meterState16_[i]);
            } else meterState16_[i]=std::max(shown,meterState16_[i]-static_cast<float>(fall*dt*24.0/(-floorDb)));
            const float source=ledValue(sourcePeaks16[i]+gain,floorDb);
            if (source<sourcePeakState16_[i]) { peakState16_[i]=meterState16_[i]; peakTime16_[i]=now; }
            const bool newPeak=source>sourcePeakState16_[i] || meterState16_[i]>=peakState16_[i];
            if (newPeak) { peakState16_[i]=std::max(source,meterState16_[i]); peakTime16_[i]=now; }
            sourcePeakState16_[i]=source;
            if (!newPeak && std::chrono::duration<double>(now-peakTime16_[i]).count()>holdSeconds)
                peakState16_[i]=std::max(meterState16_[i],peakState16_[i]-static_cast<float>(peakFall*dt*24.0/(-floorDb)));
            set(kBand16+i,meterState16_[i]); set(kPeak16+i,getBool(kPeakEnable)?peakState16_[i]:0.0f);
        }
        set(kRaw, s.rawDbfs); set(kLz, s.splZ); set(kLa, s.splA);
        set(kSensitivity,s.sensitivityFactorDb);
        set(kOrientation, s.orientation == 90 ? 1.0f : 0.0f); set(kMic, 1.0f);
        set(kCalibration, s.calibrated ? 1.0f : s.referenceType == 2 ? 2.0f : 0.0f);
        requestDisplayUpdate();
    }
    int serviceMode() const { return get(kServiceMode) >= 0.5f ? 1 : 0; }
    void offline() {
        if (getBool(kMic)) { set(kMic,0.0f); requestDisplayUpdate(); }
    }
    void setServiceState(int state) { set(kServiceState,static_cast<float>(state)); if(state!=SERVICE_ONLINE) offline(); }
    void resetPeaks() {
        const auto now=std::chrono::steady_clock::now();
        for(int i=0;i<31;i++){peakState_[i]=meterState_[i];sourcePeakState_[i]=0;peakTime_[i]=now;}
        for(int i=0;i<16;i++){peakState16_[i]=meterState16_[i];sourcePeakState16_[i]=0;peakTime16_[i]=now;}
    }
    static std::vector<float> defaults() {
        std::vector<float> v(kParamCount, 0.0f);
        for (int i=0;i<62;i++) v[i]=0.0f;
        v[kRaw]=-120; v[kLz]=-120; v[kLa]=-120; v[kOrientation]=0; v[kMic]=0; v[kCalibration]=0;
        v[kSensitivity]=2.7673f; v[kGain]=11; v[kFloor]=-90; v[kSmoothing]=0; v[kAverage]=0; v[kPeakEnable]=1; v[kPeakReset]=0;
        v[kAttack]=50; v[kFall]=33; v[kPeakHold]=200; v[kPeakFall]=60;
        v[kServiceMode]=0; v[kServiceStart]=0; v[kServiceStop]=0; v[kServiceState]=SERVICE_STOPPED;
        v[kSweepStart]=0;v[kSweepCancel]=0;v[kSweepState]=SWEEP_IDLE;v[kSweepDuration]=10;v[kSweepStartHz]=20;v[kSweepStopHz]=20000;v[kSweepLevel]=-30;v[kSweepSmoothing]=4;v[kSweepMeasurement]=0;
        for(int i=0;i<31;i++)v[kSweepResponse0+i]=0;
        for(int i=0;i<31;i++){v[kSweepSmoothDb0+i]=-30;v[kSweepRawDb0+i]=-30;}
        v[kSweepResultStatus]=RESULT_NONE;
        for(int i=0;i<31;i++)v[kSweepGraphPeak0+i]=0;
        v[kSweepDiagnostic]=0;
        v[kSweepDisplayRange]=2;v[kSweepDisplayCenter]=-12;v[kSweepDisplayMinimum]=-36;v[kSweepDisplayMaximum]=12;
        for(int i=0;i<kSweepGraphTickCount;i++)v[kSweepGraphTick0+i]=12.0f-i*12.0f;
        for(int i=0;i<kSweepGraphSegmentCount;i++)v[kSweepGraphSegment0+i]=static_cast<float>(trollfx::sweep::kSweepGraphMissingFrame);
        for(int i=0;i<31;i++)v[kSweepClipState0+i]=3;
        for(int i=0;i<62;i++)v[kSweepActualSmoothDb0+i]=-120.0f;
        return v;
    }
private:
    void publishSweepState(int state){set(kSweepState,static_cast<float>(state));requestDisplayUpdate();}
    static std::string sessionRoot(){Dl_info info{};if(!dladdr(reinterpret_cast<void*>(&servicePathAnchor),&info)||!info.dli_fname)return {};char path[4096];if(!realpath(info.dli_fname,path))return {};std::string p(path);auto slash=p.find_last_of('/');if(slash==std::string::npos)return {};p.resize(slash);return p+"/../Service/sessions";}
    void sweepWorker(){
        auto sleep=[this](int ms){std::unique_lock<std::mutex> lock(sweepMutex_);sweepCv_.wait_for(lock,std::chrono::milliseconds(ms),[this]{return sweepShutdown_.load(std::memory_order_acquire)||sweepStart_.load(std::memory_order_acquire)||sweepCancel_.load(std::memory_order_acquire)||reprocessRequested_.load(std::memory_order_acquire)||displayRefreshRequested_.load(std::memory_order_acquire);});};
        while(!sweepShutdown_.load(std::memory_order_acquire)){
            if(sweepCancel_.exchange(false,std::memory_order_acq_rel)){std::string ignored;request("/api/sweep/cancel","POST",ignored);publishSweepState(SWEEP_CANCELLED);}
            if(!sweepRunning_.load(std::memory_order_acquire)&&reprocessRequested_.exchange(false,std::memory_order_acq_rel)){
                set(kSweepResultStatus,RESULT_LOADING);requestDisplayUpdate();
                const auto root=sessionRoot();std::error_code ec;std::filesystem::path latest;
                std::vector<std::filesystem::path> sessions;
                if(!root.empty()&&std::filesystem::exists(root,ec))for(const auto& entry:std::filesystem::directory_iterator(root,ec))if(entry.is_directory()&&entry.path().filename().string().rfind("sweep-",0)==0&&std::filesystem::exists(entry.path()/"frequency-response.csv"))sessions.push_back(entry.path());
                std::sort(sessions.begin(),sessions.end(),[](const auto& a,const auto& b){return a.filename().string()>b.filename().string();});
                const int selected=mpc::clamp(static_cast<int>(std::lround(get(kSweepMeasurement))),0,7);if(static_cast<std::size_t>(selected)<sessions.size())latest=sessions[selected];
                static const unsigned resolutions[]={0,48,24,12,6,3};const int choice=mpc::clamp(static_cast<int>(std::lround(get(kSweepSmoothing))),0,5);std::string error;
                if(latest.empty()){clearSweepResult(RESULT_NONE);set(kSweepDiagnostic,3.0f);}
                else if(!trollfx::sweep::reprocessSession(latest.string(),resolutions[choice],1000.0,true,&error)){clearSweepResult(RESULT_INVALID);setSweepDiagnostic(error);}
                else if(!updateSweepGraph(latest.string(),resolutions[choice],&error)){clearSweepResult(RESULT_INVALID);setSweepDiagnostic(error);}
                requestDisplayUpdate();
            }
            if(!sweepRunning_.load(std::memory_order_acquire)&&displayRefreshRequested_.exchange(false,std::memory_order_acq_rel)){
                const auto root=sessionRoot();std::error_code ec;std::vector<std::filesystem::path> sessions;
                if(!root.empty()&&std::filesystem::exists(root,ec))for(const auto& entry:std::filesystem::directory_iterator(root,ec))if(entry.is_directory()&&entry.path().filename().string().rfind("sweep-",0)==0&&std::filesystem::exists(entry.path()/"frequency-response.csv"))sessions.push_back(entry.path());
                std::sort(sessions.begin(),sessions.end(),[](const auto& a,const auto& b){return a.filename().string()>b.filename().string();});
                const int selected=mpc::clamp(static_cast<int>(std::lround(get(kSweepMeasurement))),0,7);static const unsigned resolutions[]={0,48,24,12,6,3};const int smoothChoice=mpc::clamp(static_cast<int>(std::lround(get(kSweepSmoothing))),0,5);std::string error;
                if(static_cast<std::size_t>(selected)<sessions.size())updateSweepGraph(sessions[selected].string(),resolutions[smoothChoice],&error);
                requestDisplayUpdate();
            }
            if(!sweepStart_.exchange(false,std::memory_order_acq_rel)){sleep(50);continue;}
            if(sweepStateValue_!=SWEEP_IDLE&&sweepStateValue_!=SWEEP_READY&&sweepStateValue_!=SWEEP_CANCELLED&&sweepStateValue_!=SWEEP_ERROR){publishSweepState(SWEEP_ERROR);continue;}
            sweepRunning_.store(true,std::memory_order_release);auto failSweep=[this](const std::string& detail){std::string ignored;request("/api/sweep/cancel","POST",ignored);sweepStateValue_=SWEEP_ERROR;publishSweepState(SWEEP_ERROR);sweepRunning_.store(false,std::memory_order_release);setSweepDiagnostic(detail.empty()?"unspecified sweep processing failure":detail);};auto cancelSweep=[this]{sweepCancel_.store(false,std::memory_order_release);sweepStateValue_=SWEEP_CANCELLED;publishSweepState(SWEEP_CANCELLED);sweepRunning_.store(false,std::memory_order_release);};sweepStateValue_=SWEEP_STARTING;publishSweepState(SWEEP_STARTING);sweepCancel_.store(false,std::memory_order_release);
            const unsigned hostRate=hostSampleRate_.load(std::memory_order_acquire);
            trollfx::sweep::SweepOptions options;options.sampleRate=hostRate;options.durationSeconds=get(kSweepDuration);options.startHz=get(kSweepStartHz);options.stopHz=std::min<double>(get(kSweepStopHz),std::nextafter(0.45*hostRate,0.0));options.peakDbfs=get(kSweepLevel);options.fadeSeconds=0.01;
            std::string error;auto signal=trollfx::sweep::generateMeasurementSignal(options);if(signal.samples.empty()){failSweep(std::string{});continue;}
            std::vector<float> playback(signal.samples.size());for(std::size_t i=0;i<playback.size();i++)playback[i]=static_cast<float>(signal.samples[i]);
            std::vector<double> reference48;if(!trollfx::sweep::resampleMono(signal.samples,hostRate,48000,reference48,&error)){failSweep(std::string{});continue;}
            const auto root=sessionRoot();if(root.empty()){failSweep(std::string{});continue;}
            bool api=false;auto apiDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(12);
            while(!sweepShutdown_.load()&&!sweepCancel_.load()&&std::chrono::steady_clock::now()<apiDeadline){std::string probe;trollfx::Snapshot snapshot;if(request("/api/spectrum","GET",probe)&&trollfx::parseSpectrumJson(probe,snapshot)&&snapshot.ok){api=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(100));}
            if(!api||sweepShutdown_.load()||sweepCancel_.load()){if(sweepCancel_.load())cancelSweep();else failSweep("capture API unavailable or timed out");continue;}
            char path[96];const unsigned captureSeconds=static_cast<unsigned>(std::ceil(reference48.size()/48000.0+1.0));std::snprintf(path,sizeof(path),"/api/sweep/arm?seconds=%u",captureSeconds);std::string body;
            if(!request(path,"POST",body)||body.find("\"ok\":true")==std::string::npos){failSweep("capture API refused sweep arm");continue;}
            publishSweepState(SWEEP_ARMED);sweepStateValue_=SWEEP_ARMED;
            bool armed=false;auto armDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
            while(!sweepShutdown_.load()&&!sweepCancel_.load()&&std::chrono::steady_clock::now()<armDeadline){if(request("/api/sweep/status","GET",body)&&body.find("\"state\":\"CAPTURING\"")!=std::string::npos){armed=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(20));}
            if(!armed||sweepShutdown_.load()||sweepCancel_.load()){std::string ignored;request("/api/sweep/cancel","POST",ignored);if(sweepCancel_.load())cancelSweep();else failSweep(std::string{});continue;}
            while(audioReaders_.load(std::memory_order_acquire))std::this_thread::sleep_for(std::chrono::milliseconds(1));
            sweepBuffer_=std::move(playback);sweepIndex_=0;sweepAudioFinished_.store(false,std::memory_order_release);sweepData_.store(sweepBuffer_.data(),std::memory_order_release);sweepSize_.store(sweepBuffer_.size(),std::memory_order_release);sweepCancel_.store(false,std::memory_order_release);sweepActive_.store(true,std::memory_order_release);publishSweepState(SWEEP_CAPTURING);sweepStateValue_=SWEEP_CAPTURING;
            const auto playbackDeadline=std::chrono::steady_clock::now()+std::chrono::duration<double>(signal.samples.size()/48000.0+8.0);
            while(!sweepShutdown_.load()&&!sweepCancel_.load()&&!sweepAudioFinished_.load(std::memory_order_acquire)&&std::chrono::steady_clock::now()<playbackDeadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if(sweepCancel_.load()||sweepShutdown_.load()||!sweepAudioFinished_.load()){if(!sweepCancel_.load())sweepActive_.store(false,std::memory_order_release);std::string ignored;request("/api/sweep/cancel","POST",ignored);if(sweepCancel_.load())cancelSweep();else failSweep(std::string{});continue;}
            publishSweepState(SWEEP_PROCESSING);sweepStateValue_=SWEEP_PROCESSING;
            if(!request("/api/sweep/finish","POST",body,32768,3000)||body.find("\"state\":\"READY\"")==std::string::npos){failSweep(std::string{});continue;}
            if(!request("/api/sweep/capture.wav","GET",body,8*1024*1024,20000)||body.size()<44){failSweep(std::string{});continue;}
            std::error_code ec;std::filesystem::create_directories(root,ec);if(ec){failSweep(std::string{});continue;}
            const auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();const std::string folder=root+"/sweep-"+std::to_string(stamp);std::filesystem::create_directories(folder,ec);if(ec){failSweep(std::string{});continue;}
            {std::ofstream f(folder+"/capture.wav",std::ios::binary);f.write(body.data(),static_cast<std::streamsize>(body.size()));if(!f){failSweep(std::string{});continue;}}
            if(!trollfx::sweep::writeFloatWav(folder+"/reference.wav",reference48,48000,&error)){failSweep(error);continue;}
            std::vector<double> capture;unsigned captureFs=0;if(!trollfx::sweep::readWav(folder+"/capture.wav",capture,captureFs,&error)||captureFs!=48000){if(error.empty())error="unexpected capture sample rate";failSweep(error);continue;}
            const std::size_t markerStart48=static_cast<std::size_t>(std::llround(signal.markerStartSample*48000.0/hostRate));
            const std::size_t markerEnd48=static_cast<std::size_t>(std::llround(signal.markerEndSample*48000.0/hostRate));
            if(markerStart48>=reference48.size()||markerEnd48>reference48.size()||markerEnd48<=markerStart48){failSweep(std::string{});continue;}
            const std::size_t markerLength48=(signal.marker.size()*48000u+hostRate/2)/hostRate;
            if(markerLength48<2||markerStart48+markerLength48>reference48.size()){failSweep(std::string{});continue;}
            std::vector<double> marker48(reference48.begin()+markerStart48,reference48.begin()+markerStart48+markerLength48);
            trollfx::sweep::ClockAlignment alignment;if(!trollfx::sweep::locateClockMarkers(marker48,capture,markerEnd48-markerStart48,alignment,&error)){failSweep(error);continue;}
            options.sampleRate=48000;trollfx::sweep::AnalysisOptions analysis;analysis.sampleRate=48000;analysis.startHz=options.startHz;analysis.stopHz=options.stopHz;analysis.clockPpm=alignment.clockPpm;
            trollfx::sweep::AnalysisResult result;if(!trollfx::sweep::analyze(reference48,capture,analysis,result,&error)){failSweep(error);continue;}
            result.markerAlignmentValid=true;result.capturedFirstMarker=alignment.firstMarkerSample;result.capturedSecondMarker=alignment.secondMarkerSample;result.markerConfidence=std::min(alignment.firstConfidence,alignment.secondConfidence);
            const int orientation=get(kOrientation)>=0.5f?90:0;const std::string cal=root+"/../calibration/7033298"+(orientation==90?"_90deg":"")+".txt";trollfx::sweep::CalibrationCurve curve;
            if(!trollfx::sweep::readCalibration(cal,orientation,curve,&error)||!trollfx::sweep::applyCalibration(result,curve,&error)){failSweep(error);continue;}
            static const unsigned resolutions[]={0,48,24,12,6,3};const int smoothChoice=mpc::clamp(static_cast<int>(std::lround(get(kSweepSmoothing))),0,5);
            if(!trollfx::sweep::writeArtifacts(folder,result,48000,options.startHz,options.stopHz,&error,resolutions[smoothChoice])){failSweep(error);continue;}
            if(!updateSweepGraph(folder,resolutions[smoothChoice],&error)){clearSweepResult(RESULT_INVALID);setSweepDiagnostic(error);}
            std::filesystem::copy_file(cal,folder+"/calibration-used.txt",std::filesystem::copy_options::overwrite_existing,ec);
            sweepStateValue_=SWEEP_READY;publishSweepState(SWEEP_READY);sweepRunning_.store(false,std::memory_order_release);
        }
        sweepActive_.store(false,std::memory_order_release);sweepRunning_.store(false,std::memory_order_release);
    }
    static float ledValue(float db, float floorDb) {
        if (!std::isfinite(db)) return 0.0f;
        return mpc::clamp((db-floorDb)*24.0f/(-floorDb),0.0f,24.0f);
    }
    void setSweepDiagnostic(const std::string& error) {
        int code=8;
        if(error.find("clipped")!=std::string::npos)code=2;
        else if(error.find("cannot write")!=std::string::npos||error.find("failed writing")!=std::string::npos||error.find("cannot create")!=std::string::npos)code=5;
        else if(error.find("calibration")!=std::string::npos||error.find("sensitivity")!=std::string::npos)code=6;
        else if(error.find("capture")!=std::string::npos||error.find("API")!=std::string::npos||error.find("sweep arm")!=std::string::npos)code=7;
        else if(error.find("rounded")!=std::string::npos||error.find("legacy frequency")!=std::string::npos||error.find("frequency tokens")!=std::string::npos)code=4;
        else if(error.find("metadata")!=std::string::npos||error.find("impulse")!=std::string::npos||error.find("WAV")!=std::string::npos)code=3;
        else if(error.find("CSV")!=std::string::npos||error.find("response")!=std::string::npos||error.find("frequency")!=std::string::npos)code=4;
        set(kSweepDiagnostic,static_cast<float>(code));
    }
    bool updateSweepGraph(const std::string& folder,unsigned resolution,std::string* outError=nullptr){
        trollfx::sweep::ResponseCurve raw;std::string error;
        if(!trollfx::sweep::readResponseCsv(folder+"/frequency-response.csv",raw,&error)){if(outError)*outError=error;return false;}
        trollfx::sweep::SweepDisplayResponse display;
        static const double ranges[]={24.0,36.0,48.0,60.0,72.0};const int rangeIndex=mpc::clamp(static_cast<int>(std::lround(get(kSweepDisplayRange))),0,4);
        if(!trollfx::sweep::makeSweepDisplayResponse(raw,resolution,display,&error,ranges[rangeIndex],get(kSweepDisplayCenter))){if(outError)*outError=error;return false;}
        for(int i=0;i<31;i++)set(kSweepResponse0+i,display.led[static_cast<std::size_t>(i)]);
        for(int i=0;i<31;i++){set(kSweepSmoothDb0+i,static_cast<float>(std::isfinite(display.smoothedRelativeDb[static_cast<std::size_t>(i)])?display.smoothedRelativeDb[static_cast<std::size_t>(i)]:-120.0));set(kSweepRawDb0+i,static_cast<float>(std::isfinite(display.rawRelativeDb[static_cast<std::size_t>(i)])?display.rawRelativeDb[static_cast<std::size_t>(i)]:-120.0));set(kSweepGraphPeak0+i,display.led[static_cast<std::size_t>(i)]);set(kSweepClipState0+i,static_cast<float>(display.clipping[static_cast<std::size_t>(i)]));}
        for(int i=0;i<kSweepGraphSegmentCount;i++)set(kSweepGraphSegment0+i,static_cast<float>(display.graphSegmentFrame[static_cast<std::size_t>(i)]));
        set(kSweepDisplayMinimum,static_cast<float>(display.displayMinimumDb));set(kSweepDisplayMaximum,static_cast<float>(display.displayMaximumDb));
        for(int i=0;i<kSweepGraphTickCount;i++)set(kSweepGraphTick0+i,static_cast<float>(display.displayMaximumDb-display.displayRangeDb*i/(kSweepGraphTickCount-1)));
        for(int i=0;i<31;i++){
            const double smoothDb=display.smoothedRelativeDb[static_cast<std::size_t>(i)],rawDb=display.rawRelativeDb[static_cast<std::size_t>(i)];
            set(kSweepActualSmoothDb0+i,static_cast<float>(std::isfinite(smoothDb)?smoothDb:-120.0));
            set(kSweepActualRawDb0+i,static_cast<float>(std::isfinite(rawDb)?rawDb:-120.0));
        }
        set(kSweepDiagnostic,raw.frequencyAxisReconstructed?1.0f:0.0f);
        set(kSweepResultStatus,RESULT_READY);requestDisplayUpdate();return true;
    }
    void clearSweepResult(int state){
        for(int i=0;i<31;i++)set(kSweepResponse0+i,0.0f);
        for(int i=0;i<31;i++){set(kSweepSmoothDb0+i,-30.0f);set(kSweepRawDb0+i,-30.0f);}
        for(int i=0;i<31;i++)set(kSweepGraphPeak0+i,0.0f);
        for(int i=0;i<kSweepGraphSegmentCount;i++)set(kSweepGraphSegment0+i,static_cast<float>(trollfx::sweep::kSweepGraphMissingFrame));
        for(int i=0;i<31;i++)set(kSweepClipState0+i,3.0f);
        for(int i=0;i<62;i++)set(kSweepActualSmoothDb0+i,-120.0f);
        set(kSweepResultStatus,static_cast<float>(state));
    }
    static mpc::PluginInfo makeInfo() {
        mpc::PluginInfo i; i.name="TROLLfx - VST - RTA"; i.vendor="TROLLfx"; i.uniqueId=kUid;
        i.version=505; i.isInstrument=false; i.numInputs=2; i.numOutputs=2; i.tailSamples=1; return i;
    }
    std::array<float,31> smoothState_{};
    std::array<float,31> meterState_{};
    std::array<float,31> peakState_{};
    std::array<float,31> sourcePeakState_{};
    std::array<float,16> smoothState16_{};
    std::array<float,16> meterState16_{};
    std::array<float,16> peakState16_{};
    std::array<float,16> sourcePeakState16_{};
    std::array<std::chrono::steady_clock::time_point,31> peakTime_{};
    std::array<std::chrono::steady_clock::time_point,16> peakTime16_{};
    std::chrono::steady_clock::time_point lastUpdate_{};
    std::atomic<bool> sweepStart_{false},sweepCancel_{false},sweepShutdown_{false},sweepActive_{false},sweepAudioFinished_{false},sweepRunning_{false},reprocessRequested_{false},displayRefreshRequested_{false},audioCancelFadeRequested_{false};
    // Written/read only by the audio callback; CANCEL fades to zero over 10 ms
    // across as many host blocks as necessary, then restores normal pass-through.
    float lastSweepOutputAudio_=0.0f,audioCancelFadeStart_=0.0f;
    int audioCancelFadeRemaining_=0;
    std::atomic<unsigned> hostSampleRate_{48000};
    std::atomic<unsigned> audioReaders_{0};
    std::atomic<const float*> sweepData_{nullptr};
    std::atomic<size_t> sweepSize_{0};
    std::vector<float> sweepBuffer_;
    size_t sweepIndex_=0;
    int sweepStateValue_=SWEEP_IDLE;
    std::mutex sweepMutex_;
    std::condition_variable sweepCv_;
    std::thread sweepThread_;
};

void SharedClient::add(TrollfxRta* p) {
    std::lock_guard<std::mutex> lifecycle(lifecycleMutex_);
    std::lock_guard<std::mutex> lock(mutex_); clients_.push_back(p);
    if (!thread_.joinable()) { stop_=false; thread_=std::thread(&SharedClient::run,this); }
}
void SharedClient::remove(TrollfxRta* p) {
    std::lock_guard<std::mutex> lifecycle(lifecycleMutex_);
    std::thread join;
    { std::lock_guard<std::mutex> lock(mutex_); clients_.erase(std::remove(clients_.begin(),clients_.end(),p),clients_.end());
      if (clients_.empty() && thread_.joinable()) { stop_=true; cv_.notify_all(); join=std::move(thread_); autoSuppressed_.store(0,std::memory_order_release); } }
    if (join.joinable()) join.join();
}
void SharedClient::run() {
    std::unique_lock<std::mutex> lock(mutex_);
    int lastAverageSent=-1;
    int refreshHz=5;
    if (const char* setting=std::getenv("TROLLFX_RTA_REFRESH_HZ")) { char* end=nullptr; long n=std::strtol(setting,&end,10); if(end!=setting&&n>=1&&n<=10)refreshHz=static_cast<int>(n); }
    unsigned failedStarts=0; auto retryAt=std::chrono::steady_clock::time_point{};
    bool external=false, manualStart=false; int mode=-1;
    auto cleanupTemporaryExecutable=[this]() { if(!temporaryExecutable_.empty()){unlink(temporaryExecutable_.c_str());temporaryExecutable_.clear();} executable_.clear(); };
    auto copyExecutableToTmp=[this](const std::string& source) -> bool {
        int input=open(source.c_str(),O_RDONLY|O_CLOEXEC);if(input<0)return false;
        struct stat st{};if(fstat(input,&st)!=0||!S_ISREG(st.st_mode)||(st.st_mode&(S_IXUSR|S_IXGRP|S_IXOTH))==0){close(input);return false;}
        char path[]="/tmp/trollfx-rta-v02-armv7-XXXXXX";int output=mkstemp(path);if(output<0){close(input);return false;}
        bool ok=fchmod(output,0700)==0;char buffer[16384];
        while(ok){const ssize_t n=read(input,buffer,sizeof(buffer));if(n==0)break;if(n<0){if(errno==EINTR)continue;ok=false;break;}ssize_t written=0;while(written<n){const ssize_t w=write(output,buffer+written,static_cast<size_t>(n-written));if(w<0&&errno==EINTR)continue;if(w<=0){ok=false;break;}written+=w;}}
        if(ok&&fsync(output)!=0)ok=false;close(input);if(close(output)!=0)ok=false;
        if(!ok){unlink(path);return false;}temporaryExecutable_=path;executable_=path;return true;
    };
    auto bundledExecutable=[]() -> std::string {
        if (const char* overridePath=std::getenv("TROLLFX_RTA_SERVICE_EXECUTABLE")) return overridePath;
        Dl_info info{}; if (!dladdr(reinterpret_cast<void*>(&servicePathAnchor),&info) || !info.dli_fname) return {};
        char resolved[4096]; if (!realpath(info.dli_fname,resolved)) return {};
        std::string path(resolved); const size_t slash=path.find_last_of('/');
        if (slash==std::string::npos) return {};
        path.resize(slash); // VST directory
        return path+"/../Service/trollfx-rta-v02-armv7";
    };
    auto childMatches=[this]() -> bool {
        if (childPid_<=0) return false;
        char proc[64],exe[4096],expected[4096]; std::snprintf(proc,sizeof(proc),"/proc/%ld/exe",(long)childPid_);
        const ssize_t n=readlink(proc,exe,sizeof(exe)-1); if(n<=0)return false; exe[n]=0;
        char* deleted=std::strstr(exe," (deleted)"); if(deleted)*deleted=0;
        if(!realpath(executable_.c_str(),expected))return false;
        return std::strcmp(exe,expected)==0;
    };
    bool lockBusy=false;
    auto releaseLock=[this]() { if(lockFd_>=0){flock(lockFd_,LOCK_UN);close(lockFd_);lockFd_=-1;} };
    auto launch=[this,&bundledExecutable,&lockBusy,&releaseLock,&cleanupTemporaryExecutable,&copyExecutableToTmp]() -> bool {
        const char* lockPath=std::getenv("TROLLFX_RTA_LOCK_PATH"); if(!lockPath)lockPath="/tmp/trollfx-rta-v04.lock";
        lockFd_=open(lockPath,O_RDWR|O_CREAT|O_CLOEXEC,0600);
        if(lockFd_<0 || flock(lockFd_,LOCK_EX|LOCK_NB)!=0) { if(lockFd_>=0)close(lockFd_);lockFd_=-1;lockBusy=true;return false; }
        const std::string sourceExecutable=bundledExecutable();if(sourceExecutable.empty()||access(sourceExecutable.c_str(),R_OK)!=0){releaseLock();return false;}
        const std::string dir=sourceExecutable.substr(0,sourceExecutable.find_last_of('/'));
        if(!copyExecutableToTmp(sourceExecutable)){cleanupTemporaryExecutable();releaseLock();return false;}
        const std::string c0=dir+"/calibration/7033298.txt",c90=dir+"/calibration/7033298_90deg.txt";
        const char* port="8096"; if(const char* configured=std::getenv("TROLLFX_RTA_PORT"))port=configured;
        posix_spawn_file_actions_t actions; if(posix_spawn_file_actions_init(&actions)!=0){cleanupTemporaryExecutable();releaseLock();return false;}
        posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,"/tmp/trollfx-rta-v04.log",O_WRONLY|O_CREAT|O_APPEND,0600);
        posix_spawn_file_actions_adddup2(&actions,STDOUT_FILENO,STDERR_FILENO);
        char* const argv[]={const_cast<char*>(executable_.c_str()),const_cast<char*>("--device"),const_cast<char*>("hw:U18dB,0"),const_cast<char*>("--port"),const_cast<char*>(port),const_cast<char*>("--cal-0"),const_cast<char*>(c0.c_str()),const_cast<char*>("--cal-90"),const_cast<char*>(c90.c_str()),const_cast<char*>("--orientation"),const_cast<char*>("0"),nullptr};
        pid_t pid=-1; const int spawnResult=posix_spawn(&pid,executable_.c_str(),&actions,nullptr,argv,environ);
        posix_spawn_file_actions_destroy(&actions); if(spawnResult!=0){cleanupTemporaryExecutable();releaseLock();return false;}
        childPid_=pid; childStartedAt_=std::chrono::steady_clock::now(); return true;
    };
    auto stopOwned=[&]() {
        if(childPid_<=0){releaseLock();cleanupTemporaryExecutable();return;}
        serviceState_.store(SERVICE_STOPPING,std::memory_order_release);
        if(childMatches())kill(childPid_,SIGTERM);
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2); int status=0;
        while(std::chrono::steady_clock::now()<until) { pid_t r=waitpid(childPid_,&status,WNOHANG); if(r==childPid_){childPid_=-1;releaseLock();cleanupTemporaryExecutable();return;} std::this_thread::sleep_for(std::chrono::milliseconds(50)); }
        if(childMatches())kill(childPid_,SIGKILL);
        waitpid(childPid_,&status,0); childPid_=-1;releaseLock();cleanupTemporaryExecutable();
    };
    while (!stop_.load(std::memory_order_acquire)) {
        lock.unlock();
        int currentMode=mode<0?0:mode;
        if(mode<0) { std::lock_guard<std::mutex> stateLock(mutex_); if(!clients_.empty())currentMode=clients_.front()->serviceMode(); }
        if(modePending_.exchange(false,std::memory_order_acq_rel)) currentMode=desiredMode_.load(std::memory_order_acquire);
        if(currentMode!=mode) { mode=currentMode; autoSuppressed_.store(0,std::memory_order_release); failedStarts=0;retryAt=std::chrono::steady_clock::now();if(mode==0)manualStart=false; }
        lockBusy=false;
        const int cmd=command_.exchange(0,std::memory_order_acq_rel);
        if(cmd==1){manualStart=true;autoSuppressed_.store(0,std::memory_order_release);failedStarts=0;retryAt=std::chrono::steady_clock::now();}
        if(cmd==2){manualStart=false;autoSuppressed_.store(1,std::memory_order_release); if(childPid_>0)stopOwned();}
        bool shouldRun=manualStart || (mode==0 && !autoSuppressed_.load(std::memory_order_acquire));
        std::string probeBody; trollfx::Snapshot probe;
        const bool apiAvailable=request("/api/spectrum","GET",probeBody) && trollfx::parseSpectrumJson(probeBody,probe);
        if(childPid_>0) {
            int childStatus=0; const pid_t ended=waitpid(childPid_,&childStatus,WNOHANG);
            if(ended==childPid_){ childPid_=-1;releaseLock();cleanupTemporaryExecutable(); if(shouldRun){ ++failedStarts; const char* delay=std::getenv("TROLLFX_RTA_TEST_RETRY_DELAY_MS");retryAt=std::chrono::steady_clock::now()+std::chrono::milliseconds(delay?std::max(50,atoi(delay)):failedStarts==1?2000:failedStarts==2?5000:15000); } }
        }
        long startupTimeoutMs=10000;if(const char* configured=std::getenv("TROLLFX_RTA_TEST_STARTUP_TIMEOUT_MS")){char* end=nullptr;long n=strtol(configured,&end,10);if(end!=configured&&n>=100&&n<=60000)startupTimeoutMs=n;}
        if(childPid_>0 && !apiAvailable && std::chrono::steady_clock::now()-childStartedAt_>std::chrono::milliseconds(startupTimeoutMs)) {
            stopOwned(); ++failedStarts; const char* delay=std::getenv("TROLLFX_RTA_TEST_RETRY_DELAY_MS");retryAt=std::chrono::steady_clock::now()+std::chrono::milliseconds(delay?std::max(50,atoi(delay)):failedStarts==1?2000:failedStarts==2?5000:15000);
        }
        if(childPid_>0 && !childMatches()) { childPid_=-1;releaseLock();cleanupTemporaryExecutable(); }
        if(external && !apiAvailable && childPid_<=0) { external=false; retryAt=std::chrono::steady_clock::now()+std::chrono::seconds(1); }
        if(!shouldRun && childPid_>0) stopOwned();
        if(apiAvailable && childPid_<=0) {
            external=true; serviceState_.store(SERVICE_EXTERNAL,std::memory_order_release);
        } else if(childPid_>0) {
            external=false; serviceState_.store(probe.ok?SERVICE_ONLINE:SERVICE_ERROR,std::memory_order_release);
        } else if(shouldRun && !external && failedStarts<3 && std::chrono::steady_clock::now()>=retryAt && !apiAvailable) {
            if(launch()){ serviceState_.store(SERVICE_STARTING,std::memory_order_release); }
            else if(lockBusy)serviceState_.store(SERVICE_STARTING,std::memory_order_release);
            else { const char* delay=std::getenv("TROLLFX_RTA_TEST_RETRY_DELAY_MS");failedStarts++;retryAt=std::chrono::steady_clock::now()+std::chrono::milliseconds(delay?std::max(50,atoi(delay)):failedStarts<3?5000:30000);serviceState_.store(SERVICE_ERROR,std::memory_order_release); }
        } else if(!apiAvailable && childPid_<=0) serviceState_.store(shouldRun?(failedStarts>0?SERVICE_ERROR:SERVICE_STARTING):SERVICE_STOPPED,std::memory_order_release);
        if (reset_.exchange(false, std::memory_order_acq_rel)) {
            std::string ignored; request("/api/reset", "POST", ignored);
            std::lock_guard<std::mutex> resetLock(mutex_);
            for (TrollfxRta* p:clients_) p->resetPeaks();
        }
        trollfx::Snapshot s=probe;
        const bool good=apiAvailable;
        if (good) {
            const int avg=averageIndex_.load(std::memory_order_acquire);
            if (lastAverageSent!=avg) { static const int frames[]={1,2,4,8,16}; std::string response; char path[64]; std::snprintf(path,sizeof(path),"/api/average?frames=%d",frames[avg]); request(path,"POST",response); lastAverageSent=avg; }
            std::lock_guard<std::mutex> publish(mutex_);
            for (TrollfxRta* p:clients_) { p->setServiceState(serviceState_.load(std::memory_order_acquire)); if(s.ok)p->update(s);else p->offline(); }
        } else {
            lastAverageSent=-1;
            std::lock_guard<std::mutex> publish(mutex_);
            for (TrollfxRta* p:clients_) { p->offline();p->setServiceState(serviceState_.load(std::memory_order_acquire)); }
        }
        lock.lock(); cv_.wait_for(lock,std::chrono::milliseconds(1000/refreshHz),[this]{return stop_.load(std::memory_order_acquire) || command_.load(std::memory_order_acquire)!=0 || modePending_.load(std::memory_order_acquire);});
    }
    lock.unlock(); stopOwned();
}

} // namespace
MPC_PLUGIN_ENTRY(TrollfxRta)
