#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#include "synth.h"
#include "opt/romr/romr.h"
#include "opt/midi/midi.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#define SYNTH_QBUF_SIZE 1024
#define SYNTH_QLEVEL_DEFAULT 32000.0f
#define SYNTH_UPDATE_LIMIT 1024 /* Samples, regardless of chanc and rate. (ie limits memory exposure only) */
#define SYNTH_CHANNEL_COUNT 8

#define SYNTH_WAVE_SIZE_BITS 10
#define SYNTH_WAVE_SIZE_SAMPLES (1<<SYNTH_WAVE_SIZE_BITS)
#define SYNTH_WAVE_SHIFT (32-SYNTH_WAVE_SIZE_BITS)

/* Envelope.
 ***********************************************************************/
 
struct synth_env {
  int ttl;
  float v;
  float dv;
  int stage; // (0,1,2,3,4)=(attack,decay,sustain,release,finished)
  float iniv,atkv,decv,susv,rlsv;
  int     atkt,dect,sust,rlst;
};

/* Config times are in frames, values are 0..1 usually, but can be anything.
 * We don't have a concept of sustain in the usual sense: You establish a sustain time in advance.
 */
struct synth_env_config {
  float inivlo,inivhi; // normally zero
  int   atktlo,atkthi;
  float atkvlo,atkvhi;
  int   dectlo,decthi;
  float decvlo,decvhi;
  int   sustlo,susthi; // can be overridden at instantiate
  float susvlo,susvhi; // normally same as (decv)
  int   rlstlo,rlsthi;
  float rlsvlo,rlsvhi; // normally zero
};

/* Create a level envelope from 5 easily-encoded parameters.
 * There are opinionated constants involved, and we try to ensure that any of the 
 * trillion possible values produces a sensible envelope.
 */
void synth_env_config_init_level(
  struct synth *synth,struct synth_env_config *config,
  uint8_t atkt, // attack time, in vague units
  uint8_t rlst, // release time, in vague units
  uint8_t peak, // attack level
  uint8_t drop, // decay+sustain level relative to attack
  uint8_t velsens // constant..sensitive to velocity
);

/* Create a parameter envelope, eg for FM range.
 * (reference) is a finished config; we'll copy its timing exactly.
 * You provide exact levels for each control point at maximum velocity;
 * (varation) is a multiplier that yields the low values.
 */
void synth_env_config_init_param(
  struct synth_env_config *config,
  const struct synth_env_config *reference,
  float iniv,float atkv,float decv,float susv,float rlsv,
  float variation
);

void synth_env_config_gain(struct synth_env_config *config,float gain);

void synth_env_init(struct synth_env *env,const struct synth_env_config *config,uint8_t velocity);

// Multiply all values.
void synth_env_gain(struct synth_env *env,float gain);

static inline int synth_env_is_finished(const struct synth_env *env) {
  return (env->stage>=4);
}

// Shorten sustain time as far as we can.
void synth_env_release(struct synth_env *env);

// Let synth_env_next() call this; don't do it yourself.
void synth_env_advance(struct synth_env *env);

static inline float synth_env_next(struct synth_env *env) {
  float v=env->v;
  if (env->ttl-->0) env->v+=env->dv;
  else synth_env_advance(env);
  return v;
}

/* Signal-generating objects.
 ********************************************************************/

#define SYNTH_VOICE_LIMIT 16
#define SYNTH_PROC_LIMIT 16
#define SYNTH_PLAYBACK_LIMIT 32

#define SYNTH_ORIGIN_USER 1
#define SYNTH_ORIGIN_SONG 2

/* Voice is a tuned note with no postprocessing, following a generic form.
 */
struct synth_voice {
  uint8_t chid; // 0xff if not addressable
  uint8_t noteid;
  uint8_t origin; // also serves as the 'defunct' flag, 0=defunct
  int birthday; // (synth->framec) at the time of initialization
  uint32_t p;
  uint32_t dp;
  uint32_t dp0; // before wheel
  struct synth_env level;
  const float *wave;
  struct synth_env rock; // rock or mod, not both
  uint32_t modp;
  uint32_t moddp;
  struct synth_env modrange;
};

/* Proc is like voice, a signal generator, but with a custom signal hook.
 */
struct synth_proc {
  uint8_t chid; // 0xff if not addressable
  uint8_t noteid; // 0xff if permanently attached (associated with the channel, not a note)
  uint8_t origin;
  int birthday; // (synth->framec) at the time of initialization
  void (*del)(struct synth *synth,struct synth_proc *proc);
  void (*update)(float *v,int c,struct synth *synth,struct synth_proc *proc);
  void (*note)(struct synth *synth,struct synth_proc *proc,uint8_t noteid,uint8_t velocity,int dur);
  void (*wheel)(struct synth *synth,struct synth_proc *proc,uint8_t v);
  void *userdata;
};

/* Playback is only for verbatim PCM playback at its natural rate.
 */
struct synth_playback {
  const float *src; // WEAK
  int srcc; // also serves as 'defunct' flag
  int srcp;
  float ltrim,rtrim; // Combined trim and pan.
};

/* Channels.
 ****************************************************************/
 
struct synth_channel {
  uint8_t chid; // 0..7, same as our index in the global list.
  uint8_t pid;
  float trim; // 0..1
  float pan; // -1..1
  
  /* For custom channels, don't bother setting the fields below.
   * Instead add a proc to the graph, with (chid) set and (noteid==0xff).
   * It will receive note and wheel events.
   * If you want standalone notes with wavetable or FM, use the fields below.
   */
  struct synth_env_config level;
  struct synth_env_config rock;
  uint8_t wheel;
  float wheel_range; // cents
  float bend; // multiplier
  float modrel; // multiplier
  struct synth_env_config modrange;
};

/* Global context.
 **********************************************************************/

struct synth {
  struct romr *romr; // OPTIONAL, WEAK
  int rate,chanc;
  double frames_per_ms;
  int framec; // global frame counter (overflows every 13 hours or so, at 44.1 kHz)
  
  float qlevel;
  float *qbuf;
  int qbufa;
  
  int preprint_framec;
  //TODO PCM printers
  
  int songqual;
  int songid;
  const uint8_t *songserial; // WEAK
  int songserialc;
  void *songkeepalive; // STRONG
  int songp; // Bytes into songserial (mind that songserial includes the header). If zero, song is stopped.
  int songrepeat;
  int songdelay; // frames
  int songtransition; // If nonzero, we are draining voices before starting the partially-loaded song.
  
  struct synth_channel channelv[SYNTH_CHANNEL_COUNT];
  
  struct synth_voice voicev[SYNTH_VOICE_LIMIT];
  int voicec;
  
  struct synth_proc procv[SYNTH_PROC_LIMIT];
  int procc;
  
  struct synth_playback playbackv[SYNTH_PLAYBACK_LIMIT];
  int playbackc;
  
  float freqn_by_noteid[128]; // 0..1
  uint32_t freqi_by_noteid[128]; // 0..0xffffffff
  float sine[SYNTH_WAVE_SIZE_SAMPLES];
  float waves[SYNTH_WAVE_SIZE_SAMPLES*SYNTH_CHANNEL_COUNT];
};

void synth_end_song(struct synth *synth);
int synth_has_song_voices(const struct synth *synth);
void synth_complete_song_transition(struct synth *synth);

void synth_updatef_mono(float *v,int c,struct synth *synth);

void synth_play_note(struct synth *synth,uint8_t chid,uint8_t noteid,uint8_t velocity,int dur_frames);
void synth_turn_wheel(struct synth *synth,uint8_t chid,uint8_t v);

/* Arrange for a voice or proc to terminate soon, or force it.
 */
void synth_voice_release(struct synth *synth,struct synth_voice *voice);
void synth_proc_release(struct synth *synth,struct synth_proc *proc);

/* Get a new object.
 * Our policy is that new objects get precedence, so if the set is full, we'll kill an older one to make room.
 * The object you get back is initially defunct.
 * No errors.
 */
struct synth_voice *synth_voice_new(struct synth *synth);
struct synth_proc *synth_proc_new(struct synth *synth);
struct synth_playback *synth_playback_new(struct synth *synth);

void synth_apply_channel_header(struct synth *synth,int chid,const uint8_t *src);

#endif
