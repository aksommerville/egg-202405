#include "pcmprint_internal.h"

/* Add signals.
 */
 
static inline void pcmprint_signal_add(float *dst,const float *src,int c) {
  for (;c-->0;dst++,src++) (*dst)+=(*src);
}

/* Oscillate.
 */
 
static void pcmprint_voice_oscillate_noise(float *v,int c,struct pcmprint *pcmprint,struct pcmprint_voice *voice) {
  for (;c-->0;v++) *v=(rand()&0xffff)/32768.0f-1.0f;
}
 
static void pcmprint_voice_oscillate_wave(float *v,int c,struct pcmprint *pcmprint,struct pcmprint_voice *voice) {
  for (;c-->0;v++) {
    *v=voice->wave[((uint32_t)(voice->carp*4294967296.0f))>>PCMPRINT_WAVE_SHIFT];
    voice->carp+=pcmprint_env_update(&voice->rate);
    if (voice->carp>=1.0f) voice->carp-=1.0f;
  }
}
 
static void pcmprint_voice_oscillate_wavelfo(float *v,int c,struct pcmprint *pcmprint,struct pcmprint_voice *voice) {
  for (;c-->0;v++) {
    *v=voice->wave[((uint32_t)(voice->carp*4294967296.0f))>>PCMPRINT_WAVE_SHIFT];
    float adj=pcmprint_lfo_update(&voice->ratelfo);
    adj=powf(2.0f,adj/1200.0f);
    voice->carp+=pcmprint_env_update(&voice->rate)*adj;
    if (voice->carp>=1.0f) voice->carp-=1.0f;
  }
}
 
static void pcmprint_voice_oscillate_fm(float *v,int c,struct pcmprint *pcmprint,struct pcmprint_voice *voice) {
  for (;c-->0;v++) {
    *v=pcmprint->sine[((uint32_t)(voice->carp*4294967296.0f))>>PCMPRINT_WAVE_SHIFT];
    float mod=pcmprint->sine[((uint32_t)(voice->modp*4294967296.0f))>>PCMPRINT_WAVE_SHIFT];
    mod*=pcmprint_env_update(&voice->fmrangeenv);
    float dp=pcmprint_env_update(&voice->rate);
    voice->modp+=dp*voice->fmrate;
    if (voice->modp>1.0f) voice->modp-=1.0f;
    dp+=dp*mod;
    voice->carp+=dp;
    if (voice->carp>1.0f) voice->carp-=1.0f;
  }
}
 
static void pcmprint_voice_oscillate_fmlfo(float *v,int c,struct pcmprint *pcmprint,struct pcmprint_voice *voice) {
  for (;c-->0;v++) {
    *v=pcmprint->sine[((uint32_t)(voice->carp*4294967296.0f))>>PCMPRINT_WAVE_SHIFT];
    float mod=pcmprint->sine[((uint32_t)(voice->modp*4294967296.0f))>>PCMPRINT_WAVE_SHIFT];
    mod*=pcmprint_env_update(&voice->fmrangeenv);
    mod*=pcmprint_lfo_update(&voice->fmrangelfo);
    float dp=pcmprint_env_update(&voice->rate);
    voice->modp+=dp*voice->fmrate;
    if (voice->modp>=1.0f) voice->modp-=1.0f;
    dp+=dp*mod;
    voice->carp+=dp;
    if (voice->carp>=1.0f) voice->carp-=1.0f;
  }
}
 
static void pcmprint_voice_oscillate_fm_ratelfo(float *v,int c,struct pcmprint *pcmprint,struct pcmprint_voice *voice) {
  for (;c-->0;v++) {
    *v=pcmprint->sine[((uint32_t)(voice->carp*4294967296.0f))>>PCMPRINT_WAVE_SHIFT];
    float mod=pcmprint->sine[((uint32_t)(voice->modp*4294967296.0f))>>PCMPRINT_WAVE_SHIFT];
    mod*=pcmprint_env_update(&voice->fmrangeenv);
    float adj=pcmprint_lfo_update(&voice->ratelfo);
    adj=powf(2.0f,adj/1200.0f);
    float dp=pcmprint_env_update(&voice->rate)*adj;
    voice->modp+=dp*voice->fmrate;
    if (voice->modp>=1.0f) voice->modp-=1.0f;
    dp+=dp*mod;
    voice->carp+=dp;
    if (voice->carp>=1.0f) voice->carp-=1.0f;
  }
}
 
static void pcmprint_voice_oscillate_fmlfo_ratelfo(float *v,int c,struct pcmprint *pcmprint,struct pcmprint_voice *voice) {
  for (;c-->0;v++) {
    *v=pcmprint->sine[((uint32_t)(voice->carp*4294967296.0f))>>PCMPRINT_WAVE_SHIFT];
    float mod=pcmprint->sine[((uint32_t)(voice->modp*4294967296.0f))>>PCMPRINT_WAVE_SHIFT];
    mod*=pcmprint_env_update(&voice->fmrangeenv);
    mod+=pcmprint_lfo_update(&voice->fmrangelfo);
    float adj=pcmprint_lfo_update(&voice->ratelfo);
    adj=powf(2.0f,adj/1200.0f);
    float dp=pcmprint_env_update(&voice->rate)*adj;
    voice->modp+=dp*voice->fmrate;
    if (voice->modp>=1.0f) voice->modp-=1.0f;
    dp+=dp*mod;
    voice->carp+=dp;
    if (voice->carp>=1.0f) voice->carp-=1.0f;
  }
}

/* Post-process commands.
 */
 
static void pcmprint_update_LEVEL(float *v,int c,struct pcmprint_cmd *cmd) {
  for (;c-->0;v++) (*v)*=pcmprint_env_update(&cmd->env);
}

static void pcmprint_update_GAIN(float *v,int c,struct pcmprint_cmd *cmd) {
  for (;c-->0;v++) (*v)*=cmd->fv[0];
}

static void pcmprint_update_CLIP(float *v,int c,struct pcmprint_cmd *cmd) {
  float hi=cmd->fv[0];
  float lo=-hi;
  for (;c-->0;v++) {
    if (*v>hi) *v=hi;
    else if (*v<lo) *v=lo;
  }
}

static void pcmprint_update_DELAY(float *v,int c,struct pcmprint_cmd *cmd) {
  for (;c-->0;v++) {
    float prv=cmd->buf[cmd->iv[1]];
    cmd->buf[cmd->iv[1]]=(*v)*cmd->fv[2]+prv*cmd->fv[3];
    if (++(cmd->iv[1])>=cmd->iv[0]) cmd->iv[1]=0;
    *v=(*v)*cmd->fv[0]+prv*cmd->fv[1];
  }
}

static void pcmprint_update_filter(float *v,int c,struct pcmprint_cmd *cmd) {
  const float *coefv=cmd->fv;
  float *statev=cmd->fv+5;
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

/* Update one voice.
 * Overwrite the provided buffer, with zeroes if necessary.
 */
 
static void pcmprint_voice_update(float *v,int c,struct pcmprint *pcmprint,struct pcmprint_voice *voice) {
  
  // Run the oscillator.
  if (voice->wave) {
    if (voice->ratelfo.wave) {
      pcmprint_voice_oscillate_wavelfo(v,c,pcmprint,voice);
    } else {
      pcmprint_voice_oscillate_wave(v,c,pcmprint,voice);
    }
  } else if (voice->fmrangeenv.inuse) {
    if (voice->ratelfo.wave) {
      if (voice->fmrangelfo.wave) {
        pcmprint_voice_oscillate_fmlfo_ratelfo(v,c,pcmprint,voice);
      } else {
        pcmprint_voice_oscillate_fm_ratelfo(v,c,pcmprint,voice);
      }
    } else {
      if (voice->fmrangelfo.wave) {
        pcmprint_voice_oscillate_fmlfo(v,c,pcmprint,voice);
      } else {
        pcmprint_voice_oscillate_fm(v,c,pcmprint,voice);
      }
    }
  } else {
    pcmprint_voice_oscillate_noise(v,c,pcmprint,voice);
  }
  
  // Run post effects.
  struct pcmprint_cmd *cmd=voice->cmdv;
  int i=voice->cmdc;
  for (;i-->0;cmd++) {
    switch (cmd->opcode) {
      case PCMPRINT_CMD_LEVEL: pcmprint_update_LEVEL(v,c,cmd); break;
      case PCMPRINT_CMD_GAIN: pcmprint_update_GAIN(v,c,cmd); break;
      case PCMPRINT_CMD_CLIP: pcmprint_update_CLIP(v,c,cmd); break;
      case PCMPRINT_CMD_DELAY: pcmprint_update_DELAY(v,c,cmd); break;
      case PCMPRINT_CMD_BANDPASS:
      case PCMPRINT_CMD_NOTCH:
      case PCMPRINT_CMD_LOPASS:
      case PCMPRINT_CMD_HIPASS: pcmprint_update_filter(v,c,cmd); break;
    }
  }
}

/* Update.
 */

int pcmprint_update(struct pcmprint *pcmprint,int c) {
  while (c>0) {
    int updc=pcmprint->pcm->c-pcmprint->pcmp;
    if (c<updc) updc=c;
    if (updc>PCMPRINT_BUFFER_SIZE) updc=PCMPRINT_BUFFER_SIZE;
    if (updc<1) break;
    float *dst=pcmprint->pcm->v+pcmprint->pcmp;
    struct pcmprint_voice *voice=pcmprint->voicev;
    int vi=pcmprint->voicec;
    for (;vi-->0;voice++) {
      pcmprint_voice_update(pcmprint->buf,updc,pcmprint,voice);
      pcmprint_signal_add(dst,pcmprint->buf,updc);
    }
    pcmprint->pcmp+=updc;
  }
  if (pcmprint->pcmp<pcmprint->pcm->c) return 0;
  return 1;
}
