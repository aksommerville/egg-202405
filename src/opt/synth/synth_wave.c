#include "synth_internal.h"

/* New wave from harmonics.
 */
 
static void synth_wave_print_harmonic(float *dst,const float *src,float level,int step) {
  int srcp=0,i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;dst++,srcp+=step) {
    if (srcp>=SYNTH_WAVE_SIZE_SAMPLES) srcp-=SYNTH_WAVE_SIZE_SAMPLES;
    (*dst)+=src[srcp]*level;
  }
}
 
float *synth_wave_new_harmonics(const struct synth *synth,const uint8_t *coefv,int coefc) {
  float *dst=calloc(sizeof(float),SYNTH_WAVE_SIZE_SAMPLES);
  if (!dst) return 0;
  if (coefc>SYNTH_HARMONICS_LIMIT) coefc=SYNTH_HARMONICS_LIMIT;
  int step=1;
  for (;coefc-->0;coefv++,step++) {
    if (!*coefv) continue;
    synth_wave_print_harmonic(dst,synth->sine,(*coefv)/255.0f,step);
  }
  return dst;
}
