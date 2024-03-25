#include "synth_internal.h"

/* Cleanup.
 */
 
void synth_playback_cleanup(struct synth_playback *playback) {
  pcmprint_pcm_del(playback->pcm);
  playback->pcm=0;
}

/* Init.
 */

void synth_playback_init(struct synth *synth,struct synth_playback *playback,struct pcmprint_pcm *pcm,double trim,double pan) {
  if (pcmprint_pcm_ref(pcm)<0) return;
  playback->pcm=pcm;
  playback->p=0;
  playback->gain=trim;
}

/* Update.
 */

void synth_playback_update(float *v,int c,struct synth *synth,struct synth_playback *playback) {
  if (!playback->pcm) return;
  const float *src=playback->pcm->v+playback->p;
  int i=playback->pcm->c-playback->p;
  if (i>c) i=c;
  else c=i;
  for (;i-->0;v++,src++) (*v)+=(*src);
  playback->p+=c;
}