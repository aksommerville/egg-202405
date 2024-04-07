#include "sfg_internal.h"

/* Decode.
 */
 
int sfg_env_decode(struct sfg_env *env,const uint8_t *src,int srcc,int rate,float scale) {
  if (srcc<3) return -1;
  env->v0=((src[0]<<8)|src[1])*scale;
  int ptc=src[2];
  int srcp=3;
  if (srcp>srcc-ptc*4) return -1;
  env->pointc=0;
  if (env->pointa<ptc) {
    void *nv=realloc(env->pointv,sizeof(struct sfg_env_point)*ptc);
    if (!nv) return -1;
    env->pointv=nv;
    env->pointa=ptc;
  }
  float ratescale=rate/1000.0f;
  int now=0;
  while (ptc-->0) {
    int ms=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    float v=((src[srcp]<<8)|src[srcp+1])*scale; srcp+=2;
    int framec=(int)(ms*ratescale);
    if (framec<1) framec=1;
    struct sfg_env_point *point=env->pointv+env->pointc++;
    point->v=v;
    point->t=now+framec;
  }
  env->v=env->v0;
  env->pointp=0;
  if (env->pointc>0) {
    env->ttl=env->pointv[0].t;
    env->dv=(env->pointv[0].v-env->v)/env->ttl;
  } else {
    env->ttl=INT_MAX;
    env->dv=0.0f;
  }
  return srcp;
}

/* Set constant.
 */
 
void sfg_env_constant(struct sfg_env *env,float v) {
  env->v=env->v0=v;
  env->dv=0.0f;
  env->ttl=INT_MAX;
  env->pointp=0;
  env->pointc=0;
}

/* Advance.
 */
 
void sfg_env_advance(struct sfg_env *env) {
  env->pointp++;
  if (env->pointp>=env->pointc) {
    env->pointp=env->pointc;
    env->ttl=INT_MAX;
    env->dv=0.0f;
    if (env->pointc) env->v=env->pointv[env->pointc-1].v;
    else env->v=env->v0;
  } else {
    env->ttl=env->pointv[env->pointp].t;
    env->v=env->pointv[env->pointp-1].v;
    env->dv=(env->pointv[env->pointp].v-env->v)/env->ttl;
  }
}
