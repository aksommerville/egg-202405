#include "pcmprint_internal.h"

/* Measure encoded command.
 */
 
static int pcmprint_measure_command(const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  switch (src[0]) {
    #define ENV if (srcc<4) return -1; return 4+src[3]*4;
    case PCMPRINT_CMD_VOICE: return 1;
    case PCMPRINT_CMD_SHAPE: return 2;
    case PCMPRINT_CMD_NOISE: return 1;
    case PCMPRINT_CMD_HARMONICS: if (srcc<2) return -1; return 2+src[1];
    case PCMPRINT_CMD_FM: return 5;
    case PCMPRINT_CMD_RATE: ENV
    case PCMPRINT_CMD_RATELFO: return 5;
    case PCMPRINT_CMD_FMRANGE: ENV
    case PCMPRINT_CMD_FMLFO: return 5;
    case PCMPRINT_CMD_LEVEL: ENV
    case PCMPRINT_CMD_GAIN: return 3;
    case PCMPRINT_CMD_CLIP: return 2;
    case PCMPRINT_CMD_DELAY: return 7;
    case PCMPRINT_CMD_BANDPASS: return 5;
    case PCMPRINT_CMD_NOTCH: return 5;
    case PCMPRINT_CMD_LOPASS: return 3;
    case PCMPRINT_CMD_HIPASS: return 3;
    #undef ENV
  }
  return -1;
}

/* SHAPE
 */
 
static int pcmprint_decode_SHAPE(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (srcc<2) return -1;
  if (voice->wave) return -1;
  switch (src[1]) {
    case PCMPRINT_SHAPE_SINE: {
        if (pcmprint_require_sine(pcmprint)<0) return -1;
        voice->wave=pcmprint->sine;
        voice->ownwave=0;
      } break;
    case PCMPRINT_SHAPE_SQUARE: {
        if (!(voice->wave=malloc(sizeof(float)*PCMPRINT_WAVE_SIZE_SAMPLES))) return -1;
        voice->ownwave=1;
        int halflen=PCMPRINT_WAVE_SIZE_SAMPLES>>1;
        float *v=voice->wave;
        int i=0;
        for (;i<halflen;i++,v++) *v=1.0f;
        for (;i<PCMPRINT_WAVE_SIZE_SAMPLES;i++,v++) *v=-1.0f;
      } break;
    case PCMPRINT_SHAPE_SAWUP: {
        if (!(voice->wave=malloc(sizeof(float)*PCMPRINT_WAVE_SIZE_SAMPLES))) return -1;
        voice->ownwave=1;
        float p=-1.0f,dp=2.0f/PCMPRINT_WAVE_SIZE_SAMPLES;
        float *v=voice->wave;
        int i=PCMPRINT_WAVE_SIZE_SAMPLES;
        for (;i-->0;v++,p+=dp) *v=p;
      } break;
    case PCMPRINT_SHAPE_SAWDOWN: {
        if (!(voice->wave=malloc(sizeof(float)*PCMPRINT_WAVE_SIZE_SAMPLES))) return -1;
        voice->ownwave=1;
        float p=1.0f,dp=-2.0f/PCMPRINT_WAVE_SIZE_SAMPLES;
        float *v=voice->wave;
        int i=PCMPRINT_WAVE_SIZE_SAMPLES;
        for (;i-->0;v++,p+=dp) *v=p;
      } break;
    case PCMPRINT_SHAPE_TRIANGLE: {
        if (!(voice->wave=malloc(sizeof(float)*PCMPRINT_WAVE_SIZE_SAMPLES))) return -1;
        voice->ownwave=1;
        int halflen=PCMPRINT_WAVE_SIZE_SAMPLES>>1;
        float *front=voice->wave;
        float *back=voice->wave+PCMPRINT_WAVE_SIZE_SAMPLES-1;
        int i=halflen; // We depend on length being even, and it will be -- it's a power of two.
        float p=-1.0f,dp=2.0f/halflen;
        for (;i-->0;p+=dp,front++,back--) *front=*back=p;
      } break;
    default: return -1;
  }
  return 2;
}

/* NOISE
 */
 
static int pcmprint_decode_NOISE(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (voice->wave) return -1;
  // Leave (wave) null for noise.
  return 1;
}

/* HARMONICS
 */
 
static void pcmprint_add_harmonic(float *dst,const float *src,float level,int step) {
  int i=PCMPRINT_WAVE_SIZE_SAMPLES;
  int srcp=0;
  for (;i-->0;dst++) {
    (*dst)+=src[srcp]*level;
    if ((srcp+=step)>=PCMPRINT_WAVE_SIZE_SAMPLES) srcp-=PCMPRINT_WAVE_SIZE_SAMPLES;
  }
}
 
static int pcmprint_decode_HARMONICS(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (voice->wave) return -1;
  int srcp=1;
  if (srcp>=srcc) return -1;
  int coefc=src[srcp++];
  if (srcp>srcc-coefc) return -1;
  if (pcmprint_require_sine(pcmprint)<0) return -1;
  if (!(voice->wave=calloc(sizeof(float),PCMPRINT_WAVE_SIZE_SAMPLES))) return -1;
  int i=coefc,step=1;
  for (;i-->0;srcp++,step++) {
    if (!src[srcp]) continue;
    pcmprint_add_harmonic(voice->wave,pcmprint->sine,src[srcp]/255.0f,step);
  }
  return srcp;
}

/* FM
 */
 
static int pcmprint_decode_FM(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (srcc<5) return -1;
  voice->fmrate=((src[1]<<8)|src[2])/256.0f;
  voice->fmrange=((src[3]<<8)|src[4])/256.0f;
  if (voice->fmrate<=0.0f) return -1;
  if (voice->fmrange<=0.0f) return -1;
  if (pcmprint_require_sine(pcmprint)<0) return -1;
  return 5;
}

/* Simple commands that add to the command list.
 */
 
static int pcmprint_decode_LEVEL(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  struct pcmprint_cmd *cmd=pcmprint_voice_add_cmd(voice);
  if (!cmd) return -1;
  cmd->opcode=PCMPRINT_CMD_LEVEL;
  int srcp=pcmprint_env_decode(pcmprint,&cmd->env,src,srcc);
  if (srcp<1) return -1;
  if (!pcmprint_env_has_zeroes(&cmd->env)) return -1;
  return srcp;
}
 
static int pcmprint_decode_GAIN(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (srcc<2) return -1;
  struct pcmprint_cmd *cmd=pcmprint_voice_add_cmd(voice);
  if (!cmd) return -1;
  cmd->opcode=PCMPRINT_CMD_GAIN;
  cmd->fv[0]=((src[0]<<8)|src[1])/256.0f;
  return 2;
}
 
static int pcmprint_decode_CLIP(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  struct pcmprint_cmd *cmd=pcmprint_voice_add_cmd(voice);
  if (!cmd) return -1;
  cmd->opcode=PCMPRINT_CMD_CLIP;
  cmd->fv[0]=src[0]/255.0f;
  return 1;
}
 
static int pcmprint_decode_DELAY(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (srcc<6) return -1;
  struct pcmprint_cmd *cmd=pcmprint_voice_add_cmd(voice);
  if (!cmd) return -1;
  cmd->opcode=PCMPRINT_CMD_DELAY;
  cmd->iv[0]=(src[0]<<8)|src[1];
  cmd->iv[0]=(cmd->iv[0]*pcmprint->rate)/1000;
  if (cmd->iv[0]<1) cmd->iv[0]=1;
  if (!(cmd->buf=calloc(sizeof(float),cmd->iv[0]))) return -1;
  cmd->fv[0]=src[2]/255.0f;
  cmd->fv[1]=src[3]/255.0f;
  cmd->fv[2]=src[4]/255.0f;
  cmd->fv[3]=src[5]/255.0f;
  return 6;
}
 
static int pcmprint_decode_BANDPASS(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (srcc<4) return -1;
  struct pcmprint_cmd *cmd=pcmprint_voice_add_cmd(voice);
  if (!cmd) return -1;
  cmd->opcode=PCMPRINT_CMD_BANDPASS;
  float freq=(float)((src[0]<<8)|src[1])/(float)pcmprint->rate;
  float width=(float)((src[2]<<8)|src[3])/(float)pcmprint->rate;
  float r=1.0f-3.0f*width;
  float cosfreq=cosf(M_PI*2.0f*freq);
  float k=(1.0f-2.0f*r*cosfreq+r*r)/(2.0f-2.0f*cosfreq);
  cmd->fv[0]=1.0f-k;
  cmd->fv[1]=2.0f*(k-r)*cosfreq;
  cmd->fv[2]=r*r-k;
  cmd->fv[3]=2.0f*r*cosfreq;
  cmd->fv[4]=-r*r;
  return 4;
}
 
static int pcmprint_decode_NOTCH(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (srcc<4) return -1;
  struct pcmprint_cmd *cmd=pcmprint_voice_add_cmd(voice);
  if (!cmd) return -1;
  cmd->opcode=PCMPRINT_CMD_NOTCH;
  float freq=(float)((src[0]<<8)|src[1])/(float)pcmprint->rate;
  float width=(float)((src[2]<<8)|src[3])/(float)pcmprint->rate;
  float r=1.0f-3.0f*width;
  float cosfreq=cosf(M_PI*2.0f*freq);
  float k=(1.0f-2.0f*r*cosfreq+r*r)/(2.0f-2.0f*cosfreq);
  cmd->fv[0]=k;
  cmd->fv[1]=-2.0f*k*cosfreq;
  cmd->fv[2]=k;
  cmd->fv[3]=2.0f*r*cosfreq;
  cmd->fv[4]=-r*r;
  return 4;
}
 
static int pcmprint_decode_LOPASS(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (srcc<2) return -1;
  struct pcmprint_cmd *cmd=pcmprint_voice_add_cmd(voice);
  if (!cmd) return -1;
  float freq=(float)((src[0]<<8)|src[1])/(float)pcmprint->rate;
  float rp=-cosf(M_PI/2.0f);
  float ip=sinf(M_PI/2.0f);
  float t=2.0f*tanf(0.5f);
  float w=2.0f*M_PI*freq;
  float m=rp*rp+ip*ip;
  float d=4.0f-4.0f*rp*t+m*t*t;
  float x0=(t*t)/d;
  float x1=(2.0f*t*t)/d;
  float x2=(t*t)/d;
  float y1=(8.0f-2.0f*m*t*t)/d;
  float y2=(-4.0f-4.0f*rp*t-m*t*t)/d;
  float k=sinf(0.5f-w/2.0f)/sinf(0.5f+w/2.0f);
  cmd->fv[0]=(x0-x1*k+x2*k*k)/d;
  cmd->fv[1]=(-2.0f*x0*k+x1+x1*k*k-2.0f*x2*k)/d;
  cmd->fv[2]=(x0*k*k-x1*k+x2)/d;
  cmd->fv[3]=(2.0f*k+y1+y1*k*k-2.0f*y2*k)/d;
  cmd->fv[4]=(-k*k-y1*k+y2)/d;
  return 2;
}
 
static int pcmprint_decode_HIPASS(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (srcc<2) return -1;
  struct pcmprint_cmd *cmd=pcmprint_voice_add_cmd(voice);
  if (!cmd) return -1;
  float freq=(float)((src[0]<<8)|src[1])/(float)pcmprint->rate;
  float rp=-cosf(M_PI/2.0f);
  float ip=sinf(M_PI/2.0f);
  float t=2.0f*tanf(0.5f);
  float w=2.0f*M_PI*freq;
  float m=rp*rp+ip*ip;
  float d=4.0f-4.0f*rp*t+m*t*t;
  float x0=(t*t)/d;
  float x1=(2.0f*t*t)/d;
  float x2=(t*t)/d;
  float y1=(8.0f-2.0f*m*t*t)/d;
  float y2=(-4.0f-4.0f*rp*t-m*t*t)/d;
  float k=-cosf(w/2.0f+0.5f)/cosf(w/2.0f-0.5f);
  cmd->fv[0]=-(x0-x1*k+x2*k*k)/d;
  cmd->fv[1]=(-2.0f*x0*k+x1+x1*k*k-2.0f*x2*k)/d;
  cmd->fv[2]=(x0*k*k-x1*k+x2)/d;
  cmd->fv[3]=-(2.0f*k+y1+y1*k*k-2.0f*y2*k)/d;
  cmd->fv[4]=(-k*k-y1*k+y2)/d;
  return 2;
}

/* Decode one command into voice, excluding oscillator commands.
 */
 
static int pcmprint_decode_voice_command(struct pcmprint *pcmprint,struct pcmprint_voice *voice,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  int srcp=1,err=-1;
  switch (src[0]) {
  
    // RATE, RATELFO, FMRANGE, and FMLFO are global to the voice:
    
    case PCMPRINT_CMD_RATE: {
        if ((err=pcmprint_env_decode(pcmprint,&voice->rate,src+srcp,srcc-srcp))<0) return err;
        pcmprint_env_scale(&voice->rate,65535.0f/(float)pcmprint->rate);
      } break;
      
    case PCMPRINT_CMD_RATELFO: {
        if ((err=pcmprint_lfo_decode(pcmprint,&voice->ratelfo,src+srcp,srcc-srcp))<0) return err;
      } break;
    
    case PCMPRINT_CMD_FMRANGE: {
        if (voice->fmrange<=0.0f) return -1; // not using an FM oscillator
        if ((err=pcmprint_env_decode(pcmprint,&voice->fmrangeenv,src+srcp,srcc-srcp))<0) return -1;
        pcmprint_env_scale(&voice->fmrangeenv,voice->fmrange);
      } break;
    
    case PCMPRINT_CMD_FMLFO: {
        if (voice->fmrange<=0.0f) return -1;
        err=pcmprint_lfo_decode(pcmprint,&voice->fmrangelfo,src+srcp,srcc-srcp);
        voice->fmrangelfo.mlt/=256.0f;
        voice->fmrangelfo.add=voice->fmrangelfo.mlt;
      } break;
      
    // All else produce a command:
      
    case PCMPRINT_CMD_LEVEL: err=pcmprint_decode_LEVEL(pcmprint,voice,src+srcp,srcc-srcp); break;
    case PCMPRINT_CMD_GAIN: err=pcmprint_decode_GAIN(pcmprint,voice,src+srcp,srcc-srcp); break;
    case PCMPRINT_CMD_CLIP: err=pcmprint_decode_CLIP(pcmprint,voice,src+srcp,srcc-srcp); break;
    case PCMPRINT_CMD_DELAY: err=pcmprint_decode_DELAY(pcmprint,voice,src+srcp,srcc-srcp); break;
    case PCMPRINT_CMD_BANDPASS: err=pcmprint_decode_BANDPASS(pcmprint,voice,src+srcp,srcc-srcp); break;
    case PCMPRINT_CMD_NOTCH: err=pcmprint_decode_NOTCH(pcmprint,voice,src+srcp,srcc-srcp); break;
    case PCMPRINT_CMD_LOPASS: err=pcmprint_decode_LOPASS(pcmprint,voice,src+srcp,srcc-srcp); break;
    case PCMPRINT_CMD_HIPASS: err=pcmprint_decode_HIPASS(pcmprint,voice,src+srcp,srcc-srcp); break;
    
  }
  if (err<0) return -1;
  return srcp+err;
}

/* Decode one pre-measured voice.
 */
 
static int pcmprint_decode_voice(struct pcmprint *pcmprint,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  struct pcmprint_voice *voice=pcmprint_voice_new(pcmprint);
  if (!voice) return -1;
  
  // First command must be the oscillator.
  int srcp=-1;
  int rate_required=1;
  switch (src[0]) {
    case PCMPRINT_CMD_SHAPE: srcp=pcmprint_decode_SHAPE(pcmprint,voice,src,srcc); break;
    case PCMPRINT_CMD_NOISE: rate_required=0; srcp=pcmprint_decode_NOISE(pcmprint,voice,src,srcc); break;
    case PCMPRINT_CMD_HARMONICS: srcp=pcmprint_decode_HARMONICS(pcmprint,voice,src,srcc); break;
    case PCMPRINT_CMD_FM: srcp=pcmprint_decode_FM(pcmprint,voice,src,srcc); break;
  }
  if (srcp<1) return -1;
  
  // After oscillator, we'll take them in any order.
  while (srcp<srcc) {
    int err=pcmprint_decode_voice_command(pcmprint,voice,src+srcp,srcc-srcp);
    if (err<1) return -1;
    srcp+=err;
  }
  
  // Confirm that we got all the required things.
  if (rate_required&&!voice->rate.inuse) return -1;
  if (voice->fmrange>0.0f) {
    if (!voice->fmrangeenv.inuse) {
      if (pcmprint_env_init_constant(pcmprint,&voice->fmrangeenv,voice->fmrange)<0) return -1;
    }
  }
  const struct pcmprint_cmd *cmd=voice->cmdv;
  int i=voice->cmdc,have_level=0;
  for (;i-->0;cmd++) {
    if (cmd->opcode==PCMPRINT_CMD_LEVEL) {
      have_level=1;
      break;
    }
  }
  if (!have_level) return -1;
  
  return srcp;
}

/* Decode serial, main entry point.
 */
 
int pcmprint_decode(struct pcmprint *pcmprint,const uint8_t *src,int srcc) {
  if (pcmprint->rate<1) return -1;
  if (pcmprint->pcm) return -1;
  if (!src) return -1;
  int srcp=0;
  
  // Total duration, then allocate the PCM dump.
  if (srcp>srcc-2) return -1;
  int durms=(src[srcp]<<8)|src[srcp+1];
  srcp+=2;
  if (durms<1) return -1;
  if (durms>INT_MAX/pcmprint->rate) return -1;
  int durframes=(durms*pcmprint->rate)/1000;
  if (durframes<1) durframes=1;
  if (!(pcmprint->pcm=pcmprint_pcm_new(durframes))) return -1;
  
  // Add voices until we run out. Zero voices is legal.
  while (srcp<srcc) {
    if (src[srcp]==0x01) { // "voice", skip it.
      srcp++;
      continue;
    }
    int vlen=0;
    while (srcp<srcc-vlen) {
      if (src[srcp+vlen]==0x01) break;
      int err=pcmprint_measure_command(src+srcp+vlen,srcc-vlen-srcp);
      if ((err<1)||(vlen+srcp>srcc-err)) {
        fprintf(stderr,"pcmprint: Invalid command 0x%02x at %d/%d\n",src[srcp+vlen],srcp+vlen,srcc);
        return -1;
      }
      vlen+=err;
    }
    if (pcmprint_decode_voice(pcmprint,src+srcp,vlen)<0) return -1;
    srcp+=vlen;
  }
  
  return 0;
}
