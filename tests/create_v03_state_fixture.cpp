#include <dlfcn.h>
#include <cstdio>
#include <vector>
#include "mpc_vst2_abi.h"

static VstIntPtr master(AEffect*,VstInt32 opcode,VstInt32,VstIntPtr,void*,float){return opcode==audioMasterVersion?2400:1;}
int main(int argc,char**argv){
    if(argc!=3)return 2;
    void*lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){std::fprintf(stderr,"dlopen: %s\n",dlerror());return 1;}
    using Entry=AEffect*(*)(audioMasterCallback);auto entry=reinterpret_cast<Entry>(dlsym(lib,"VSTPluginMain"));if(!entry)return 1;
    AEffect* plugin=entry(master);if(!plugin)return 1;plugin->dispatcher(plugin,effOpen,0,0,nullptr,0);
    plugin->setParameter(plugin,101,(15.0f+12.0f)/48.0f);
    plugin->setParameter(plugin,110,(42.0f-2.0f)/58.0f);
    void* chunk=nullptr;const VstIntPtr size=plugin->dispatcher(plugin,effGetChunk,0,0,&chunk,0.0f);
    if(size<=0||!chunk)return 1;
    FILE*out=std::fopen(argv[2],"wb");if(!out)return 1;
    const bool ok=std::fwrite(chunk,1,(size_t)size,out)==(size_t)size;std::fclose(out);
    plugin->dispatcher(plugin,effClose,0,0,nullptr,0);dlclose(lib);return ok?0:1;
}
