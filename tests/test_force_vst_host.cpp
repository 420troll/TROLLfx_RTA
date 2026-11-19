#include <dlfcn.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <initializer_list>
#include <signal.h>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <string>
#include <iterator>
#include "mpc_vst2_abi.h"

static VstIntPtr master(AEffect*, VstInt32 opcode, VstInt32, VstIntPtr, void*, float) {
    if (opcode==audioMasterVersion) return 2400;
    if (opcode==audioMasterGetTime) {
        static VstTimeInfo info{}; info.sampleRate=44100.0; info.tempo=120.0;
        info.timeSigNumerator=4; info.timeSigDenominator=4;
        return reinterpret_cast<VstIntPtr>(&info);
    }
    return 1;
}
int main(int argc,char**argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=2 && argc!=3 && !(argc==4&&std::strcmp(argv[2],"--saved-state-compat")==0)) return 2;
    void* lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!lib) { std::fprintf(stderr,"dlopen: %s\n",dlerror()); return 1; }
    using Entry=AEffect*(*)(audioMasterCallback);
    auto entry=reinterpret_cast<Entry>(dlsym(lib,"VSTPluginMain"));
    if(!entry) { std::puts("FAIL VSTPluginMain symbol"); return 1; }
    AEffect* a=entry(master); AEffect* b=entry(master);
    if(!a||!b) { std::puts("FAIL independent instantiation"); return 1; }
    for(AEffect* e:{a,b}) {
        if(e->magic!=kEffectMagic||e->numInputs!=2||e->numOutputs!=2||e->uniqueID!=MPC_FOURCC('T','f','R','A')) { std::puts("FAIL plugin identity/I-O"); return 1; }
        e->dispatcher(e,effOpen,0,0,nullptr,0);
        e->dispatcher(e,effSetSampleRate,0,0,nullptr,44100.0f);
        e->dispatcher(e,effSetBlockSize,0,128,nullptr,0);
        e->dispatcher(e,effMainsChanged,0,1,nullptr,0);
    }
    if(a->numParams!=607){std::printf("FAIL appended V0.5.4 parameter count: %d\n",a->numParams);return 1;}
    if(a->dispatcher(a,effGetVendorVersion,0,0,nullptr,0.0f)!=504){std::puts("FAIL V0.5.4 build version");return 1;}
    char parameterName[64]{};a->dispatcher(a,effGetParamName,110,0,parameterName,0.0f);
    if(std::strcmp(parameterName,"Peak Fall")!=0){std::printf("FAIL V0.3 parameter ordering changed at 110: %s\n",parameterName);return 1;}
    a->dispatcher(a,effGetParamName,111,0,parameterName,0.0f);
    if(std::strcmp(parameterName,"Service Mode")!=0){std::printf("FAIL V0.4 controls were not appended: %s\n",parameterName);return 1;}
    a->dispatcher(a,effGetParamName,114,0,parameterName,0.0f);
    if(std::strcmp(parameterName,"Service Status")!=0){std::printf("FAIL V0.4 parameter 114 moved: %s\n",parameterName);return 1;}
    a->dispatcher(a,effGetParamName,115,0,parameterName,0.0f);
    if(std::strcmp(parameterName,"SWEEP START")!=0||a->getParameter(a,115)!=0.0f||a->getParameter(a,116)!=0.0f||a->getParameter(a,117)!=0.0f){std::printf("FAIL V0.5 sweep controls/defaults: %s\n",parameterName);return 1;}
    a->dispatcher(a,effGetParamName,122,0,parameterName,0.0f);
    if(std::strcmp(parameterName,"Sweep Smoothing")!=0||std::fabs(a->getParameter(a,122)-4.0f/5.0f)>0.01f){std::printf("FAIL V0.5.1 appended smoothing selector/default: %s %.3f\n",parameterName,a->getParameter(a,122));return 1;}
    a->dispatcher(a,effGetParamName,123,0,parameterName,0.0f);if(std::strcmp(parameterName,"Saved Measurement")!=0||a->getParameter(a,123)!=0.0f){std::printf("FAIL V0.5.1 saved measurement selector/default: %s\n",parameterName);return 1;}
    a->dispatcher(a,effGetParamName,155,0,parameterName,0.0f);if(std::strcmp(parameterName,"SP20")!=0){std::printf("FAIL V0.5.2 smoothed graph parameter index: %s\n",parameterName);return 1;}
    a->dispatcher(a,effGetParamName,186,0,parameterName,0.0f);if(std::strcmp(parameterName,"RawP20")!=0){std::printf("FAIL V0.5.2 raw graph parameter index: %s\n",parameterName);return 1;}
    a->dispatcher(a,effGetParamName,217,0,parameterName,0.0f);if(std::strcmp(parameterName,"Result Status")!=0){std::printf("FAIL V0.5.2 result status parameter: %s\n",parameterName);return 1;}
    a->dispatcher(a,effGetParamName,218,0,parameterName,0.0f);if(std::strcmp(parameterName,"GP20")!=0){std::printf("FAIL V0.5.3 appended graph marker parameter: %s\n",parameterName);return 1;}
    a->dispatcher(a,effGetParamName,249,0,parameterName,0.0f);if(std::strcmp(parameterName,"Sweep Diagnostic")!=0){std::printf("FAIL V0.5.3 appended diagnostic parameter: %s\n",parameterName);return 1;}
    if(a->numParams!=607){std::printf("FAIL V0.5.4 parameter count: %d expected 607\n",a->numParams);return 1;}
    a->dispatcher(a,effGetParamName,250,0,parameterName,0.0f);if(std::strcmp(parameterName,"Display Range")!=0){std::printf("FAIL V0.5.4 appended display range parameter: %s\n",parameterName);return 1;}
    a->dispatcher(a,effGetParamName,251,0,parameterName,0.0f);if(std::strcmp(parameterName,"Display Center")!=0){std::printf("FAIL V0.5.4 appended display center parameter: %s\n",parameterName);return 1;}
    a->dispatcher(a,effGetParamName,259,0,parameterName,0.0f);if(std::strcmp(parameterName,"G0")!=0){std::printf("FAIL V0.5.4 appended curve-segment parameter: %s\n",parameterName);return 1;}
    if(argc>=3) {
        const char* mode=argv[2];
        auto display=[&](AEffect* e,int index){char out[64]{};e->dispatcher(e,effGetParamDisplay,index,0,out,0.0f);return std::string(out);};
        auto waitDisplay=[&](int index,const char* expected,int timeoutMs){const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeoutMs);while(display(a,index)!=expected&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));return display(a,index)==expected;};
        auto waitParameter=[&](int index,float expected,float tolerance,int timeoutMs){const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeoutMs);while(std::fabs(a->getParameter(a,index)-expected)>tolerance&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));return std::fabs(a->getParameter(a,index)-expected)<=tolerance;};
        if(std::strcmp(mode,"--sweep-playback")==0){
            a->setParameter(a,118,0.0f); // minimum 0.5 s sweep; all work remains on the worker
            a->setParameter(a,115,1.0f);
            const auto armed=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while(display(a,117)!="CAPTURING"&&std::chrono::steady_clock::now()<armed)std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if(display(a,117)!="CAPTURING"){std::printf("FAIL sweep never reached CAPTURING: %s\n",display(a,117).c_str());return 1;}
            float il[128],ir[128],ol[128]{},orr[128]{};float peak=0;size_t sweepFrames=0;
            for(int block=0;block<600;block++){
                for(int i=0;i<128;i++){il[i]=0.013f;ir[i]=-0.021f;ol[i]=orr[i]=0;}
                float* in[]={il,ir};float* out[]={ol,orr};a->processReplacing(a,in,out,128);
                for(int i=0;i<128;i++)if(ol[i]==orr[i]&&ol[i]!=il[i]){++sweepFrames;peak=std::max(peak,std::abs(ol[i]));}
                if(sweepFrames>70000)break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            const auto complete=std::chrono::steady_clock::now()+std::chrono::seconds(8);
            while(display(a,117)!="READY"&&std::chrono::steady_clock::now()<complete)std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if(sweepFrames<1000||peak>0.04f||display(a,117)!="READY"){std::printf("FAIL sweep output/capture analysis: frames=%zu peak=%.4f state=%s\n",sweepFrames,peak,display(a,117).c_str());return 1;}
            for(int i=0;i<128;i++){il[i]=0.013f;ir[i]=-0.021f;ol[i]=orr[i]=0;}
            float* in[]={il,ir};float* out[]={ol,orr};a->processReplacing(a,in,out,128);
            for(int i=0;i<128;i++)if(ol[i]!=il[i]||orr[i]!=ir[i]){std::puts("FAIL normal pass-through did not resume after sweep error");return 1;}
            a->setParameter(a,122,0.0f); // No Smoothing re-renders the saved result only.
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            a->setParameter(a,123,1.0f/7.0f); // Previous 2 selects the older saved measurement.
            if(std::fabs(a->getParameter(a,123)-1.0f/7.0f)>0.01f){std::printf("FAIL saved-measurement selector value %.4f\n",a->getParameter(a,123));return 1;}
            if(!waitParameter(210,1.0f,0.02f,20000)||!waitDisplay(217,"READY",5000)||std::fabs(a->getParameter(a,148)-0.875f)>0.02f||std::fabs(a->getParameter(a,179)-1.0f)>0.02f){std::printf("FAIL selected Previous 2 response values/status: %s led %.3f smooth %.3f raw %.3f\n",display(a,217).c_str(),a->getParameter(a,148),a->getParameter(a,179),a->getParameter(a,210));return 1;}
            for(int i=0;i<31;i++)if(std::fabs(a->getParameter(a,218+i)-a->getParameter(a,124+i))>0.03f){std::printf("FAIL saved graph marker is not aligned with saved LED response at %d\n",i);return 1;}
            const float previous2Raw=a->getParameter(a,210);
            a->setParameter(a,123,2.0f/7.0f); // Previous 3 has a distinct 5 kHz response.
            if(!waitParameter(210,2.0f/3.0f,0.03f,20000)||!waitDisplay(217,"READY",5000)||std::fabs(a->getParameter(a,148)-0.625f)>0.03f||std::fabs(a->getParameter(a,210)-previous2Raw)<0.25f){std::printf("FAIL switching to Previous 3 did not update static response curve: %s led %.3f raw %.3f\n",display(a,217).c_str(),a->getParameter(a,148),a->getParameter(a,210));return 1;}
            if(std::fabs(a->getParameter(a,218+24)-a->getParameter(a,124+24))>0.03f){std::puts("FAIL graph peak marker did not follow saved Previous 3 response");return 1;}
            const float smoothingModes[]={0.0f,0.2f,0.4f,0.6f,0.8f,1.0f};
            for(float mode:smoothingModes){a->setParameter(a,122,mode);std::this_thread::sleep_for(std::chrono::milliseconds(250));}
            if(!waitDisplay(217,"READY",4000)){std::puts("FAIL smoothing/measurement changes lost the displayed result");return 1;}
            // The new full-precision outputs retain actual relative dB outside
            // the LED window; range and center change only the reversible view.
            const int actualSmoothDb=545+17, actualRawDb=576+17;
            const float expectedRelativeDb=(0.0f+240.0f)/360.0f;
            if(std::fabs(a->getParameter(a,actualSmoothDb)-expectedRelativeDb)>0.01f||std::fabs(a->getParameter(a,actualRawDb)-expectedRelativeDb)>0.01f){std::printf("FAIL unbounded dB diagnostics: smooth %.4f raw %.4f\n",a->getParameter(a,actualSmoothDb),a->getParameter(a,actualRawDb));return 1;}
            a->setParameter(a,250,0.0f); // 24 dB range, same measurement and reference.
            if(!waitParameter(141,1.0f,0.02f,3000)){std::puts("FAIL 24 dB range did not map the 1 kHz reference to its visible upper boundary");return 1;}
            a->setParameter(a,251,0.5f); // center at 0 dB, range now [-12,+12].
            if(!waitParameter(141,0.5f,0.02f,3000)||std::fabs(a->getParameter(a,actualSmoothDb)-expectedRelativeDb)>0.01f){std::puts("FAIL display center altered stored response instead of only its view");return 1;}
            for(AEffect* e:{a,b}){e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);}dlclose(lib);
            std::printf("PASS explicit sweep emits safe identical stereo signal, then returns to pass-through (%zu sweep frames)\n",sweepFrames);return 0;
        }
        if(std::strcmp(mode,"--saved-state-compat")==0){
            if(argc!=4)return 2;
            std::ifstream file(argv[3],std::ios::binary);std::vector<char> chunk((std::istreambuf_iterator<char>(file)),{});
            if(chunk.size()<12||std::memcmp(chunk.data(),"MPCP",4)!=0||a->dispatcher(a,effSetChunk,0,(VstIntPtr)chunk.size(),chunk.data(),0.0f)!=1){std::puts("FAIL loading V0.3 state chunk");return 1;}
            if(std::fabs(a->getParameter(a,101)-27.0f/48.0f)>0.002f||std::fabs(a->getParameter(a,110)-40.0f/58.0f)>0.002f||a->getParameter(a,111)!=0||a->getParameter(a,112)!=0||a->getParameter(a,113)!=0||a->getParameter(a,115)!=0||a->getParameter(a,116)!=0||a->getParameter(a,117)!=0){std::puts("FAIL V0.3 parameter values or appended V0.4/V0.5 defaults changed");return 1;}
            for(AEffect* e:{a,b}){e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);}dlclose(lib);std::puts("PASS V0.3 saved-state chunk loads unchanged and appended controls default safely");return 0;
        }
        if(std::strcmp(mode,"--saved-v051-state-compat")==0){
            // Build a V0.5.1-shaped 155-parameter state chunk from the current
            // stable IDs, then load it into V0.5.4. This exercises the exact
            // old chunk shape without relying on an obsolete desktop binary.
            a->setParameter(a,101,0.75f);a->setParameter(a,110,38.0f/58.0f);a->setParameter(a,122,0.6f);a->setParameter(a,123,1.0f/7.0f);
            void* chunkPtr=nullptr;const VstIntPtr chunkSize=a->dispatcher(a,effGetChunk,0,0,&chunkPtr,0.0f);
            if(!chunkPtr||chunkSize<12){std::puts("FAIL obtaining current stable parameter state");return 1;}
            const auto* source=static_cast<const unsigned char*>(chunkPtr);std::vector<unsigned char> old(source,source+12);old[4]=2;old[5]=old[6]=old[7]=0;
            old[8]=155;old[9]=old[10]=old[11]=0;size_t pos=12;
            for(int i=0;i<155;i++){if(pos>=static_cast<size_t>(chunkSize)){std::puts("FAIL truncating V0.5.1 parameter state");return 1;}const size_t item=1+source[pos]+8;if(pos+item>static_cast<size_t>(chunkSize)){std::puts("FAIL parsing V0.5.1 parameter state");return 1;}old.insert(old.end(),source+pos,source+pos+item);pos+=item;}
            old.insert(old.end(),4,0);if(a->dispatcher(a,effSetChunk,0,static_cast<VstIntPtr>(old.size()),old.data(),0.0f)!=1){std::puts("FAIL loading V0.5.1 saved-state shape");return 1;}
            if(std::fabs(a->getParameter(a,101)-0.75f)>0.002f||std::fabs(a->getParameter(a,110)-38.0f/58.0f)>0.002f||std::fabs(a->getParameter(a,122)-0.6f)>0.002f||std::fabs(a->getParameter(a,123)-1.0f/7.0f)>0.002f||std::fabs(a->getParameter(a,155))>0.002f||std::fabs(a->getParameter(a,186))>0.002f){std::puts("FAIL V0.5.1 parameters changed or V0.5.2 appended defaults were not safe");return 1;}
            for(AEffect* e:{a,b}){e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);}dlclose(lib);std::puts("PASS V0.5.1 155-parameter saved-state shape loads with indices preserved and appended graph values defaulted");return 0;
        }
        if(std::strcmp(mode,"--saved-v052-state-compat")==0){
            a->setParameter(a,101,0.625f);a->setParameter(a,122,0.4f);a->setParameter(a,123,0.25f);
            void* chunkPtr=nullptr;const VstIntPtr chunkSize=a->dispatcher(a,effGetChunk,0,0,&chunkPtr,0.0f);
            if(!chunkPtr||chunkSize<12){std::puts("FAIL obtaining V0.5.2-compatible state");return 1;}
            const auto* source=static_cast<const unsigned char*>(chunkPtr);std::vector<unsigned char> old(source,source+12);old[4]=2;old[5]=old[6]=old[7]=0;old[8]=218;old[9]=old[10]=old[11]=0;size_t pos=12;
            for(int i=0;i<218;i++){if(pos>=static_cast<size_t>(chunkSize)){std::puts("FAIL truncating V0.5.2 saved state");return 1;}const size_t item=1+source[pos]+8;if(pos+item>static_cast<size_t>(chunkSize)){std::puts("FAIL parsing V0.5.2 saved state");return 1;}old.insert(old.end(),source+pos,source+pos+item);pos+=item;}
            old.insert(old.end(),4,0);const auto loaded=a->dispatcher(a,effSetChunk,0,static_cast<VstIntPtr>(old.size()),old.data(),0.0f);const float gain=a->getParameter(a,101),smoothing=a->getParameter(a,122),measurement=a->getParameter(a,123),graphPeak=a->getParameter(a,218);if(loaded!=1||std::fabs(gain-0.625f)>0.002f||std::fabs(smoothing-0.4f)>0.002f||std::fabs(measurement-0.25f)>0.002f||!std::isfinite(graphPeak)||graphPeak<0.0f||graphPeak>24.0f){std::printf("FAIL V0.5.2 state: load=%ld gain=%.4f smoothing=%.4f measurement=%.4f appendedGraph=%.4f\n",static_cast<long>(loaded),gain,smoothing,measurement,graphPeak);return 1;}
            for(AEffect* e:{a,b}){e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);}dlclose(lib);std::puts("PASS V0.5.2 218-parameter state loads unchanged; V0.5.4 graph parameters append safely");return 0;
        }
        const char* pidPath=std::getenv("TROLLFX_RTA_TEST_PIDFILE"); if(!pidPath){std::puts("FAIL missing mock service PID file setting");return 1;}
        auto readPids=[&](){std::vector<pid_t> p;std::ifstream f(pidPath);long value=0;while(f>>value)p.push_back(static_cast<pid_t>(value));return p;};
        if(std::strcmp(mode,"--service-rapid")==0) {
            auto closeEffect=[](AEffect* e){e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);};
            auto waitPid=[&](size_t wanted){const auto d=std::chrono::steady_clock::now()+std::chrono::seconds(4);while(readPids().size()<wanted&&std::chrono::steady_clock::now()<d)std::this_thread::sleep_for(std::chrono::milliseconds(25));return readPids();};
            auto p=waitPid(1);if(p.empty()){std::puts("FAIL rapid load initial service start");return 1;}closeEffect(a);closeEffect(b);
            auto dead=[&](pid_t id){const auto d=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(kill(id,0)==0&&std::chrono::steady_clock::now()<d)std::this_thread::sleep_for(std::chrono::milliseconds(25));return kill(id,0)!=0;};
            if(!dead(p.back())){std::puts("FAIL rapid unload did not stop service");return 1;}
            for(size_t i=1;i<4;i++){AEffect* e=entry(master);e->dispatcher(e,effOpen,0,0,nullptr,0);const auto ids=waitPid(i+1);if(ids.size()!=i+1){std::puts("FAIL rapid reload did not start one service");return 1;}closeEffect(e);if(!dead(ids.back())){std::puts("FAIL rapid unload left service running");return 1;}}
            dlclose(lib);std::puts("PASS rapid load/unload cycles release worker, service and child process");return 0;
        }
        if(std::strcmp(mode,"--service-occupied")==0||std::strcmp(mode,"--service-shutdown-starting")==0||std::strcmp(mode,"--service-timeout")==0) {
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);std::vector<pid_t> p;
            while((p=readPids()).empty()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(25));
            if(p.empty()){std::puts("FAIL startup test did not launch analyzer child");return 1;}
            if(std::strcmp(mode,"--service-occupied")==0){std::this_thread::sleep_for(std::chrono::milliseconds(1300));if(readPids().size()!=1||kill(p[0],0)==0){std::printf("FAIL port conflict child cleanup: records=%zu alive=%d\n",readPids().size(),kill(p[0],0)==0);return 1;}}
            if(std::strcmp(mode,"--service-timeout")==0){const auto d=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(kill(p[0],0)==0&&std::chrono::steady_clock::now()<d)std::this_thread::sleep_for(std::chrono::milliseconds(25));if(kill(p[0],0)==0){std::puts("FAIL startup timeout did not terminate owned child");return 1;}}
            for(AEffect* e:{a,b}){e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);}
            const auto d=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(kill(p.back(),0)==0&&std::chrono::steady_clock::now()<d)std::this_thread::sleep_for(std::chrono::milliseconds(25));if(kill(p.back(),0)==0){std::puts("FAIL startup shutdown left owned child alive");return 1;}
            dlclose(lib);std::printf("PASS %s\n",mode+2);return 0;
        }
        if(std::strcmp(mode,"--service-missing")==0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(700));
            if(display(a,114)!="ERROR"||!readPids().empty()){std::printf("FAIL missing service state: %s\n",display(a,114).c_str());return 1;}
            for(AEffect* e:{a,b}){e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);}dlclose(lib);
            std::puts("PASS missing executable gives bounded visible ERROR without blocking audio");return 0;
        }
        if(std::strcmp(mode,"--service-manual")==0) {
            for(AEffect* e:{a,b})e->setParameter(e,111,1.0f);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            if(!readPids().empty()){std::puts("FAIL MANUAL mode auto-started the service");return 1;}
            a->setParameter(a,112,1.0f);const auto started=std::chrono::steady_clock::now()+std::chrono::seconds(4);
            while(readPids().empty()&&std::chrono::steady_clock::now()<started)std::this_thread::sleep_for(std::chrono::milliseconds(50));
            auto p=readPids();if(p.size()!=1){std::puts("FAIL momentary manual START did not start exactly one service");return 1;}
            a->setParameter(a,113,1.0f);const auto stopped=std::chrono::steady_clock::now()+std::chrono::seconds(3);
            while(kill(p[0],0)==0&&std::chrono::steady_clock::now()<stopped)std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            if(kill(p[0],0)==0||readPids().size()!=1){std::puts("FAIL manual STOP did not suppress automatic restart");return 1;}
            a->setParameter(a,112,1.0f);const auto restarted=std::chrono::steady_clock::now()+std::chrono::seconds(4);
            while(readPids().size()<2&&std::chrono::steady_clock::now()<restarted)std::this_thread::sleep_for(std::chrono::milliseconds(50));
            p=readPids();if(p.size()!=2){std::puts("FAIL manual START did not resume service");return 1;}
            for(AEffect* e:{a,b}){e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);}dlclose(lib);
            std::puts("PASS manual mode, momentary START/STOP, and restart suppression policy");return 0;
        }
        const bool recovery=std::strcmp(mode,"--service-recovery")==0;
        if(!recovery&&std::strcmp(mode,"--service-lifecycle")!=0&&std::strcmp(mode,"--service-offline")!=0)return 2;
        if(recovery) {
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);std::vector<pid_t> p;
            while((p=readPids()).size()<2&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if(p.size()<2||p[0]==p[1]){std::puts("FAIL crashed owned analyzer was not restarted");return 1;}
            const auto online=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(display(a,114)!="ONLINE"&&std::chrono::steady_clock::now()<online)std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if(display(a,114)!="ONLINE"){std::puts("FAIL analyzer recovery did not reconnect API");return 1;}
            for(AEffect* e:{a,b}){e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);}dlclose(lib);
            std::puts("PASS controlled crash recovery and API reconnection");return 0;
        }
        if(std::strcmp(mode,"--service-offline")==0) {
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);auto p=readPids();
            while(p.empty()&&std::chrono::steady_clock::now()<deadline){std::this_thread::sleep_for(std::chrono::milliseconds(50));p=readPids();}
            std::this_thread::sleep_for(std::chrono::milliseconds(700));
            if(p.size()!=1||readPids().size()!=1||display(a,114)!="ERROR"){std::printf("FAIL offline microphone handling: %s\n",display(a,114).c_str());return 1;}
            for(AEffect* e:{a,b}){e->dispatcher(e,effMainsChanged,0,0,nullptr,0);e->dispatcher(e,effClose,0,0,nullptr,0);}dlclose(lib);
            std::puts("PASS missing microphone is visible without a respawn loop");return 0;
        }
        pid_t pid=-1; const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
        while(std::chrono::steady_clock::now()<deadline){std::ifstream f(pidPath); long value=0;if(f>>value){pid=static_cast<pid_t>(value);break;}std::this_thread::sleep_for(std::chrono::milliseconds(50));}
        if(pid<=0){std::puts("FAIL first instance did not start bundled analyzer mock");return 1;}
        if(std::strcmp(mode,"--service-lifecycle")==0){char proc[64],exe[4096];std::snprintf(proc,sizeof(proc),"/proc/%ld/exe",static_cast<long>(pid));const ssize_t n=readlink(proc,exe,sizeof(exe)-1);if(n<=0){std::puts("FAIL cannot inspect analyzer launch path");return 1;}exe[n]=0;if(std::strncmp(exe,"/tmp/trollfx-rta-v02-armv7-",27)!=0){std::printf("FAIL analyzer was not staged to executable tmp storage: %s\n",exe);return 1;}}
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        auto p=readPids();const int records=static_cast<int>(p.size());
        if(records!=1){std::printf("FAIL duplicate analyzer processes: %d\n",records);return 1;}
        a->dispatcher(a,effMainsChanged,0,0,nullptr,0);a->dispatcher(a,effClose,0,0,nullptr,0);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        if(kill(pid,0)!=0){std::puts("FAIL unloading one instance stopped shared analyzer");return 1;}
        b->dispatcher(b,effMainsChanged,0,0,nullptr,0);b->dispatcher(b,effClose,0,0,nullptr,0);
        const auto stopped=std::chrono::steady_clock::now()+std::chrono::seconds(4);
        while(std::chrono::steady_clock::now()<stopped&&kill(pid,0)==0)std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if(kill(pid,0)==0){std::puts("FAIL final instance did not stop owned analyzer");return 1;}
        dlclose(lib);std::puts("PASS owned analyzer starts once, is shared, and stops on final unload");return 0;
    }
    const float expected[]={23.0f/48.0f,30.0f/80.0f,0.0f,0.0f,1.0f,0.0f,50.0f/400.0f,31.0f/58.0f,200.0f/3000.0f,1.0f};
    for(int i=0;i<10;i++) if(std::fabs(a->getParameter(a,101+i)-expected[i])>0.002f) {
        std::printf("FAIL display control default %d: %.4f expected %.4f\n",i,a->getParameter(a,101+i),expected[i]); return 1;
    }
    if(std::fabs(a->getParameter(a,68)-(22.7673f/40.0f))>0.002f) { std::puts("FAIL UMIK Sens Factor readout default"); return 1; }
    char provisional[32]{}; a->dispatcher(a,effGetParamDisplay,63,0,provisional,0.0f);
    if(std::strcmp(provisional,"N/A")!=0) { std::puts("FAIL provisional SPL presentation"); return 1; }
    std::puts("PASS display defaults and parameter-bound ranges");
    float il[128],ir[128],ol[128]{},orr[128]{};
    for(int i=0;i<128;i++){il[i]=0.2f*std::sin(i*.11f);ir[i]=-.17f*std::cos(i*.07f);}
    float* in[]={il,ir}; float* out[]={ol,orr};
    a->processReplacing(a,in,out,128);
    for(int i=0;i<128;i++) if(ol[i]!=il[i]||orr[i]!=ir[i]) { std::puts("FAIL transparent pass-through"); return 1; }
    std::puts("PASS plugin init, UID, stereo I/O and bit-transparent pass-through");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const float levelA=a->getParameter(a,0), levelB=b->getParameter(b,0);
    if(levelA<0.70f||levelA>0.90f||std::fabs(levelA-levelB)>0.001f) {
        std::printf("FAIL reconnect/shared client parameter values: %.3f %.3f\n",levelA,levelB); return 1;
    }
    char serviceStatus[32]{};a->dispatcher(a,effGetParamDisplay,114,0,serviceStatus,0.0f);
    if(std::strcmp(serviceStatus,"EXTERNAL")!=0){std::printf("FAIL external analyzer ownership state: %s\n",serviceStatus);return 1;}
    a->setParameter(a,113,1.0f);std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const float expectedBand16=( -30.0f+10.0f*std::log10(2.0f)+11.0f+90.0f )/90.0f;
    const float expectedPeak16=( -20.0f+10.0f*std::log10(2.0f)+11.0f+90.0f )/90.0f;
    if(a->getParameter(a,31)<0.70f || std::fabs(a->getParameter(a,69)-expectedBand16)>0.02f ||
       a->getParameter(a,85)<a->getParameter(a,69)-0.005f || a->getParameter(a,85)>expectedPeak16+0.01f) {
        std::printf("FAIL service peak markers or 16-band power aggregation: p31 %.3f b16 %.3f p16 %.3f expected %.3f/%.3f\n",a->getParameter(a,31),a->getParameter(a,69),a->getParameter(a,85),expectedBand16,expectedPeak16); return 1;
    }
    std::puts("PASS offline-to-online recovery and coherent two-instance client snapshot");
    std::this_thread::sleep_for(std::chrono::milliseconds(650));
    for(AEffect* e:{a,b}) { e->dispatcher(e,effMainsChanged,0,0,nullptr,0); e->dispatcher(e,effClose,0,0,nullptr,0); }
    dlclose(lib);
    std::puts("PASS clean suspend, instance shutdown and library unload");
    return 0;
}
