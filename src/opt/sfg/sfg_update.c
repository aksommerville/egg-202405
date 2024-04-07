#include "sfg_internal.h"

/* Signal arithmetic, good candidates for SIMD.
 */
 
static inline void sfg_signal_add(float *dst,const float *src,int c) {
  for (;c-->0;dst++,src++) (*dst)+=(*src);
}

static inline void sfg_signal_mlt_s(float *v,int c,float a) {
  for (;c-->0;v++) (*v)*=a;
}

/* Silence.
 */
 
void sfg_oscillate_silence(float *v,int c,struct sfg_voice *voice) {
  memset(v,0,sizeof(float)*c);
}

/* Noise.
 */
 
void sfg_oscillate_noise(float *v,int c,struct sfg_voice *voice) {
  for (;c-->0;v++) (*v)=((rand()&0xffff)-32768)/32768.0f;
}

/* Simplest tonal oscillator: No FM, rate envelope, or rate LFO.
 */
 
void sfg_oscillate_flat(float *v,int c,struct sfg_voice *voice) {
  for (;c-->0;v++) {
    *v=voice->wave[voice->carpi>>SFG_WAVE_SHIFT];
    voice->carpi+=voice->cardpi;
  }
}

/* Full FM and rate envelope, no rate LFO.
 */
 
void sfg_oscillate_lfno(float *v,int c,struct sfg_voice *voice) {
  for (;c-->0;v++) {
  
    // Acquire carrier rate.
    float crate=sfg_env_update(&voice->rate);
    
    // Acquire modulation.
    float mod=sinf(voice->modp);
    mod*=sfg_env_update(&voice->range);
    voice->modp+=crate*voice->fmrate;
    if (voice->modp>=M_PI) voice->modp-=M_PI*2.0f;
    
    // Acquire sample and advance carrier.
    int sp=voice->carp*SFG_WAVE_SIZE_SAMPLES;
    if (sp<0) sp=0; else if (sp>=SFG_WAVE_SIZE_SAMPLES) sp=0; //TODO is oob realistic? Strictly speaking, we should mod it into range.
    *v=voice->wave[sp];
    crate+=crate*mod;
    voice->carp+=crate;
    if (voice->carp>=1.0f) voice->carp-=1.0f;
  }
}

/* Full oscillator.
 */
 
void sfg_oscillate_full(float *v,int c,struct sfg_voice *voice) {
  for (;c-->0;v++) {
  
    // Acquire carrier rate.
    float crate=sfg_env_update(&voice->rate);
    crate*=powf(2.0f,sinf(voice->ratelfop)*voice->ratelforange);
    voice->ratelfop+=voice->ratelfodp;
    if (voice->ratelfop>=M_PI) voice->ratelfop-=M_PI*2.0f;
    
    // Acquire modulation.
    float mod=sinf(voice->modp);
    mod*=sfg_env_update(&voice->range);
    voice->modp+=crate*voice->fmrate;
    if (voice->modp>=M_PI) voice->modp-=M_PI*2.0f;
    
    // Acquire sample and advance carrier.
    int sp=voice->carp*SFG_WAVE_SIZE_SAMPLES;
    if (sp<0) sp=0; else if (sp>=SFG_WAVE_SIZE_SAMPLES) sp=0; //TODO is oob realistic? Strictly speaking, we should mod it into range.
    *v=voice->wave[sp];
    crate+=crate*mod;
    voice->carp+=crate;
    if (voice->carp>=1.0f) voice->carp-=1.0f;
  }
}

/* Level envelope.
 */
 
void sfg_op_level_update(float *v,int c,struct sfg_op *op) {
  for (;c-->0;v++) (*v)*=sfg_env_update(&op->env);
}

/* Gain.
 */
 
void sfg_op_gain_update(float *v,int c,struct sfg_op *op) {
  for (;c-->0;v++) (*v)*=op->fv[0];
}

/* Clip.
 */
 
void sfg_op_clip_update(float *v,int c,struct sfg_op *op) {
  float hi=op->fv[0];
  float lo=-hi;
  for (;c-->0;v++) {
    if (*v>hi) *v=hi;
    else if (*v<lo) *v=lo;
  }
}

/* Delay.
 */
 
void sfg_op_delay_update(float *v,int c,struct sfg_op *op) {
  for (;c-->0;v++) {
    float dry=*v;
    float wet=op->buf[op->iv[1]];
    *v=dry*op->fv[0]+wet*op->fv[1];
    op->buf[op->iv[1]]=dry*op->fv[2]+wet*op->fv[3];
    op->iv[1]++;
    if (op->iv[1]>=op->iv[0]) op->iv[1]=0;
  }
}

/* Filter.
 */
 
void sfg_op_filter_update(float *v,int c,struct sfg_op *op) {
  const float *coefv=op->fv;
  float *statev=op->fv+5;
  for (;c-->0;v++) {
    statev[2]=statev[1];
    statev[1]=statev[0];
    statev[0]=*v;
    *v=(
      statev[0]*coefv[0]+
      statev[1]*coefv[1]+
      statev[2]*coefv[2]+
      statev[3]*coefv[3]+
      statev[4]*coefv[4]
    );
    statev[4]=statev[3];
    statev[3]=*v;
  }
}

/* Update one voice, count no longer than SFG_BUFFER_SIZE.
 * Must overwrite (v).
 */
 
static void sfg_voice_update(float *v,int c,struct sfg_voice *voice) {
  voice->oscillate(v,c,voice);
  struct sfg_op *op=voice->opv;
  int i=voice->opc;
  for (;i-->0;op++) op->update(v,c,op);
}

/* Update, main entry point.
 */
 
int sfg_printer_update(struct sfg_printer *printer,int c) {
  if (!printer) return -1;
  if (printer->voicec<1) {
    printer->pcmp=printer->pcm->c;
    return 1;
  }
  while (c>0) {
    int updc=printer->pcm->c-printer->pcmp;
    if (updc>c) updc=c;
    if (updc>SFG_BUFFER_SIZE) updc=SFG_BUFFER_SIZE;
    if (updc<1) break;
    
    float *dst=printer->pcm->v+printer->pcmp;
    float tmp[SFG_BUFFER_SIZE];
    struct sfg_voice *voice=printer->voicev;
    int i=printer->voicec;
    for (;i-->0;voice++) {
      sfg_voice_update(tmp,updc,voice);
      sfg_signal_add(dst,tmp,updc);
    }
    sfg_signal_mlt_s(dst,updc,printer->master);
    
    printer->pcmp+=updc;
    c-=updc;
  }
  return (printer->pcmp<printer->pcm->c)?0:1;
}
