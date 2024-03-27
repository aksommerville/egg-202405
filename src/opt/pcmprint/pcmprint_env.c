#include "pcmprint_internal.h"

/* Cleanup.
 */
 
void pcmprint_env_cleanup(struct pcmprint_env *env) {
  if (env->pointv) free(env->pointv);
}

/* Reset.
 */
 
static void pcmprint_env_reset(struct pcmprint_env *env) {
  env->pointp=0;
  env->v=env->value0;
  if (env->pointc) {
    env->ttl=env->pointv->delay;
    env->dv=(env->pointv->value-env->v)/(float)env->ttl;
  } else {
    env->ttl=INT_MAX;
    env->dv=0.0f;
  }
}

/* Init for constant value.
 */
 
int pcmprint_env_init_constant(struct pcmprint *pcmprint,struct pcmprint_env *env,float v) {
  if (env->inuse) return -1;
  env->inuse=1;
  env->value0=v;
  env->pointc=0;
  pcmprint_env_reset(env);
  return 0;
}

/* Decode.
 */
 
int pcmprint_env_decode(struct pcmprint *pcmprint,struct pcmprint_env *env,const uint8_t *src,int srcc) {
  if (srcc<3) return -1;
  if (env->inuse) return -1;
  if (env->pointv) return -1;
  env->inuse=1;
  
  // Starts with initial value, then point count.
  // Each point is four bytes encoded.
  env->value0=((src[0]<<8)|src[1])/65535.0f;
  int pointc=src[2];
  int srcp=3;
  if (pointc) {
    if (srcp>srcc-(pointc*4)) return -1;
    if (!(env->pointv=malloc(sizeof(struct pcmprint_env_point)*pointc))) return -1;
    int i=pointc;
    struct pcmprint_env_point *dst=env->pointv;
    for (;i-->0;dst++,srcp+=4) {
      dst->delay=(src[srcp]<<8)|src[srcp+1];
      if ((dst->delay=(dst->delay*pcmprint->rate)/1000)<1) dst->delay=1;
      dst->value=((src[srcp+2]<<8)|src[srcp+3])/65535.0f;
    }
    env->pointc=pointc;
  }
  
  pcmprint_env_reset(env);
  return srcp;
}

/* Assert that first and last value are zero.
 */
 
int pcmprint_env_has_zeroes(struct pcmprint_env *env) {
  if (fpclassify(env->value0)!=FP_ZERO) return 0;
  if (env->pointc>0) {
    if (fpclassify(env->pointv[env->pointc-1].value)!=FP_ZERO) return 0;
  }
  return 1;
}

/* Scale values.
 */
 
void pcmprint_env_scale(struct pcmprint_env *env,float scale) {
  env->value0*=scale;
  struct pcmprint_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) point->value*=scale;
  pcmprint_env_reset(env);
}

/* Advance to next leg.
 */
 
void pcmprint_env_advance(struct pcmprint_env *env) {
  env->pointp++;
  if (env->pointp>=env->pointc) {
    env->pointp=env->pointc;
    env->ttl=INT_MAX;
    env->dv=0.0f;
    if (env->pointc) env->v=env->pointv[env->pointc-1].value;
    else env->v=env->value0;
  } else {
    const struct pcmprint_env_point *point=env->pointv+env->pointp;
    env->v=point[-1].value;
    env->ttl=point->delay;
    env->dv=(point->value-env->v)/(float)env->ttl;
  }
}

/* Dropping LFO decode in here too, not that it has any business here.
 */
 
int pcmprint_lfo_decode(struct pcmprint *pcmprint,struct pcmprint_lfo *lfo,const uint8_t *src,int srcc) {
  if (lfo->wave) return -1;
  if (srcc<4) return -1;
  if (pcmprint_require_sine(pcmprint)<0) return -1;
  float hz=(float)((src[0]<<8)|src[1])/256.0f;
  float fnorm=hz/(float)pcmprint->rate;
  lfo->p=0;
  lfo->dp=(uint32_t)(fnorm*4294967296.0f);
  lfo->mlt=(float)((src[2]<<8)|src[3]);
  lfo->add=0.0f;
  lfo->wave=pcmprint->sine;
  return 4;
}
