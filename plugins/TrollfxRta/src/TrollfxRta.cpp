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
#include <cstdlib>
#include <dlfcn.h>
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
    kServiceMode, kServiceStart, kServiceStop, kServiceState, kParamCount
};
constexpr int kUid = MPC_FOURCC('T', 'f', 'R', 'A');
enum ServiceState : int { SERVICE_STOPPED, SERVICE_STARTING, SERVICE_ONLINE, SERVICE_STOPPING, SERVICE_ERROR, SERVICE_EXTERNAL };
static void servicePathAnchor() {}

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
        if (index == kServiceMode) SharedClient::instance().queueServiceMode(static_cast<int>(std::lround(value)));
        if (index == kServiceStart && value >= 0.5f) { SharedClient::instance().command(1); set(kServiceStart,0.0f); }
        if (index == kServiceStop && value >= 0.5f) { SharedClient::instance().command(2); set(kServiceStop,0.0f); }
    }
    bool formatValue(int index, float v, std::string& out) override {
        if (index == kOrientation) { out = v < 0.5f ? "0 deg" : "90 deg"; return true; }
        if ((index == kLz || index == kLa) && get(kCalibration) < 0.5f) { out = "N/A"; return true; }
        if (index == kCalibration) {
            out = v < 0.5f ? "PROVISIONAL" : (v < 1.5f ? "VERIFIED" : "USER REF"); return true;
        }
        if (index == kMic) { out = v >= 0.5f ? "ONLINE" : "OFFLINE"; return true; }
        if (index == kServiceState) { static const char* names[]={"STOPPED","STARTING","ONLINE","STOPPING","ERROR","EXTERNAL"}; int n=mpc::clamp(static_cast<int>(v),0,5); out=names[n]; return true; }
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
        return v;
    }
private:
    static float ledValue(float db, float floorDb) {
        if (!std::isfinite(db)) return 0.0f;
        return mpc::clamp((db-floorDb)*24.0f/(-floorDb),0.0f,24.0f);
    }
    static mpc::PluginInfo makeInfo() {
        mpc::PluginInfo i; i.name="TROLLfx - VST - RTA"; i.vendor="TROLLfx"; i.uniqueId=kUid;
        i.version=400; i.isInstrument=false; i.numInputs=2; i.numOutputs=2; i.tailSamples=1; return i;
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
    auto launch=[this,&bundledExecutable,&lockBusy,&releaseLock]() -> bool {
        const char* lockPath=std::getenv("TROLLFX_RTA_LOCK_PATH"); if(!lockPath)lockPath="/tmp/trollfx-rta-v04.lock";
        lockFd_=open(lockPath,O_RDWR|O_CREAT|O_CLOEXEC,0600);
        if(lockFd_<0 || flock(lockFd_,LOCK_EX|LOCK_NB)!=0) { if(lockFd_>=0)close(lockFd_);lockFd_=-1;lockBusy=true;return false; }
        executable_=bundledExecutable(); if(executable_.empty() || access(executable_.c_str(),X_OK)!=0){releaseLock();return false;}
        const std::string dir=executable_.substr(0,executable_.find_last_of('/'));
        const std::string c0=dir+"/calibration/7033298.txt",c90=dir+"/calibration/7033298_90deg.txt";
        const char* port="8096"; if(const char* configured=std::getenv("TROLLFX_RTA_PORT"))port=configured;
        posix_spawn_file_actions_t actions; if(posix_spawn_file_actions_init(&actions)!=0){releaseLock();return false;}
        posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,"/tmp/trollfx-rta-v04.log",O_WRONLY|O_CREAT|O_APPEND,0600);
        posix_spawn_file_actions_adddup2(&actions,STDOUT_FILENO,STDERR_FILENO);
        char* const argv[]={const_cast<char*>(executable_.c_str()),const_cast<char*>("--device"),const_cast<char*>("hw:U18dB,0"),const_cast<char*>("--port"),const_cast<char*>(port),const_cast<char*>("--cal-0"),const_cast<char*>(c0.c_str()),const_cast<char*>("--cal-90"),const_cast<char*>(c90.c_str()),const_cast<char*>("--orientation"),const_cast<char*>("0"),nullptr};
        pid_t pid=-1; const int spawnResult=posix_spawn(&pid,executable_.c_str(),&actions,nullptr,argv,environ);
        posix_spawn_file_actions_destroy(&actions); if(spawnResult!=0){releaseLock();return false;}
        childPid_=pid; childStartedAt_=std::chrono::steady_clock::now(); return true;
    };
    auto stopOwned=[&]() {
        if(childPid_<=0){releaseLock();return;}
        serviceState_.store(SERVICE_STOPPING,std::memory_order_release);
        if(childMatches())kill(childPid_,SIGTERM);
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2); int status=0;
        while(std::chrono::steady_clock::now()<until) { pid_t r=waitpid(childPid_,&status,WNOHANG); if(r==childPid_){childPid_=-1;releaseLock();return;} std::this_thread::sleep_for(std::chrono::milliseconds(50)); }
        if(childMatches())kill(childPid_,SIGKILL);
        waitpid(childPid_,&status,0); childPid_=-1;releaseLock();
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
            if(ended==childPid_){ childPid_=-1;releaseLock(); if(shouldRun){ ++failedStarts; const char* delay=std::getenv("TROLLFX_RTA_TEST_RETRY_DELAY_MS");retryAt=std::chrono::steady_clock::now()+std::chrono::milliseconds(delay?std::max(50,atoi(delay)):failedStarts==1?2000:failedStarts==2?5000:15000); } }
        }
        long startupTimeoutMs=10000;if(const char* configured=std::getenv("TROLLFX_RTA_TEST_STARTUP_TIMEOUT_MS")){char* end=nullptr;long n=strtol(configured,&end,10);if(end!=configured&&n>=100&&n<=60000)startupTimeoutMs=n;}
        if(childPid_>0 && !apiAvailable && std::chrono::steady_clock::now()-childStartedAt_>std::chrono::milliseconds(startupTimeoutMs)) {
            stopOwned(); ++failedStarts; const char* delay=std::getenv("TROLLFX_RTA_TEST_RETRY_DELAY_MS");retryAt=std::chrono::steady_clock::now()+std::chrono::milliseconds(delay?std::max(50,atoi(delay)):failedStarts==1?2000:failedStarts==2?5000:15000);
        }
        if(childPid_>0 && !childMatches()) { childPid_=-1;releaseLock(); }
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
