#include "mpc_plugin.h"
#include "spectrum_json.h"

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
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace {
enum : int {
    kBand = 0, kPeak = 31, kRaw = 62, kLz, kLa, kOrientation, kMic, kCalibration,
    kSensitivity = 68, kBand16 = 69, kPeak16 = 85, kGain = 101, kFloor, kSmoothing, kAverage, kPeakEnable, kPeakReset,
    kAttack, kFall, kPeakHold, kPeakFall, kParamCount
};
constexpr int kUid = MPC_FOURCC('T', 'f', 'R', 'A');

bool writeAll(int fd, const char* p, size_t n) {
    while (n) {
        const ssize_t w = ::send(fd, p, n, MSG_NOSIGNAL);
        if (w <= 0) return false;
        p += w; n -= static_cast<size_t>(w);
    }
    return true;
}

bool request(const char* path, const char* method, std::string& body) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    timeval tv{0, 160000};
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
    char buf[2048]; std::string response; response.reserve(8192);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(400);
    while (response.size() < 32768) {
        const auto remaining=std::chrono::duration_cast<std::chrono::microseconds>(deadline-std::chrono::steady_clock::now()).count();
        if (remaining<=0) break;
        fd_set readable; FD_ZERO(&readable); FD_SET(fd,&readable);
        timeval wait{static_cast<time_t>(remaining/1000000),static_cast<suseconds_t>(remaining%1000000)};
        if (select(fd+1,&readable,nullptr,nullptr,&wait)<=0) break;
        const size_t room=32768-response.size();
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
private:
    void run();
    std::mutex lifecycleMutex_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<TrollfxRta*> clients_;
    std::thread thread_;
    bool stop_ = false;
    std::atomic<int> averageIndex_{0};
    std::atomic<bool> reset_{false};
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
        addProgram("RTA", defaults());
        SharedClient::instance().add(this);
    }
    ~TrollfxRta() override { SharedClient::instance().remove(this); }

    void process(mpc::ProcessContext& ctx) override {
        const int channels = mpc::clamp(ctx.numOutputs, 0, 2);
        for (int ch=0; ch<channels; ++ch) {
            if (!ctx.outputs || !ctx.outputs[ch] || !ctx.inputs || ch >= ctx.numInputs || !ctx.inputs[ch] || ctx.outputs[ch] == ctx.inputs[ch]) continue;
            std::memcpy(ctx.outputs[ch], ctx.inputs[ch], static_cast<size_t>(ctx.numFrames) * sizeof(float));
        }
    }
    void parameterChanged(int index, float value) override {
        if (index == kPeakReset && value >= 0.5f) SharedClient::instance().queueReset();
        if (index == kAverage) SharedClient::instance().queueAverage(static_cast<int>(std::lround(value)));
    }
    bool formatValue(int index, float v, std::string& out) override {
        if (index == kOrientation) { out = v < 0.5f ? "0 deg" : "90 deg"; return true; }
        if ((index == kLz || index == kLa) && get(kCalibration) < 0.5f) { out = "N/A"; return true; }
        if (index == kCalibration) {
            out = v < 0.5f ? "PROVISIONAL" : (v < 1.5f ? "VERIFIED" : "USER REF"); return true;
        }
        if (index == kMic) { out = v >= 0.5f ? "ONLINE" : "OFFLINE"; return true; }
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
    void offline() {
        if (getBool(kMic)) { set(kMic,0.0f); requestDisplayUpdate(); }
    }
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
        return v;
    }
private:
    static float ledValue(float db, float floorDb) {
        if (!std::isfinite(db)) return 0.0f;
        return mpc::clamp((db-floorDb)*24.0f/(-floorDb),0.0f,24.0f);
    }
    static mpc::PluginInfo makeInfo() {
        mpc::PluginInfo i; i.name="TROLLfx - VST - RTA"; i.vendor="TROLLfx"; i.uniqueId=kUid;
        i.version=300; i.isInstrument=false; i.numInputs=2; i.numOutputs=2; i.tailSamples=1; return i;
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
      if (clients_.empty() && thread_.joinable()) { stop_=true; cv_.notify_all(); join=std::move(thread_); } }
    if (join.joinable()) join.join();
}
void SharedClient::run() {
    std::unique_lock<std::mutex> lock(mutex_);
    int lastAverageSent=-1;
    int refreshHz=5;
    if (const char* setting=std::getenv("TROLLFX_RTA_REFRESH_HZ")) { char* end=nullptr; long n=std::strtol(setting,&end,10); if(end!=setting&&n>=1&&n<=10)refreshHz=static_cast<int>(n); }
    while (!stop_) {
        lock.unlock();
        if (reset_.exchange(false, std::memory_order_acq_rel)) {
            std::string ignored; request("/api/reset", "POST", ignored);
            std::lock_guard<std::mutex> resetLock(mutex_);
            for (TrollfxRta* p:clients_) p->resetPeaks();
        }
        std::string body; trollfx::Snapshot s;
        const bool good=request("/api/spectrum","GET",body) && trollfx::parseSpectrumJson(body,s);
        if (good) {
            const int avg=averageIndex_.load(std::memory_order_acquire);
            if (lastAverageSent!=avg) { static const int frames[]={1,2,4,8,16}; std::string response; char path[64]; std::snprintf(path,sizeof(path),"/api/average?frames=%d",frames[avg]); request(path,"POST",response); lastAverageSent=avg; }
            std::lock_guard<std::mutex> publish(mutex_);
            for (TrollfxRta* p:clients_) p->update(s);
        } else {
            lastAverageSent=-1;
            std::lock_guard<std::mutex> publish(mutex_);
            for (TrollfxRta* p:clients_) p->offline();
        }
        lock.lock(); cv_.wait_for(lock,std::chrono::milliseconds(1000/refreshHz),[this]{return stop_;});
    }
}

} // namespace
MPC_PLUGIN_ENTRY(TrollfxRta)
