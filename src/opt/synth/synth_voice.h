/* synth_voice.h
 * Single tuned voice for simple cases like wavetable and FM, with no LFO or postprocessing.
 * My hope is that most instruments will be able to use this, it's very efficient.
 * These structs live directly on the context.
 */
 
#ifndef SYNTH_VOICE_H
#define SYNTH_VOICE_H

struct synth_voice {
  uint8_t chid;
  uint8_t noteid;
  uint8_t origin; // zero for defunct
  int64_t birthday;
  int mode; // SYNTH_CHANNEL_MODE_*
  uint32_t p;
  uint32_t dp;
  uint32_t dp0; // before wheel
  int ttl; // BLIP
  float bliplevel; // BLIP
  struct synth_env level; // Everything except BLIP
  const float *wave;
  struct synth_env param0;
  uint32_t modp;
  uint32_t moddp;
  struct synth_filter filter1;
  struct synth_filter filter2;
};

void synth_voice_cleanup(struct synth_voice *voice);

void synth_voice_begin(struct synth *synth,struct synth_voice *voice,struct synth_channel *channel,uint8_t noteid,uint8_t velocity,int dur);

void synth_voice_release(struct synth *synth,struct synth_voice *voice);

void synth_voice_update(float *v,int c,struct synth *synth,struct synth_voice *voice);

static inline int synth_voice_is_channel(const struct synth_voice *voice,uint8_t chid) {
  return (voice->chid==chid);
}

static inline int synth_voice_is_song(const struct synth_voice *voice) {
  return (voice->origin==SYNTH_ORIGIN_SONG);
}

static inline int synth_voice_is_defunct(const struct synth_voice *voice) {
  return !voice->origin;
}

static inline int synth_voice_compare(
  const struct synth_voice *a,
  const struct synth_voice *b
) {
  return a->birthday-b->birthday;
}

#endif
