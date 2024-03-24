#include "synth_internal.h"

/* subtractor: IIR bandpass against white noise.
 */
 
struct synth_subtractor {
  float noise[SYNTH_UPDATE_LIMIT];
  float trim;
  struct synth_env_config level;
  struct synth_subtractor_voice {
    uint8_t noteid;
    double coefv[10]; // 3 dry + 2 wet, and then repeat it
    double buf[10];
    struct synth_env level;
  } *voicev;
  int voicec,voicea;
};
 
static void synth_subtractor_del(struct synth *synth,struct synth_proc *proc) {
  struct synth_subtractor *ctx=proc->userdata;
  proc->userdata=0;
  if (!ctx) return;
  if (ctx->voicev) free(ctx->voicev);
  free(ctx);
}

static void *synth_subtractor_new(struct synth *synth,const struct synth_channel *channel) {
  struct synth_subtractor *ctx=calloc(1,sizeof(struct synth_subtractor));
  if (!ctx) return 0;
  ctx->trim=channel->trim;
  synth_env_config_init_level(synth,&ctx->level,0x10,0x40,0x80,0x20,0xf0);
  synth_env_config_gain(&ctx->level,150.0f);
  return ctx;
}

static void synth_subtractor_voice_update(float *v,int c,struct synth_subtractor_voice *voice,const float *noise) {
  for (;c-->0;v++,noise++) {
  
    // First pass.
    voice->buf[2]=voice->buf[1];
    voice->buf[1]=voice->buf[0];
    voice->buf[0]=(rand()&0xffff)/32768.0-1.0;
    double sample=
      voice->coefv[0]*voice->buf[0]+
      voice->coefv[1]*voice->buf[1]+
      voice->coefv[2]*voice->buf[2]+
      voice->coefv[3]*voice->buf[3]+
      voice->coefv[4]*voice->buf[4];
    voice->buf[4]=voice->buf[3];
    voice->buf[3]=sample;
    
    // Second pass.
    voice->buf[7]=voice->buf[6];
    voice->buf[6]=voice->buf[5];
    voice->buf[5]=sample;
    sample=
      voice->coefv[5]*voice->buf[5]+
      voice->coefv[6]*voice->buf[6]+
      voice->coefv[7]*voice->buf[7]+
      voice->coefv[8]*voice->buf[8]+
      voice->coefv[9]*voice->buf[9];
    voice->buf[9]=voice->buf[8];
    voice->buf[8]=sample;
    
    if (sample>1.0) {
      fprintf(stderr,"clip %f\n",sample);
      sample=1.0;
    } else if (sample<-1.0) {
      fprintf(stderr,"clip %f\n",sample);
      sample=-1.0;
    }
    float limit=synth_env_next(&voice->level);
    limit*=(float)sample;
    if (limit>1.0f) {
      fprintf(stderr,"clip(2) %f\n",limit);
      limit=1.0f;
    } else if (limit<-1.0f) {
      fprintf(stderr,"clip(2) %f\n",limit);
      limit=-1.0f;
    }
    //if (sample>limit) sample=limit;
    //else if (sample<-limit) sample=-limit;
    (*v)+=limit;
    //(*v)+=sample*synth_env_next(&voice->level);
  }
}

static void synth_subtractor_update(float *v,int c,struct synth *synth,struct synth_proc *proc) {
  struct synth_subtractor *ctx=proc->userdata;
  if (!ctx) return;
  if (ctx->voicec<1) return;
  
  // Generate noise.
  float *p=ctx->noise;
  int i=c;
  for (;i-->0;p++) *p=(rand()&0xffff)/32768.0f-1.0f;
  
  // Filter thru each voice.
  struct synth_subtractor_voice *voice=ctx->voicev;
  for (i=ctx->voicec;i-->0;voice++) {
    synth_subtractor_voice_update(v,c,voice,ctx->noise);
  }
  
  // Drop defunct voices from the end.
  while (ctx->voicec&&synth_env_is_finished(&ctx->voicev[ctx->voicec-1].level)) ctx->voicec--;
}

static void synth_subtractor_note(struct synth *synth,struct synth_proc *proc,uint8_t noteid,uint8_t velocity,int dur) {
  struct synth_subtractor *ctx=proc->userdata;
  if (!ctx) return;
  if (noteid>=0x80) return;
  
  struct synth_subtractor_voice *voice=0;
  if (ctx->voicec<ctx->voicea) {
    voice=ctx->voicev+ctx->voicec++;
  } else {
    int i=ctx->voicec;
    struct synth_subtractor_voice *q=ctx->voicev;
    for (;i-->0;q++) {
      if (synth_env_is_finished(&q->level)) {
        voice=q;
        break;
      }
    }
    if (!voice) {
      int na=ctx->voicea+8;
      if (na>INT_MAX/sizeof(struct synth_subtractor_voice)) return;
      void *nv=realloc(ctx->voicev,sizeof(struct synth_subtractor_voice)*na);
      if (!nv) return;
      ctx->voicev=nv;
      ctx->voicea=na;
      voice=ctx->voicev+ctx->voicec++;
    }
  }
  
  memset(voice,0,sizeof(struct synth_subtractor_voice));
  voice->noteid=noteid;
  synth_env_init(&voice->level,&ctx->level,velocity);
  synth_env_gain(&voice->level,ctx->trim);
  if ((voice->level.sust=dur)<1) voice->level.sust=1;
  
  /* 3-point IIR bandpass.
   * I have only a vague idea of how this works, and the formula is taken entirely on faith.
   * Reference:
   *   Steven W Smith: The Scientist and Engineer's Guide to Digital Signal Processing
   *   Ch 19, p 326, Equation 19-7
   */
  double w1=50.0,w2=40.0;//TODO configurable
  double midnorm=synth->freqn_by_noteid[noteid];
  double wnorm=w1/(double)synth->rate;
  double r=1.0-3.0*wnorm;
  double cosfreq=cos(M_PI*2.0*midnorm);
  double k=(1.0-2.0*r*cosfreq+r*r)/(2.0-2.0*cosfreq);
  
  voice->coefv[0]=1.0-k;
  voice->coefv[1]=2.0*(k-r)*cosfreq;
  voice->coefv[2]=r*r-k;
  voice->coefv[3]=2.0*r*cosfreq;
  voice->coefv[4]=-r*r;
  
  // And then once more. This second pass really cleans it up.
  wnorm=w2/(double)synth->rate;
  r=1.0-3.0*wnorm;
  // cosfreq, no change
  k=(1.0-2.0*r*cosfreq+r*r)/(2.0-2.0*cosfreq);
  
  voice->coefv[5]=1.0-k;
  voice->coefv[6]=2.0*(k-r)*cosfreq;
  voice->coefv[7]=r*r-k;
  voice->coefv[8]=2.0*r*cosfreq;
  voice->coefv[9]=-r*r;
}

/* Generate wave from harmonics.
 */
 
static void synth_wave_add_harmonic(float *dst,const float *ref,int c,int step,float level) {
  int refp=0;
  int i=c;
  for (;i-->0;dst++) {
    (*dst)+=ref[refp]*level;
    if ((refp+=step)>=c) refp-=c;
  }
}
 
static void synth_wave_harmonics(float *dst,const float *ref,int c,const float *coefv,int coefc) {
  if (coefc>32) coefc=32;
  memset(dst,0,sizeof(float)*c);
  int i=0; for (;i<coefc;i++) {
    if (coefv[i]>0.0f) {
      synth_wave_add_harmonic(dst,ref,c,i+1,coefv[i]);
    }
  }
}

/* Apply channel header, starting song.
 */
 
void synth_apply_channel_header(struct synth *synth,int chid,const uint8_t *src) {
  //fprintf(stderr,"%s chid=%d src=(%02x %02x %02x %02x)\n",__func__,chid,src[0],src[1],src[2],src[3]);
  if ((chid<0)||(chid>=SYNTH_CHANNEL_COUNT)) return;
  struct synth_channel *channel=synth->channelv+chid;
  float *wave=synth->waves+chid*SYNTH_WAVE_SIZE_SAMPLES;
  
  memset(channel,0,sizeof(struct synth_channel));
  channel->chid=chid;
  channel->pid=src[0];
  channel->trim=src[1]/255.0f;
  channel->pan=src[2]/128.0f-1.0f;
  channel->wheel_range=200.0f;
  channel->wheel=0x40;
  channel->bend=1.0f;
  
  switch (channel->pid) {
    // feline-mind and what-fits-in-the-box, 0 is lead and 1 is bass.
    case 0x00: {
        struct synth_proc *proc=synth_proc_new(synth);
        if (proc->userdata=synth_subtractor_new(synth,channel)) {
          proc->chid=chid;
          proc->noteid=0xff;
          proc->origin=SYNTH_ORIGIN_SONG;
          proc->del=synth_subtractor_del;
          proc->update=synth_subtractor_update;
          proc->note=synth_subtractor_note;
          proc->wheel=0;
        } // leaving update unset will cause the proc to get deleted soon
      } break;
    case 0x01: {
        synth_env_config_init_level(synth,&channel->level,0x20,0x60,0x50,0x50,0x80);
        memcpy(wave,synth->sine,sizeof(float)*SYNTH_WAVE_SIZE_SAMPLES);
        channel->modrel=0.5f;
        synth_env_config_init_param(&channel->modrange,&channel->level,0.0f,1.0f,0.125f,0.125f,0.0f,0.75f);
        synth_env_config_gain(&channel->modrange,2.0f);
      } break;
    case 0xf0: { // kind of annoying wowow lead
        synth_env_config_init_level(synth,&channel->level,0x30,0x40,0x50,0x30,0xff);
        //const float coefv[]={0.6f,0.0f,0.4f,0.0f,0.2f,0.0,0.05f}; // simple squareish organ
        const float coefv[]={0.4f,0.5f,0.3f,0.20f,0.10f,0.10f,0.05f};
        synth_wave_harmonics(wave,synth->sine,SYNTH_WAVE_SIZE_SAMPLES,coefv,sizeof(coefv)/sizeof(coefv[0]));
        synth_env_config_init_param(&channel->rock,&channel->level,0.5f,1.0f,0.5f,0.5f,0.25f,0.75f);
      } break;
    case 0xf1: { // simple bass
        synth_env_config_init_level(synth,&channel->level,0x20,0x60,0x50,0x50,0x80);
        const float coefv[]={0.8f,0.2f,0.03f};
        synth_wave_harmonics(wave,synth->sine,SYNTH_WAVE_SIZE_SAMPLES,coefv,sizeof(coefv)/sizeof(coefv[0]));
        synth_env_config_init_param(&channel->rock,&channel->level,0.0f,1.0f,0.125f,0.125f,0.0f,0.75f);
      } break;
    default: {
        fprintf(stderr,"DEFAULT PROGRAM (0x%02x)\n",channel->pid);
        synth_env_config_init_level(synth,&channel->level,0x80,0x80,0x80,0x80,0x80);
        memcpy(wave,synth->sine,sizeof(float)*SYNTH_WAVE_SIZE_SAMPLES);
      }
  }
}
