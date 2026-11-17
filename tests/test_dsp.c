#define _POSIX_C_SOURCE 200809L
#include "../src/dsp.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FS 48000.0
#define PI 3.14159265358979323846
static float *sig;
static int failures=0;
static void check(int ok,const char *name){printf("%s %s\n",ok?"PASS":"FAIL",name);if(!ok)failures++;}
static void tone(double hz,double amp){for(int i=0;i<DSP_FFT_SIZE;i++)sig[i]=(float)(amp*sin(2*PI*hz*i/FS));}
int main(void){sig=malloc(sizeof(float)*DSP_FFT_SIZE);double db[DSP_BANDS],rms;
    const double ts[]={50,100,1000,5000,10000};
    const int expect[]={4,7,17,24,27};
    for(int t=0;t<5;t++){tone(ts[t],.5);dsp_analyze(sig,DSP_FFT_SIZE,FS,NULL,0,db,&rms,NULL,NULL);int mx=0;for(int b=1;b<DSP_BANDS;b++)if(db[b]>db[mx])mx=b;char n[80];snprintf(n,sizeof n,"%.0f Hz band location",ts[t]);check(mx==expect[t],n);}
    tone(1000,.5);dsp_analyze(sig,DSP_FFT_SIZE,FS,NULL,0,db,&rms,NULL,NULL);check(fabs(rms-20*log10(.5/sqrt(2)))<.05,"Hann power normalization RMS");
    int count=0;for(int i=0;i<DSP_BANDS;i++)if(db[i]>-45)count++;check(count<=3,"1 kHz tone confined to adjacent bands");
    memset(sig,0,sizeof(float)*DSP_FFT_SIZE);dsp_analyze(sig,DSP_FFT_SIZE,FS,NULL,0,db,&rms,NULL,NULL);check(rms==-120,"silence numerical floor");
    srand(1701);for(int i=0;i<DSP_FFT_SIZE;i++)sig[i]=(float)((rand()/(double)RAND_MAX)*2-1);
    dsp_analyze(sig,DSP_FFT_SIZE,FS,NULL,0,db,&rms,NULL,NULL);double sum=0;for(int i=0;i<DSP_BANDS;i++)sum+=db[i];check(isfinite(sum)&&rms < -4 && rms > -7,"deterministic broadband noise RMS");
    tone(1000.3,.5);dsp_analyze(sig,DSP_FFT_SIZE,FS,NULL,0,db,&rms,NULL,NULL);int leakage=0;for(int i=0;i<DSP_BANDS;i++)if(db[i]>-45)leakage++;check(db[17]>-12&&leakage<=3,"off-bin sine leakage and normalization");
    for(int i=0;i<DSP_FFT_SIZE;i++)sig[i]=(float)(.25*sin(2*PI*100*i/FS)+.25*sin(2*PI*5000*i/FS));
    dsp_analyze(sig,DSP_FFT_SIZE,FS,NULL,0,db,&rms,NULL,NULL);check(db[7]>-18&&db[24]>-18&&db[17]<-35,"multiple simultaneous tones");
    for(int i=0;i<DSP_FFT_SIZE;i++){double t=(double)i/(DSP_FFT_SIZE-1),duration=DSP_FFT_SIZE/FS;double phase=2*PI*100*duration/log(100)*(pow(100,t)-1);sig[i]=(float)(.4*sin(phase));}dsp_analyze(sig,DSP_FFT_SIZE,FS,NULL,0,db,&rms,NULL,NULL);int sweep_bands=0;for(int i=0;i<DSP_BANDS;i++)if(db[i]>-45)sweep_bands++;check(sweep_bands>=12,"log sweep spans expected bands");
    double pink=0;for(int i=0;i<DSP_FFT_SIZE;i++){pink=.985*pink+.015*((rand()/(double)RAND_MAX)*2-1);sig[i]=(float)pink;}dsp_analyze(sig,DSP_FFT_SIZE,FS,NULL,0,db,&rms,NULL,NULL);check(isfinite(db[4])&&isfinite(db[27])&&db[4]>db[27],"pink noise slopes toward low frequencies");
    calibration_point real0[1024],real90[1024];double sf0=NAN,sf90=NAN;size_t n0=dsp_load_calibration("calibration/7033298.txt",real0,1024,&sf0),n90=dsp_load_calibration("calibration/7033298_90deg.txt",real90,1024,&sf90);check(n0>600&&n90>600,"provided UMIK calibration curves parse");check(fabs(sf0-2.7673)<1e-5&&fabs(sf90-2.7673)<1e-5,"both UMIK orientation files retain Sens Factor");check(real0[n0-1].hz==real90[n90-1].hz&&real0[n0-1].db!=real90[n90-1].db,"0 and 90 degree files remain distinct");check(fabs(dsp_calibration_db(real0,n0,5000)-dsp_calibration_db(real90,n90,5000))>.5,"orientation response curves differ at 5 kHz");tone(5000,.5);double b0[DSP_BANDS],b90[DSP_BANDS];dsp_analyze(sig,DSP_FFT_SIZE,FS,real0,n0,b0,&rms,NULL,NULL);dsp_analyze(sig,DSP_FFT_SIZE,FS,real90,n90,b90,&rms,NULL,NULL);check(fabs((b0[24]-b90[24])-(dsp_calibration_db(real90,n90,5000)-dsp_calibration_db(real0,n0,5000)))<.2,"selected calibration orientation corrects spectrum");
    calibration_point c0[]={{100,-2},{1000,0},{10000,4}},c90[]={{100,1},{1000,2},{10000,3}};check(fabs(dsp_calibration_db(c0,3,1000))<1e-10,"0-degree calibration exact knot");check(fabs(dsp_calibration_db(c0,3,sqrt(100*1000))+1)<1e-9,"log-frequency calibration interpolation");check(dsp_calibration_db(c0,3,1000)!=dsp_calibration_db(c90,3,1000),"orientation curve selection changes correction");
    check(fabs(dsp_spl_db(-20,-40)-114)<1e-9,"SPL sensitivity mapping");check(fabs(dsp_a_weight_db(1000))<.02,"A weighting 1 kHz reference");
    tone(1000,.1);double a_dbfs;for(int i=0;i<DSP_FFT_SIZE;i++)sig[i]=(float)(.5*sin(2*PI*100*i/FS));dsp_analyze(sig,DSP_FFT_SIZE,FS,NULL,0,db,&rms,NULL,&a_dbfs);check(a_dbfs<rms-15,"A weighting attenuation below 1 kHz");
    tone(1000,.1);struct timespec a,b;clock_gettime(CLOCK_MONOTONIC,&a);for(int i=0;i<5;i++)dsp_analyze(sig,DSP_FFT_SIZE,FS,NULL,0,db,&rms,NULL,NULL);clock_gettime(CLOCK_MONOTONIC,&b);double ms=(b.tv_sec-a.tv_sec)*1000+(b.tv_nsec-a.tv_nsec)/1e6;printf("BENCH 32768 point FFT+bands: %.3f ms/frame (host; 5 frames)\n",ms/5);
    free(sig);return failures?1:0;
}
