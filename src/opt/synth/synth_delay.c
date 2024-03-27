#include "synth_internal.h"

/* Cleanup.
 */
 
void synth_delay_cleanup(struct synth_delay *delay) {
  if (delay->v) free(delay->v);
  delay->v=0;
  delay->c=0;
}

/* Init.
 */

int synth_delay_init(struct synth_delay *delay,int framec,float mix,float feedback) {
  if (framec<1) framec=1;
  if (!(delay->v=calloc(sizeof(float),framec))) return -1;
  delay->c=framec;
  delay->p=0;
  if (mix>=1.0f) {
    delay->wet=1.0f;
    delay->dry=0.0f;
  } else if (mix<=0.0f) {
    delay->wet=0.0f;
    delay->dry=1.0f;
  } else {
    delay->wet=mix;
    delay->dry=1.0f-mix;
  }
  if (feedback>=1.0f) {
    delay->fbk=1.0f;
    delay->sto=0.0f;
  } else if (feedback<=0.0f) {
    delay->fbk=0.0f;
    delay->sto=1.0f;
  } else {
    delay->fbk=feedback;
    delay->sto=1.0f-feedback;
  }
  return 0;
}

/* Detune.
 */
 
int synth_detune_init(struct synth_delay *delay,int framec) {
  if (synth_delay_init(delay,framec,0.0f,0.0f)<0) return -1;
  delay->wet=framec*0.5f;
  return 0;
}

float synth_detune_update(struct synth_delay *delay,float src,float phase) {
  int r=(int)lround((phase+1.0f)*delay->wet);
  if (r<0) r=0;
  else if (r>=delay->c) r=delay->c-1;
  r=delay->p-r;
  if (r<0) r+=delay->c;
  delay->v[delay->p]=src;
  if (++(delay->p)>=delay->c) delay->p=0;
  return delay->v[r];
}
