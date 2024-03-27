#ifndef PCMPRINT_INTERNAL_H
#define PCMPRINT_INTERNAL_H

#include "pcmprint.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define PCMPRINT_WAVE_SIZE_BITS 10
#define PCMPRINT_WAVE_SIZE_SAMPLES (1<<PCMPRINT_WAVE_SIZE_BITS)
#define PCMPRINT_WAVE_SHIFT (32-PCMPRINT_WAVE_SIZE_BITS)

// Completely arbitrary. Higher to trade memory for performance.
#define PCMPRINT_BUFFER_SIZE 512

#define PCMPRINT_CMD_VOICE     0x01
#define PCMPRINT_CMD_SHAPE     0x02 /* u8:SHAPE ; (0,1,2,3,4)=(sine,square,sawup,sawdown,triangle) */
#define PCMPRINT_CMD_NOISE     0x03
#define PCMPRINT_CMD_HARMONICS 0x04 /* u8:COUNT u8:COEF... */
#define PCMPRINT_CMD_FM        0x05 /* u8.8:RATE u8.8:RANGE */
#define PCMPRINT_CMD_RATE      0x06 /* u16:HZ u8:COUNT (u16:MS u16:HZ)... */
#define PCMPRINT_CMD_RATELFO   0x07 /* u8.8:HZ u16:CENTS */
#define PCMPRINT_CMD_FMRANGE   0x08 /* u0.16:V u8:COUNT (u16:MS u0.16:V)... */
#define PCMPRINT_CMD_FMLFO     0x09 /* u8.8:HZ u8.8:MLT ; FMLFO */
#define PCMPRINT_CMD_LEVEL     0x0a /* u0.16:V u8:COUNT (u16:MS u0.16:V)... */
#define PCMPRINT_CMD_GAIN      0x0b /* u8.8:MLT */
#define PCMPRINT_CMD_CLIP      0x0c /* u0.8 LEVEL */
#define PCMPRINT_CMD_DELAY     0x0d /* u16:MS u0.8:DRY u0.8:WET u0.8:STORE u0.8:FEEDBACK */
#define PCMPRINT_CMD_BANDPASS  0x0e /* u16:MID u16:WIDTH */
#define PCMPRINT_CMD_NOTCH     0x0f /* u16:MID u16:WIDTH */
#define PCMPRINT_CMD_LOPASS    0x10 /* u16:HZ */
#define PCMPRINT_CMD_HIPASS    0x11 /* u16:HZ */

#define PCMPRINT_SHAPE_SINE 0
#define PCMPRINT_SHAPE_SQUARE 1
#define PCMPRINT_SHAPE_SAWUP 2
#define PCMPRINT_SHAPE_SAWDOWN 3
#define PCMPRINT_SHAPE_TRIANGLE 4

struct pcmprint_env {
  float v;
  float dv;
  int ttl;
  int pointp;
  int inuse; // nonzero if decoded, otherwise assume uninitialized
  float value0;
  struct pcmprint_env_point {
    int delay;
    float value;
  } *pointv;
  int pointc;
};

// Values are in 0..1 after decode.
void pcmprint_env_cleanup(struct pcmprint_env *env);
int pcmprint_env_decode(struct pcmprint *pcmprint,struct pcmprint_env *env,const uint8_t *src,int srcc);
int pcmprint_env_init_constant(struct pcmprint *pcmprint,struct pcmprint_env *env,float v);
int pcmprint_env_has_zeroes(struct pcmprint_env *env); // => true if first and last value are zero
void pcmprint_env_scale(struct pcmprint_env *env,float scale);
void pcmprint_env_advance(struct pcmprint_env *env);

static inline float pcmprint_env_update(struct pcmprint_env *env) {
  if (env->ttl--<1) pcmprint_env_advance(env);
  env->v+=env->dv;
  return env->v;
}

struct pcmprint_lfo {
  uint32_t p;
  uint32_t dp;
  const float *wave;
  float mlt;
  float add;
};

// (mlt) is in 0..65535 after decode; scale to taste.
int pcmprint_lfo_decode(struct pcmprint *pcmprint,struct pcmprint_lfo *lfo,const uint8_t *src,int srcc);

static inline float pcmprint_lfo_update(struct pcmprint_lfo *lfo) {
  float sample=lfo->wave[lfo->p>>PCMPRINT_WAVE_SHIFT];
  lfo->p+=lfo->dp;
  return sample*lfo->mlt+lfo->add;
}

struct pcmprint_cmd {
  uint8_t opcode;
  int iv[8];
  float fv[10];
  struct pcmprint_env env;
  float *buf;
};

struct pcmprint_voice {
  float *wave; // Null for noise or FM.
  int ownwave;
  float fmrate,fmrange; // 0 for wave or noise.
  struct pcmprint_env fmrangeenv; // if in use, (fmrange) is baked in
  struct pcmprint_lfo fmrangelfo;
  struct pcmprint_env rate;
  struct pcmprint_lfo ratelfo;
  struct pcmprint_cmd *cmdv;
  int cmdc,cmda;
  
  float carp,modp;
};

struct pcmprint_cmd *pcmprint_voice_add_cmd(struct pcmprint_voice *voice);

struct pcmprint {
  struct pcmprint_pcm *pcm;
  int pcmp;
  int rate;
  struct pcmprint_voice *voicev;
  int voicec,voicea;
  float *sine;
  float buf[PCMPRINT_BUFFER_SIZE];
};

int pcmprint_decode(struct pcmprint *pcmprint,const uint8_t *src,int srcc);

struct pcmprint_voice *pcmprint_voice_new(struct pcmprint *pcmprint);
int pcmprint_require_sine(struct pcmprint *pcmprint);

#endif
