#define _POSIX_C_SOURCE 200809L
#include "../src/dsp.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define OLD_N 8192
#define PI 3.14159265358979323846
static double elapsed_ms(struct timespec a,struct timespec b){return (b.tv_sec-a.tv_sec)*1000.0+(b.tv_nsec-a.tv_nsec)/1e6;}
/* v0.1.2 scalar FFT and bin-magnitude band calculation, retained as benchmark reference. */
static void old_fft(float *re,float *im){unsigned j=0;for(unsigned i=1;i<OLD_N;i++){unsigned b=OLD_N>>1;while(j&b){j^=b;b>>=1;}j^=b;if(j>i){float t=re[i];re[i]=re[j];re[j]=t;t=im[i];im[i]=im[j];im[j]=t;}}for(unsigned len=2;len<=OLD_N;len*=2){double c=cos(-2*PI/len),s=sin(-2*PI/len);for(unsigned i=0;i<OLD_N;i+=len){double wr=1,wi=0;for(unsigned k=0;k<len/2;k++){unsigned a=i+k,b=a+len/2;double tr=wr*re[b]-wi*im[b],ti=wr*im[b]+wi*re[b];re[b]=re[a]-tr;im[b]=im[a]-ti;re[a]+=tr;im[a]+=ti;double w=wr*c-wi*s;wi=wr*s+wi*c;wr=w;}}}}
static void old_frame(const float*x,float*re,float*im){static const double hz[31]={20,25,31.5,40,50,63,80,100,125,160,200,250,315,400,500,630,800,1000,1250,1600,2000,2500,3150,4000,5000,6300,8000,10000,12500,16000,20000};for(int i=0;i<OLD_N;i++){re[i]=x[i]*(float)(.5-.5*cos(2*PI*i/(OLD_N-1)));im[i]=0;}old_fft(re,im);volatile double sink=0;for(int b=0;b<31;b++){int lo=(int)ceil(hz[b]/pow(2,1./6.)*OLD_N/48000.),hi=(int)floor(hz[b]*pow(2,1./6.)*OLD_N/48000.);if(lo<1)lo=1;if(hi<lo)hi=lo;if(hi>=OLD_N/2)hi=OLD_N/2;double sum=0;int n=0;for(int k=lo;k<=hi;k++){sum+=re[k]*re[k]+im[k]*im[k];n++;}sink+=20*log10(fmax(sqrt(sum/n)/(OLD_N*.5),1e-9));}(void)sink;}
int main(void){float *old=malloc(sizeof(float)*OLD_N),*cur=malloc(sizeof(float)*DSP_FFT_SIZE),*re=malloc(sizeof(float)*OLD_N),*im=malloc(sizeof(float)*OLD_N);double bands[DSP_BANDS],rms;for(int i=0;i<OLD_N;i++)old[i]=(float)sin(2*PI*1000*i/48000);for(int i=0;i<DSP_FFT_SIZE;i++)cur[i]=(float)sin(2*PI*1000*i/48000);const int loops=100;struct timespec a,b;clock_gettime(CLOCK_MONOTONIC,&a);for(int i=0;i<loops;i++)old_frame(old,re,im);clock_gettime(CLOCK_MONOTONIC,&b);double old_ms=elapsed_ms(a,b)/loops;clock_gettime(CLOCK_MONOTONIC,&a);for(int i=0;i<loops;i++)dsp_analyze(cur,DSP_FFT_SIZE,48000,NULL,0,bands,&rms,NULL,NULL);clock_gettime(CLOCK_MONOTONIC,&b);double new_ms=elapsed_ms(a,b)/loops;printf("Host benchmark, %d frames: V0.1.2 8192-point %.3f ms/frame; V0.2 32768-point %.3f ms/frame (%.2fx CPU/frame; 4x samples)\n",loops,old_ms,new_ms,new_ms/old_ms);free(old);free(cur);free(re);free(im);return 0;}
