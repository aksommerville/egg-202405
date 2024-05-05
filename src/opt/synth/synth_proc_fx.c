#include "synth_internal.h"

#define SYNTH_FX_VOICE_LIMIT 8

struct synth_fx_voice {
  float carp; // radians
  float modp; // radians
  float cardp0;
  float cardp;
  float moddp;
  struct synth_env level;
  struct synth_env range;
  int64_t birthday;
  uint8_t noteid;
};
    
struct synth_fx_context {
  struct synth_fx_voice voicev[SYNTH_FX_VOICE_LIMIT];
  int voicec;
  struct synth_env_config level;
  struct synth_env_config range;
  float wheel_range;
  float bend;
  float modrate;
  uint32_t lfop;
  uint32_t lfodp;
  float lforange;
  float drive;
  float clip;
  float trim;
  struct synth_delay delay;
  struct synth_delay detune;
  uint32_t detunep;
  uint32_t detunedp;
  int ttl;
  float buf[SYNTH_BUFFER_LIMIT];
  float lfobuf[SYNTH_BUFFER_LIMIT];
};

#define CTX ((struct synth_fx_context*)proc->userdata)
    
/* Cleanup.
 */
    
static void _fx_del(struct synth_proc *proc) {
  if (proc->userdata) {
    synth_delay_cleanup(&CTX->delay);
    synth_delay_cleanup(&CTX->detune);
    free(proc->userdata);
  }
}

/* Update voice.
 */
 
static inline void _fx_voice_update(float *v,int c,struct synth *synth,struct synth_proc *proc,struct synth_fx_voice *voice) {
  if (synth_env_is_finished(&voice->level)) return;
  const float *lfo=CTX->lfobuf;
  for (;c-->0;v++,lfo++) {
  
    (*v)+=sinf(voice->carp)*synth_env_update(&voice->level);
    
    float mod=sinf(voice->modp);
    mod*=synth_env_update(&voice->range)+*lfo;
    voice->modp+=voice->moddp;
    if (voice->modp>M_PI) voice->modp-=M_PI*2.0f;
    voice->carp+=voice->cardp+voice->cardp*mod;
    if (voice->carp>M_PI) voice->carp-=M_PI*2.0f;
  }
}

/* Update.
 */

static void _fx_update(float *v,int c,struct synth *synth,struct synth_proc *proc) {
  memset(CTX->buf,0,sizeof(float)*c);
  
  // Calculate FM range LFO.
  if (CTX->lforange>0.0f) {
    float *dst=CTX->lfobuf;
    int i=c;
    for (;i-->0;dst++) {
      *dst=synth->sine[CTX->lfop>>SYNTH_WAVE_SHIFT]*CTX->lforange;
      CTX->lfop+=CTX->lfodp;
    }
  }
  
  // Voices.
  struct synth_fx_voice *voice=CTX->voicev;
  int i=CTX->voicec;
  for (;i-->0;voice++) {
    _fx_voice_update(CTX->buf,c,synth,proc,voice);
  }
  while (CTX->voicec&&synth_env_is_finished(&CTX->voicev[CTX->voicec-1].level)) CTX->voicec--;
  
  // Overdrive.
  if (CTX->drive>0.0f) {
    float clip=CTX->clip;
    float nclip=-clip;
    float *p=CTX->buf;
    for (i=c;i-->0;p++) {
      (*p)*=CTX->drive;
      if (*p>clip) *p=clip;
      else if (*p<nclip) *p=nclip;
    }
  }
  
  // Detune. I thought this would go before overdrive, but actually sounds a lot nicer after.
  if (CTX->detune.v) {
    float *p=CTX->buf;
    for (i=c;i-->0;p++) {
      float phase=synth->sine[CTX->detunep>>SYNTH_WAVE_SHIFT];
      CTX->detunep+=CTX->detunedp;
      (*p)=
        synth_detune_update(&CTX->detune,*p,phase)*0.25f+
        (*p)*0.75f;
    }
  }
  
  // Delay.
  if (CTX->delay.v) {
    float *p=CTX->buf;
    for (i=c;i-->0;p++) (*p)=synth_delay_update(&CTX->delay,*p);
  }
  
  // Trim.
  if (CTX->trim<1.0f) {
    float *p=CTX->buf;
    for (i=c;i-->0;p++) (*p)*=CTX->trim;
  }
  
  // TTL.
  if (CTX->ttl>0) {
    float *p=CTX->buf;
    for (i=c;i-->0;p++) {
      if (CTX->ttl<=0) {
        *p=0;
        proc->update=0;
      } else if (CTX->ttl<10000) {
        (*p)*=(float)CTX->ttl/10000.0f;
      }
      CTX->ttl--;
    }
  }
  
  const float *src=CTX->buf;
  for (i=c;i-->0;v++,src++) (*v)+=(*src);
}

/* Release all notes.
 */

static void _fx_release(struct synth *synth,struct synth_proc *proc) {
  struct synth_fx_voice *voice=CTX->voicev;
  int i=CTX->voicec;
  for (;i-->0;voice++) {
    synth_env_release(&voice->level);
    synth_env_release(&voice->range);
  }
  CTX->ttl=synth->rate;
}

/* Control change.
 */

static void _fx_control(struct synth *synth,struct synth_proc *proc,uint8_t k,uint8_t v) {
  switch (k) {
    case MIDI_CONTROL_VOLUME_MSB: {
        CTX->trim=v/127.0f;
      } break;
  }
}

/* Pitch wheel.
 */

static void _fx_wheel(struct synth *synth,struct synth_proc *proc,uint16_t v) {
  CTX->bend=powf(2.0f,(CTX->wheel_range*((v-0x2000)/8192.0f))/1200.0f);
  struct synth_fx_voice *voice=CTX->voicev;
  int i=CTX->voicec;
  for (;i-->0;voice++) {
    voice->cardp=voice->cardp0*CTX->bend;
    voice->moddp=voice->cardp*CTX->modrate;
  }
}

/* Note Once.
 */

static void _fx_note_once(struct synth *synth,struct synth_proc *proc,uint8_t noteid,uint8_t velocity,int dur) {
  
  // Find a voice, evicting one if needed.
  struct synth_fx_voice *voice=CTX->voicev;
  if (CTX->voicec<SYNTH_FX_VOICE_LIMIT) {
    voice=CTX->voicev+CTX->voicec++;
  } else {
    struct synth_fx_voice *q=CTX->voicev;
    int i=CTX->voicec;
    for (;i-->0;q++) {
      if (synth_env_is_finished(&q->level)) {
        voice=q;
        break;
      }
      if (q->birthday<voice->birthday) {
        voice=q;
      }
    }
  }
  
  voice->carp=0.0f;
  voice->modp=0.0f;
  voice->cardp0=synth->ffreqv[noteid&0x7f]*M_PI*2.0f;
  voice->cardp=voice->cardp0*CTX->bend;
  voice->moddp=voice->cardp*CTX->modrate;
  synth_env_init(&voice->level,&CTX->level,velocity,dur);
  synth_env_init(&voice->range,&CTX->range,velocity,dur);
  voice->birthday=synth->framec;
  voice->noteid=noteid;
}

/* Note On.
 */

static void _fx_note_on(struct synth *synth,struct synth_proc *proc,uint8_t noteid,uint8_t velocity) {
  _fx_note_once(synth,proc,noteid,velocity,INT_MAX);
}

/* Note Off.
 */

static void _fx_note_off(struct synth *synth,struct synth_proc *proc,uint8_t noteid,uint8_t velocity) {
  struct synth_fx_voice *voice=CTX->voicev;
  int i=CTX->voicec;
  for (;i-->0;voice++) {
    if (voice->noteid!=noteid) continue;
    synth_env_release(&voice->level);
    synth_env_release(&voice->range);
    voice->noteid=0xff;
    // keep going, because we don't check for unique noteid at Note On.
  }
}

/* Init.
 */
 
int synth_proc_fx_init(struct synth *synth,struct synth_proc *proc,struct synth_channel *channel,const struct synth_builtin *builtin) {
  if (!(proc->userdata=calloc(1,sizeof(struct synth_fx_context)))) return -1;
  proc->del=_fx_del;
  proc->update=_fx_update;
  proc->release=_fx_release;
  proc->control=_fx_control;
  proc->wheel=_fx_wheel;
  proc->note_on=_fx_note_on;
  proc->note_off=_fx_note_off;
  proc->note_once=_fx_note_once;
  
  CTX->wheel_range=200.0f;
  CTX->bend=1.0f;
  CTX->modrate=builtin->fx.rate/16.0f;
  CTX->lfop=0;
  int period=(builtin->fx.rangelfo*synth_frames_per_beat(synth))>>4;
  if ((period>0)&&builtin->fx.rangelfo_depth) {
    CTX->lfodp=0xffffffff/period;
    CTX->lforange=builtin->fx.rangelfo_depth/16.0f;
  } else {
    CTX->lfodp=0;
    CTX->lforange=0.0f;
  }
  if (builtin->fx.overdrive>0) {
    CTX->drive=1.0f+builtin->fx.overdrive/4.0f;
    CTX->clip=1.0f-builtin->fx.overdrive/500.0f;
    CTX->clip*=channel->master;
  } else {
    CTX->drive=0.0f;
    CTX->clip=0.0f;
  }
  CTX->trim=channel->trim;
  synth_env_config_init_tiny(synth,&CTX->level,builtin->fx.level);
  synth_env_config_gain(&CTX->level,channel->master);
  synth_env_config_init_parameter(&CTX->range,&CTX->level,builtin->fx.rangeenv);
  synth_env_config_gain(&CTX->range,builtin->fx.scale/16.0f);
  
  if (builtin->fx.delay_rate&&builtin->fx.delay_depth) {
    int framec=(builtin->fx.delay_rate*synth_frames_per_beat(synth))>>4;
    float mix=0.125f+((builtin->fx.delay_depth/255.0f)*0.5f);
    float feedback=0.250f+((builtin->fx.delay_depth/255.0f)*0.5f);
    if (synth_delay_init(&CTX->delay,framec,mix,feedback)<0) return -1;
  }
  
  if (builtin->fx.detune_rate&&builtin->fx.detune_depth) {
    int period=(builtin->fx.detune_rate*synth_frames_per_beat(synth))>>4;
    if (period<1) period=1;
    CTX->detunep=0xc0000000; // start at lowest point: 3/4
    CTX->detunedp=0xffffffff/period;
    int framec=1+(builtin->fx.detune_depth*period)/5000;
    if (synth_detune_init(&CTX->detune,framec)<0) return -1;
  }
  
  return 0;
}
