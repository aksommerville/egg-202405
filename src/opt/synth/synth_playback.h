/* synth_playback.h
 * Shovels a PCM dump on to the main output at its natural rate, with trim and pan.
 */
 
#ifndef SYNTH_PLAYBACK_H
#define SYNTH_PLAYBACK_H

struct synth_playback {
  struct sfg_pcm *pcm;
  int p;
  float gain; //TODO If we go stereo, make two gains and bake pan into them.
};

void synth_playback_cleanup(struct synth_playback *playback);

void synth_playback_init(struct synth *synth,struct synth_playback *playback,struct sfg_pcm *pcm,double trim,double pan);

void synth_playback_update(float *v,int c,struct synth *synth,struct synth_playback *playback);

static inline int synth_playback_is_defunct(const struct synth_playback *playback) {
  if (!playback->pcm) return 1;
  return (playback->p>=playback->pcm->c);
}

static inline int synth_playback_compare(
  const struct synth_playback *a,
  const struct synth_playback *b
) {
  if (!a->pcm) return -1;
  if (!b->pcm) return 1;
  return b->p-a->p; // greater (p) is older
}

#endif
