/* synth_filter.h
 * IIR and FIR helpers.
 */
 
#ifndef SYNTH_FILTER_H
#define SYNTH_FILTER_H

struct synth_filter {
  float coefv[5]; // constant
  float statev[5]; // volatile
};

/* It's safe to use IIR update if you're not sure.
 * But if you know it's an FIR, use synth_filter_fir_update to avoid some redundant arithmetic.
 * ...actually everything I implemented is IIR.
 */
static inline float synth_filter_iir_update(struct synth_filter *filter,float src) {
  filter->statev[2]=filter->statev[1];
  filter->statev[1]=filter->statev[0];
  filter->statev[0]=src;
  src=(
    filter->statev[0]*filter->coefv[0]+
    filter->statev[1]*filter->coefv[1]+
    filter->statev[2]*filter->coefv[2]+
    filter->statev[3]*filter->coefv[3]+
    filter->statev[4]*filter->coefv[4]
  );
  filter->statev[4]=filter->statev[3];
  filter->statev[3]=src;
  return src;
}
static inline float synth_filter_fir_update(struct synth_filter *filter,float src) {
  filter->statev[2]=filter->statev[1];
  filter->statev[1]=filter->statev[0];
  filter->statev[0]=src;
  return (
    filter->statev[0]*filter->coefv[0]+
    filter->statev[1]*filter->coefv[1]+
    filter->statev[2]*filter->coefv[2]
  );
}

void synth_filter_init_lopass(struct synth_filter *filter,float freq);
void synth_filter_init_hipass(struct synth_filter *filter,float freq);
void synth_filter_init_bandpass(struct synth_filter *filter,float freq,float width);
void synth_filter_init_notch(struct synth_filter *filter,float freq,float width);

#endif
