#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <initializer_list>
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
    if(argc!=2) return 2;
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
    const float expectedBand16=( -30.0f+10.0f*std::log10(2.0f)+11.0f+90.0f )/90.0f;
    const float expectedPeak16=( -25.0f+10.0f*std::log10(2.0f)+11.0f+90.0f )/90.0f;
    if(a->getParameter(a,31)<0.70f || std::fabs(a->getParameter(a,69)-expectedBand16)>0.02f ||
       std::fabs(a->getParameter(a,85)-expectedPeak16)>0.02f) {
        std::printf("FAIL service peak markers or 16-band power aggregation: p31 %.3f b16 %.3f p16 %.3f expected %.3f/%.3f\n",a->getParameter(a,31),a->getParameter(a,69),a->getParameter(a,85),expectedBand16,expectedPeak16); return 1;
    }
    std::puts("PASS offline-to-online recovery and coherent two-instance client snapshot");
    std::this_thread::sleep_for(std::chrono::milliseconds(650));
    for(AEffect* e:{a,b}) { e->dispatcher(e,effMainsChanged,0,0,nullptr,0); e->dispatcher(e,effClose,0,0,nullptr,0); }
    dlclose(lib);
    std::puts("PASS clean suspend, instance shutdown and library unload");
    return 0;
}
