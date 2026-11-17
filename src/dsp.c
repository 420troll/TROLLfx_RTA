#include "dsp.h"
#include "third_party/pffft/pffft.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PI 3.14159265358979323846
static PFFFT_Setup *fft_setup;static float *fft_input,*fft_output,*fft_work;
static double cal_gain[DSP_FFT_SIZE/2],a_gain[DSP_FFT_SIZE/2],bin_power[DSP_FFT_SIZE/2],a_bin_power[DSP_FFT_SIZE/2];
static const calibration_point *cached_cal=NULL;static size_t cached_caln=0,cached_n=0;static double cached_fs=0;
static const double centers[DSP_BANDS]={20,25,31.5,40,50,63,80,100,125,160,200,250,315,400,500,630,800,1000,1250,1600,2000,2500,3150,4000,5000,6300,8000,10000,12500,16000,20000};

int dsp_init(void){
    if(fft_setup)return 1;
    fft_setup=pffft_new_setup(DSP_FFT_SIZE,PFFFT_REAL);if(!fft_setup)return 0;
    fft_input=pffft_aligned_malloc(DSP_FFT_SIZE*sizeof(float));fft_output=pffft_aligned_malloc(DSP_FFT_SIZE*sizeof(float));fft_work=pffft_aligned_malloc(DSP_FFT_SIZE*sizeof(float));
    if(!fft_input||!fft_output||!fft_work){pffft_aligned_free(fft_input);pffft_aligned_free(fft_output);pffft_aligned_free(fft_work);pffft_destroy_setup(fft_setup);fft_setup=NULL;return 0;}
    return 1;
}

void dsp_analyze(const float *x,size_t count,double fs,const calibration_point *cal,size_t caln,double bands[DSP_BANDS],double *rms_dbfs,double *z_corrected_dbfs,double *a_weighted_dbfs) {
    if(!x||count!=DSP_FFT_SIZE){for(int b=0;b<DSP_BANDS;b++)bands[b]=-120;if(rms_dbfs)*rms_dbfs=-120;if(z_corrected_dbfs)*z_corrected_dbfs=-120;if(a_weighted_dbfs)*a_weighted_dbfs=-120;return;}
    if(!dsp_init()){for(int b=0;b<DSP_BANDS;b++)bands[b]=-120;if(rms_dbfs)*rms_dbfs=-120;if(z_corrected_dbfs)*z_corrected_dbfs=-120;if(a_weighted_dbfs)*a_weighted_dbfs=-120;return;}
    double wsum2=0,raw=0;
    for(size_t i=0;i<count;i++){double w=.5-.5*cos(2*PI*i/(count-1));fft_input[i]=(float)(x[i]*w);wsum2+=w*w;raw+=(double)x[i]*x[i];}
    if(rms_dbfs)*rms_dbfs=fmax(-120,10*log10(fmax(raw/count,1e-24)));
    pffft_transform_ordered(fft_setup,fft_input,fft_output,fft_work,PFFFT_FORWARD);
    if(cal!=cached_cal||caln!=cached_caln||count!=cached_n||fs!=cached_fs){
        for(size_t k=1;k<count/2;k++){double f=(double)k*fs/count;cal_gain[k]=pow(10,-dsp_calibration_db(cal,caln,f)/20);a_gain[k]=pow(10,dsp_a_weight_db(f)/20);}
        cached_cal=cal;cached_caln=caln;cached_n=count;cached_fs=fs;
    }
    double ze=0,ae=0;
    for(size_t k=1;k<count/2;k++){double f=(double)k*fs/count,re=fft_output[2*k],im=fft_output[2*k+1],rawp=2*(re*re+im*im)/(count*wsum2),p=rawp*cal_gain[k]*cal_gain[k];bin_power[k]=p;a_bin_power[k]=p*a_gain[k]*a_gain[k];if(f>=20.0&&f<=20000.0){ze+=p;ae+=a_bin_power[k];}}
    if(z_corrected_dbfs)*z_corrected_dbfs=fmax(-120,10*log10(fmax(ze,1e-24)));
    if(a_weighted_dbfs)*a_weighted_dbfs=fmax(-120,10*log10(fmax(ae,1e-24)));
    for(int b=0;b<DSP_BANDS;b++){
        double lo=centers[b]/pow(2,1.0/6),hi=centers[b]*pow(2,1.0/6),energy=0;
        /* Integrate one-sided power. Split edge-bin energy by fractional overlap. */
        double bw=fs/count;
        size_t k0=(size_t)fmax(1,ceil(lo/bw-.5)),k1=(size_t)fmin(count/2-1,floor(hi/bw+.5));
        for(size_t k=k0;k<=k1;k++){double f=(double)k*bw;
            double left=f-bw/2,right=f+bw/2,over=fmin(right,hi)-fmax(left,lo);
            if(over>0){double fraction=fmin(1.0,over/bw);energy+=bin_power[k]*fraction;}}
        bands[b]=fmax(-120,10*log10(fmax(energy,1e-24)));
    }
}

size_t dsp_load_calibration(const char *path,calibration_point *points,size_t cap,double *sens){
    FILE *f=fopen(path,"r");if(!f)return 0;char line[256];size_t n=0;double factor=NAN;
    while(fgets(line,sizeof line,f)){
        char *sf=strstr(line,"Sens Factor");if(sf){char *eq=strchr(sf,'=');if(eq){char *end;double v=strtod(eq+1,&end);if(end!=eq+1)factor=v;}continue;}
        char *p=line,*end;double hz=strtod(p,&end);if(end==p||hz<=0)continue;p=end;double db=strtod(p,&end);if(end==p||!isfinite(db))continue;
        if(n<cap)points[n++]=(calibration_point){hz,db};
    }
    fclose(f);if(sens)*sens=factor;return n;
}

double dsp_calibration_db(const calibration_point *p,size_t n,double hz) {
    if(!p||n==0||hz<=0)return 0;
    if(hz<=p[0].hz)return p[0].db;
    if(hz>=p[n-1].hz)return p[n-1].db;
    size_t lo=0,hi=n-1;while(hi-lo>1){size_t m=lo+(hi-lo)/2;if(hz<p[m].hz)hi=m;else lo=m;}
    double t=log(hz/p[lo].hz)/log(p[hi].hz/p[lo].hz);return p[lo].db+t*(p[hi].db-p[lo].db);
}

double dsp_a_weight_db(double f) {
    if(f<=0)return -INFINITY;
    double f2=f*f, c1=20.6*20.6,c2=107.7*107.7,c3=737.9*737.9,c4=12194.0*12194.0;
    double ra=(c4*f2*f2)/((f2+c1)*sqrt((f2+c2)*(f2+c3))*(f2+c4));
    return 20*log10(ra)+2.0;
}
double dsp_spl_db(double rms_dbfs,double sensitivity_dbfs_at_94_db_spl){return 94.0+rms_dbfs-sensitivity_dbfs_at_94_db_spl;}
