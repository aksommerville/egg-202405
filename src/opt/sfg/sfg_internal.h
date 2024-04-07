#ifndef SFG_INTERNAL_H
#define SFG_INTERNAL_H

#include "sfg.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>

#define SFG_WAVE_SIZE_BITS 10
#define SFG_WAVE_SIZE_SAMPLES (1<<SFG_WAVE_SIZE_BITS)
#define SFG_WAVE_SHIFT (32-SFG_WAVE_SIZE_BITS)

// Updates are fragmented to no longer than this, so we can employ fixed-size buffers internally.
#define SFG_BUFFER_SIZE 256

struct sfg_env {
  float v;
  float dv;
  int ttl;
  int pointp;
  float v0;
  struct sfg_env_point {
    int t;
    float v;
  } *pointv;
  int pointc,pointa;
};

struct sfg_op {
  void (*update)(float *v,int c,struct sfg_op *op);
  struct sfg_env env;
  int iv[2];
  float fv[10];
  float *buf;
};

struct sfg_voice {
  uint8_t shape;
  float wave[SFG_WAVE_SIZE_SAMPLES];
  struct sfg_env rate;
  float ratelfop;
  float ratelfodp;
  float ratelforange;
  struct sfg_env range;
  float fmrate;
  float modp;
  float carp;
  uint32_t carpi,cardpi; // for 'flat' oscillator
  void (*oscillate)(float *v,int c,struct sfg_voice *voice);
  struct sfg_op *opv;
  int opc,opa;
};

struct sfg_printer {
  struct sfg_pcm *pcm;
  int pcmp;
  int rate;
  float master;
  struct sfg_voice *voicev;
  int voicec,voicea;
};

void sfg_voice_cleanup(struct sfg_voice *voice);

/* Prepare printer from encoded sound.
 * Allocates (printer->pcm).
 */
int sfg_printer_decode(struct sfg_printer *printer,const uint8_t *src,int srcc);

int sfg_env_decode(struct sfg_env *env,const uint8_t *src,int srcc,int rate,float scale);
void sfg_env_constant(struct sfg_env *env,float v);
void sfg_env_advance(struct sfg_env *env); // only sfg_env_update should call this

static inline float sfg_env_update(struct sfg_env *env) {
  if (env->ttl-->0) env->v+=env->dv;
  else sfg_env_advance(env);
  return env->v;
}

/* A "silence" oscillator exists and works, but it will never actually be used.
 * Voices using it get dropped during decode.
 */
void sfg_oscillate_silence(float *v,int c,struct sfg_voice *voice);
void sfg_oscillate_noise(float *v,int c,struct sfg_voice *voice);
void sfg_oscillate_flat(float *v,int c,struct sfg_voice *voice);
void sfg_oscillate_lfno(float *v,int c,struct sfg_voice *voice);
void sfg_oscillate_full(float *v,int c,struct sfg_voice *voice);

void sfg_op_level_update(float *v,int c,struct sfg_op *op);
void sfg_op_gain_update(float *v,int c,struct sfg_op *op);
void sfg_op_clip_update(float *v,int c,struct sfg_op *op);
void sfg_op_delay_update(float *v,int c,struct sfg_op *op);
void sfg_op_filter_update(float *v,int c,struct sfg_op *op);

#endif
