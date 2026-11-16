#include "force_rta_display.h"
#include <math.h>
#include <stddef.h>

static double combine_db(double a, double b) {
    double hi=fmax(a,b), lo=fmin(a,b);
    if (!isfinite(hi)) return -120.0;
    if (!isfinite(lo)) return hi;
    return hi + 10.0*log10(1.0+pow(10.0,(lo-hi)/10.0));
}
static int led_steps(double db, double gain, double floor_db) {
    if (!isfinite(db)) return 0;
    double scaled=(db+gain-floor_db)*FORCE_RTA_LED_STEPS/(-floor_db);
    if (scaled<=0) return 0;
    if (scaled>=FORCE_RTA_LED_STEPS) return FORCE_RTA_LED_STEPS;
    return (int)floor(scaled+0.5);
}
int force_rta_render(const double *levels,const double *peaks,int mode,
                     double gain,double floor_db,force_rta_display *out) {
    if (!levels||!peaks||!out || (mode!=16&&mode!=31) ||
        !isfinite(gain)||!isfinite(floor_db)||floor_db>=0) return 0;
    out->band_count=mode;
    for(int i=0;i<FORCE_RTA_MAX_DISPLAY_BANDS;i++){
        out->led[i]=0;out->peak_led[i]=0;
    }
    for(int j=0;j<mode;j++){
        int k=(mode==31)?j:j*2;
        double v=levels[k], p=peaks[k];
        if(mode==16 && k+1<31){
            v=combine_db(v,levels[k+1]);
            p=combine_db(p,peaks[k+1]);
        }
        out->led[j]=led_steps(v,gain,floor_db);
        out->peak_led[j]=led_steps(p,gain,floor_db);
    }
    return 1;
}
