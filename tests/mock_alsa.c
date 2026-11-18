#define _DEFAULT_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct _snd_pcm { int unused; } snd_pcm_t;
static unsigned long frame_counter;
static void count_open(void) {
    const char *path=getenv("MOCK_ALSA_OPEN_COUNT");if(!path)return;
    FILE*f=fopen(path,"a");if(f){fputs("open\n",f);fclose(f);}
}
int snd_pcm_open(snd_pcm_t **p,const char*name,int stream,int mode){(void)name;(void)stream;(void)mode;*p=calloc(1,sizeof(**p));count_open();return *p?0:-12;}
int snd_pcm_format_value(const char*name){(void)name;return 1;}
int snd_pcm_set_params(snd_pcm_t*p,int format,int access,unsigned channels,unsigned rate,int soft_resample,unsigned latency){(void)p;(void)format;(void)access;(void)channels;(void)rate;(void)soft_resample;(void)latency;return 0;}
int snd_pcm_prepare(snd_pcm_t*p){(void)p;return 0;}
int snd_pcm_start(snd_pcm_t*p){(void)p;return 0;}
long snd_pcm_readi(snd_pcm_t*p,void*buffer,unsigned long frames){(void)p;uint8_t*out=buffer;for(unsigned long i=0;i<frames;i++){
    int32_t v=((int32_t)((frame_counter+i)%2000)-1000)*100;for(int c=0;c<2;c++){int32_t q=c?-v:v;uint32_t u=(uint32_t)q&0xffffff;out[(i*2+c)*3]=u;out[(i*2+c)*3+1]=u>>8;out[(i*2+c)*3+2]=u>>16;}}
    frame_counter+=frames;struct timespec pause={0,20000000};nanosleep(&pause,0);return (long)frames;}
int snd_pcm_drop(snd_pcm_t*p){(void)p;return 0;}
int snd_pcm_close(snd_pcm_t*p){free(p);return 0;}
const char*snd_strerror(int e){(void)e;return "mock error";}
