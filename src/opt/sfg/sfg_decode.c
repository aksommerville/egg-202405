#include "sfg_internal.h"

// In case I need diagnostics... Messages are here, ready to go.
#define FAIL(fmt,...) return -1;

/* Generate geometric waves.
 */
 
static void sfg_generate_sine(float *v) {
  float p=0.0f;
  float dp=(M_PI*2.0f)/(float)SFG_WAVE_SIZE_SAMPLES;
  int i=SFG_WAVE_SIZE_SAMPLES;
  for (;i-->0;v++,p+=dp) *v=sinf(p);
}

static void sfg_generate_square(float *v) {
  int halflen=SFG_WAVE_SIZE_SAMPLES>>1; // no risk of round-off; size is a power of two.
  int i;
  for (i=halflen;i-->0;v++) *v=1.0f;
  for (i=halflen;i-->0;v++) *v=-1.0f;
}

static void sfg_generate_sawup(float *v,int c) {
  float t=-1.0f;
  float dt=2.0f/(float)c;
  for (;c-->0;v++,t+=dt) *v=t;
}

static void sfg_generate_sawdown(float *v,int c) {
  float t=1.0f;
  float dt=-2.0f/(float)c;
  for (;c-->0;v++,t+=dt) *v=t;
}

static void sfg_generate_triangle(float *v) {
  int halflen=SFG_WAVE_SIZE_SAMPLES>>1; // no risk of round-off; size is a power of two.
  sfg_generate_sawup(v,halflen);
  sfg_generate_sawdown(v+halflen,halflen);
}

/* Apply 8-bit harmonics in place.
 */
 
static void sfg_apply_wave_harmonic(float *dst,const float *src,int step,float level) {
  int srcp=0;
  int i=SFG_WAVE_SIZE_SAMPLES;
  for (;i-->0;dst++,srcp+=step) {
    if (srcp>=SFG_WAVE_SIZE_SAMPLES) srcp-=SFG_WAVE_SIZE_SAMPLES; // Ensure wave size >=256 so we can't overrun by more than one cycle.
    (*dst)+=src[srcp]*level;
  }
}
 
static void sfg_apply_wave_harmonics(float *wave,const uint8_t *coefv,int coefc) {
  float tmp[SFG_WAVE_SIZE_SAMPLES]={0};
  int step=1;
  for (;coefc-->0;coefv++,step++) {
    if (!*coefv) continue;
    sfg_apply_wave_harmonic(tmp,wave,step,(*coefv)/255.0f);
  }
  memcpy(wave,tmp,sizeof(tmp));
}

/* Individual commands.
 */
  
static int sfg_printer_decode_op_level(struct sfg_op *op,struct sfg_voice *voice,struct sfg_printer *printer,const uint8_t *src,int srcc) {
  int srcp=sfg_env_decode(&op->env,src,srcc,printer->rate,1.0f/65535.0f);
  if (srcp<1) return -1;
  op->update=sfg_op_level_update;
  return srcp;
}
  
static int sfg_printer_decode_op_gain(struct sfg_op *op,struct sfg_voice *voice,struct sfg_printer *printer,const uint8_t *src,int srcc) {
  if (srcc<2) return -1;
  op->fv[0]=(float)src[0]+(float)src[1]/256.0f;
  op->update=sfg_op_gain_update;
  return 2;
}
  
static int sfg_printer_decode_op_clip(struct sfg_op *op,struct sfg_voice *voice,struct sfg_printer *printer,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  op->fv[0]=(float)src[0]/255.0f;
  op->update=sfg_op_clip_update;
  return 1;
}
  
static int sfg_printer_decode_op_delay(struct sfg_op *op,struct sfg_voice *voice,struct sfg_printer *printer,const uint8_t *src,int srcc) {
  if (srcc<6) return -1;
  int periodms=(src[0]<<8)|src[1];
  op->iv[0]=(periodms*printer->rate)/1000;
  if (op->iv[0]<1) op->iv[0]=1;
  if (!(op->buf=calloc(sizeof(float),op->iv[0]))) return -1;
  op->fv[0]=src[2]/255.0f;
  op->fv[1]=src[3]/255.0f;
  op->fv[2]=src[4]/255.0f;
  op->fv[3]=src[5]/255.0f;
  op->update=sfg_op_delay_update;
  return 6;
}
  
static int sfg_printer_decode_op_bandpass(struct sfg_op *op,struct sfg_voice *voice,struct sfg_printer *printer,const uint8_t *src,int srcc) {
  if (srcc<4) return -1;
  float mid=(float)((src[0]<<8)|src[1])/(float)printer->rate;
  float wid=(float)((src[2]<<8)|src[3])/(float)printer->rate;
  float r=1.0f-3.0f*wid;
  float cosfreq=cosf(M_PI*2.0f*mid);
  float k=(1.0f-2.0f*r*cosfreq+r*r)/(2.0f-2.0f*cosfreq);
  op->fv[0]=1.0f-k;
  op->fv[1]=2.0f*(k-r)*cosfreq;
  op->fv[2]=r*r-k;
  op->fv[3]=2.0f*r*cosfreq;
  op->fv[4]=-r*r;
  op->update=sfg_op_filter_update;
  return 4;
}
  
static int sfg_printer_decode_op_notch(struct sfg_op *op,struct sfg_voice *voice,struct sfg_printer *printer,const uint8_t *src,int srcc) {
  if (srcc<4) return -1;
  float mid=(float)((src[0]<<8)|src[1])/(float)printer->rate;
  float wid=(float)((src[2]<<8)|src[3])/(float)printer->rate;
  float r=1.0f-3.0f*wid;
  float cosfreq=cosf(M_PI*2.0f*mid);
  float k=(1.0f-2.0f*r*cosfreq+r*r)/(2.0f-2.0f*cosfreq);
  op->fv[0]=k;
  op->fv[1]=-2.0f*k*cosfreq;
  op->fv[2]=k;
  op->fv[3]=2.0f*r*cosfreq;
  op->fv[4]=-r*r;
  op->update=sfg_op_filter_update;
  return 4;
}
  
static int sfg_printer_decode_op_lopass(struct sfg_op *op,struct sfg_voice *voice,struct sfg_printer *printer,const uint8_t *src,int srcc) {
  if (srcc<2) return -1;
  float freq=(float)((src[0]<<8)|src[1])/(float)printer->rate;
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
  op->fv[0]=(x0-x1*k+x2*k*k)/d;
  op->fv[1]=(-2.0f*x0*k+x1+x1*k*k-2.0f*x2*k)/d;
  op->fv[2]=(x0*k*k-x1*k+x2)/d;
  op->fv[3]=(2.0f*k+y1+y1*k*k-2.0f*y2*k)/d;
  op->fv[4]=(-k*k-y1*k+y2)/d;
  op->update=sfg_op_filter_update;
  return 2;
}
  
static int sfg_printer_decode_op_hipass(struct sfg_op *op,struct sfg_voice *voice,struct sfg_printer *printer,const uint8_t *src,int srcc) {
  if (srcc<2) return -1;
  float freq=(float)((src[0]<<8)|src[1])/(float)printer->rate;
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
  op->fv[0]=-(x0-x1*k+x2*k*k)/d;
  op->fv[1]=(-2.0f*x0*k+x1+x1*k*k-2.0f*x2*k)/d;
  op->fv[2]=(x0*k*k-x1*k+x2)/d;
  op->fv[3]=-(2.0f*k+y1+y1*k*k-2.0f*y2*k)/d;
  op->fv[4]=(-k*k-y1*k+y2)/d;
  op->update=sfg_op_filter_update;
  return 2;
}

/* Decode the oscillator prefix for one voice.
 */
 
static int sfg_printer_decode_oscillator(struct sfg_voice *voice,struct sfg_printer *printer,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  uint8_t features=src[0];
  int srcp=1;

  if (features&0x01) {
    if (srcp>=srcc) return -1;
    voice->shape=src[srcp++];
  }
  
  const uint8_t *coefv=0;
  int coefc=0;
  if (features&0x02) {
    if (srcp>=srcc) return -1;
    coefc=src[srcp++];
    if (srcp>srcc-coefc) return -1;
    coefv=src+srcp;
    srcp+=coefc;
  }
  
  float fmscale=1.0f;
  if (features&0x04) {
    if (srcp>srcc-4) return -1;
    voice->fmrate=(float)src[srcp]+(float)src[srcp+1]/256.0f; srcp+=2;
    fmscale=(float)src[srcp]+(float)src[srcp+1]/256.0f; srcp+=2;
    voice->fmrate*=M_PI*2.0f; // radians, not a plain multiplier
  }
  
  if (features&0x08) {
    int err=sfg_env_decode(&voice->range,src+srcp,srcc-srcp,printer->rate,fmscale/65535.0f);
    if (err<0) return -1;
    srcp+=err;
  } else {
    sfg_env_constant(&voice->range,fmscale);
  }
  
  if (features&0x10) {
    int err=sfg_env_decode(&voice->rate,src+srcp,srcc-srcp,printer->rate,1.0f/(float)printer->rate);
    if (err<0) return -1;
    srcp+=err;
  } else {
    sfg_env_constant(&voice->rate,440.0f/(float)printer->rate);
  }
  
  if (features&0x20) {
    if (srcp>srcc-4) return -1;
    float ratelforate=(float)src[srcp]+(float)src[srcp+1]/256.0f; srcp+=2;
    int ratelfodepth=(src[srcp]<<8)|src[srcp+1]; srcp+=2; // cents
    voice->ratelfodp=(ratelforate*M_PI*2.0f)/(float)printer->rate;
    voice->ratelforange=(float)ratelfodepth/1200.0f; // cents=>power of 2
  }
  
  if (features&0xc0) FAIL("Unknown features (0x%02x) in voice.",features&0xc0)
  
  // Noise or silence, all other params would be noop, don't bother. (we do have to read them, can't short circuit earlier than this).
  if (voice->shape==5) {
    voice->oscillate=sfg_oscillate_noise;
    return srcp;
  }
  if (voice->shape==6) {
    voice->oscillate=sfg_oscillate_silence;
    return srcp;
  }
  
  // Generate the initial geometric wave.
  switch (voice->shape) {
    case 0x00: sfg_generate_sine(voice->wave); break;
    case 0x01: sfg_generate_square(voice->wave); break;
    case 0x02: sfg_generate_sawup(voice->wave,SFG_WAVE_SIZE_SAMPLES); break;
    case 0x03: sfg_generate_sawdown(voice->wave,SFG_WAVE_SIZE_SAMPLES); break;
    case 0x04: sfg_generate_triangle(voice->wave); break;
    default: FAIL("Unknown wave shape %d",voice->shape)
  }
  
  // Apply harmonics.
  if (coefc>0) sfg_apply_wave_harmonics(voice->wave,coefv,coefc);
  
  // Select the appropriate oscillator hook.
  if (!voice->rate.pointc&&!(features&0x24)) {
    // No rate envelope, rate LFO, or FM (no need to check the 'fmenv' bit 0x08).
    voice->oscillate=sfg_oscillate_flat;
    voice->cardpi=(uint32_t)(voice->rate.v*4294967296.0f);
  } else if (!(features&0x20)) {
    // No rate LFO.
    voice->oscillate=sfg_oscillate_lfno;
  } else {
    // No optimized oscillator available, do the real thing.
    voice->oscillate=sfg_oscillate_full;
  }
  
  return srcp;
}

/* Decode one voice.
 * Returns consumed length on success.
 */
 
static int sfg_printer_decode_voice(struct sfg_voice *voice,struct sfg_printer *printer,const uint8_t *src,int srcc) {
  int srcp=sfg_printer_decode_oscillator(voice,printer,src,srcc);
  if (srcp<0) return srcp;
  if (!voice->oscillate) FAIL("Voice did not acquire an oscillator.")
  while (srcp<srcc) {
    uint8_t opcode=src[srcp++];
    if (!opcode) break; // End Of Voice
    if (voice->opc>=voice->opa) {
      int na=voice->opa+4;
      if (na>INT_MAX/sizeof(struct sfg_op)) return -1;
      void *nv=realloc(voice->opv,sizeof(struct sfg_op)*na);
      if (!nv) return -1;
      voice->opv=nv;
      voice->opa=na;
    }
    struct sfg_op *op=voice->opv+voice->opc++;
    memset(op,0,sizeof(struct sfg_op));
    switch (opcode) {
      #define _(opcode,name) case opcode: { \
          int err=sfg_printer_decode_op_##name(op,voice,printer,src+srcp,srcc-srcp); \
          if (err<0) return err; \
          srcp+=err; \
        } break;
      _(0x01,level)
      _(0x02,gain)
      _(0x03,clip)
      _(0x04,delay)
      _(0x05,bandpass)
      _(0x06,notch)
      _(0x07,lopass)
      _(0x08,hipass)
      #undef _
      default: FAIL("Unknown opcode 0x%02x in voice pipeline.",opcode)
    }
    if (!op->update) return -1;
  }
  return srcp;
}

/* Decode, main entry point.
 */
 
int sfg_printer_decode(struct sfg_printer *printer,const uint8_t *src,int srcc) {
  if (!printer||printer->pcm||!src) return -1;
  
  /* Starts with fixed header:
   *   u16  Duration, ms.
   *   u8.8 Master.
   */
  if (srcc<4) return -1;
  int durms=(src[0]<<8)|src[1];
  double durs=(double)durms/1000.0; // Switch to floating-point transiently, to avoid integer overflow.
  int durframes=(int)(durs*printer->rate); // The final frame count shouldn't overflow; limit about 13 M.
  if (durframes<1) durframes=1;
  if (!(printer->pcm=sfg_pcm_new(durframes))) FAIL("Failed to allocate PCM, %d samples (dur %d ms)",durframes,durms)
  printer->master=(float)src[2]+(float)src[3]/256.0f;
  int srcp=4;
  
  /* Then decode voices until we run out of them.
   */
  while (srcp<srcc) {
    if (printer->voicec>=printer->voicea) {
      int na=printer->voicea+4;
      if (na>INT_MAX/sizeof(struct sfg_voice)) return -1;
      void *nv=realloc(printer->voicev,sizeof(struct sfg_voice)*na);
      if (!nv) return -1;
      printer->voicev=nv;
      printer->voicea=na;
    }
    struct sfg_voice *voice=printer->voicev+printer->voicec++;
    memset(voice,0,sizeof(struct sfg_voice));
    int err=sfg_printer_decode_voice(voice,printer,src+srcp,srcc-srcp);
    if (!err) err=-1;
    if (err<0) return err;
    srcp+=err;
    
    // If it uses the noop silence oscillator, drop the voice.
    if (voice->oscillate==sfg_oscillate_silence) {
      printer->voicec--;
      sfg_voice_cleanup(voice);
    }
  }
  
  return 0;
}
