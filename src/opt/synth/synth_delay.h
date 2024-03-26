/* synth_delay.h
 * Self-contained tape delay.
 */
 
#ifndef SYNTH_DELAY_H
#define SYNTH_DELAY_H

struct synth_delay {
  float dry,wet,sto,fbk;
  float *v;
  int p;
  int c;
};

void synth_delay_cleanup(struct synth_delay *delay);

int synth_delay_init(struct synth_delay *delay,int framec,float mix,float feedback);

static inline float synth_delay_update(struct synth_delay *delay,float src) {
  float prv=delay->v[delay->p];
  delay->v[delay->p]=src*delay->sto+prv*delay->fbk;
  if (++(delay->p)>=delay->c) delay->p=0;
  return src*delay->dry+prv*delay->wet;
}

/* Detune operates on similar principles, so it piggybacks on the delay object.
 * At each sample, you must provide (phase) in -1..1.
 */
int synth_detune_init(struct synth_delay *delay,int framec);
float synth_detune_update(struct synth_delay *delay,float src,float phase);

#endif
