#include "../src/force/force_rta_display.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  double v[31],p[31];force_rta_display d;
  for(int i=0;i<31;i++){v[i]=-90.;p[i]=-75.;}
  assert(force_rta_render(v,p,31,11,-90,&d));
  assert(d.band_count==31);
  assert(d.led[0]==3);
  assert(d.peak_led[0]>d.led[0]);
  assert(force_rta_render(v,p,16,11,-90,&d));
  assert(d.band_count==16);
  assert(d.led[0]>3); /* sum of two adjacent power bands */
  assert(d.led[15]==3); /* 20 kHz has no neighbour */
  for(int i=16;i<31;i++)assert(d.led[i]==0);
  assert(!force_rta_render(v,p,17,11,-90,&d));
  assert(!force_rta_render(v,p,31,11,0,&d));
  puts("Force display adapter tests passed");
  return 0;
}
