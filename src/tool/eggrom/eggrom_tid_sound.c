#include "eggrom_internal.h"

/* Pcmprint compiler context.
 */
 
struct pcmprint_compiler {
  struct sr_encoder *dst;
  const struct romw_res *res;
  float master;
  int lenp;
  
  // Voice in progress.
  char wave[256]; int wavec;
  char rate[256]; int ratec;
  char ratelfo[32]; int ratelfoc;
  char fmrange[256]; int fmrangec;
  char fmlfo[32]; int fmlfoc;
  struct sr_encoder cmd;
  int voicelen;
};

static void pcmprint_compiler_cleanup(struct pcmprint_compiler *compiler) {
  sr_encoder_cleanup(&compiler->cmd);
}

/* Compile "wave".
 */
 
static int pcmprint_compile_wave(struct sr_encoder *dst,struct pcmprint_compiler *compiler,const char *src,int srcc) {

  // Seven cases are simple tokens.
  #define SIMPLE(kw,shapeid) if ((srcc==sizeof(kw)-1)&&!memcmp(src,kw,srcc)) { \
    if (sr_encode_u8(dst,0x02)<0) return -1; \
    if (sr_encode_u8(dst,shapeid)<0) return -1; \
    return 0; \
  }
  SIMPLE("sine",0)
  SIMPLE("square",1)
  SIMPLE("sawup",2)
  SIMPLE("sawdown",3)
  SIMPLE("saw",3)
  SIMPLE("triangle",4)
  SIMPLE("silence",0) // will be stricken later, just emit anything
  #undef SIMPLE
  if ((srcc==5)&&!memcmp(src,"noise",5)) return sr_encode_u8(dst,0x03);
  
  // "fm=RATE,RANGE" => 0x05 u8.8 u8.8
  if ((srcc>3)&&!memcmp(src,"fm=",3)) {
    int srcp=3;
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&(src[srcp++]!=',')) tokenc++;
    double rate;
    if (sr_double_eval(&rate,token,tokenc)<0) {
      fprintf(stderr,"Expected floating-point FM rate, found '%.*s'\n",tokenc,token);
      return -1;
    }
    token=src+srcp;
    tokenc=srcc-srcp;
    double range;
    if (sr_double_eval(&range,token,tokenc)<0) {
      fprintf(stderr,"Expected floating-point FM range, found '%.*s'\n",tokenc,token);
      return -1;
    }
    int irate=(int)(rate*256.0f);
    int irange=(int)(range*256.0f);
    if ((irate<0)||(irate>0xffff)||(irange<0)||(irange>0xffff)) {
      fprintf(stderr,"FM rate or range (%f,%f) out of range 0..256\n",rate,range);
      return -1;
    }
    if (sr_encode_u8(dst,0x05)<0) return -1;
    if (sr_encode_intbe(dst,irate,2)<0) return -1;
    if (sr_encode_intbe(dst,irange,2)<0) return -1;
    return 0;
  }

  /* Anything else, including empty, is harmonics:
   * text: wave COEFFICIENTS...
   * bin: 04 u8:COUNT u8:COEF...
   */
  if (sr_encode_u8(dst,0x04)<0) return -1;
  int coefcp=dst->c;
  if (sr_encode_u8(dst,0)<0) return -1;
  int srcp=0,coefc=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) {
      srcp++;
      continue;
    }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    double v;
    if (sr_double_eval(&v,token,tokenc)<0) {
      fprintf(stderr,"Expected floating-point harmonic coefficient, found '%.*s'\n",tokenc,token);
      return -1;
    }
    int iv=(int)(v*256.0f);
    if (iv==0x100) iv=0xff;
    if ((iv<0)||(iv>0xff)) {
      fprintf(stderr,"Coefficient %f out of range 0..1\n",v);
      return -1;
    }
    if (coefc<0xff) {
      if (sr_encode_u8(dst,iv)<0) return -1;
    }
    coefc++;
  }
  if (coefc>0xff) {
    fprintf(stderr,"WARNING: Ignoring some harmonic coefficients, limit 255\n");
    ((uint8_t*)dst->v)[coefcp]=0xff;
  } else {
    ((uint8_t*)dst->v)[coefcp]=coefc;
  }
  return 0;
}

/* Compile an envelope command.
 */
 
static int pcmprint_compile_env(struct sr_encoder *dst,struct pcmprint_compiler *compiler,uint8_t opcode,float vsize,const char *src,int srcc) {
  if (sr_encode_u8(dst,opcode)<0) return -1;
  
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *token=src+srcp;
  int tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  double value;
  if (sr_double_eval(&value,token,tokenc)<0) {
    fprintf(stderr,"Expected floating-point envelope point, found '%.*s'\n",tokenc,token);
    return -1;
  }
  int ivalue=(int)(value*vsize);
  if (ivalue==0x10000) ivalue=0xffff; // so close it might have been a rounding error, make it legal
  if ((ivalue<0)||(ivalue>0xffff)) {
    fprintf(stderr,"Value %f out of range for this envelope.\n",value);
    return -1;
  }
  if (sr_encode_intbe(dst,ivalue,2)<0) return -1;
  
  int time=0;
  int ptcp=dst->c;
  if (sr_encode_u8(dst,0)<0) return -1; // point-count placeholder
  int ptc=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) {
      srcp++;
      continue;
    }
    token=src+srcp;
    tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    int abs=0;
    if ((tokenc>=1)&&(token[0]=='@')) {
      abs=1;
      token++;
      tokenc--;
    }
    int delay;
    if ((sr_int_eval(&delay,token,tokenc)<2)||(delay<0)||(delay>0xffff)) {
      fprintf(stderr,"Expected ms delay in 0..65535, found '%.*s'\n",tokenc,token);
      return -1;
    }
    if (abs) {
      if (delay<time) {
        fprintf(stderr,"Invalid absolute delay to %d ms, current time %d ms\n",delay,time);
        return -1;
      }
      delay-=time;
    }
    time+=delay;
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    token=src+srcp;
    tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    if (sr_double_eval(&value,token,tokenc)<0) {
      fprintf(stderr,"Expected floating-point envelope point, found '%.*s'\n",tokenc,token);
      return -1;
    }
    ivalue=(int)(value*vsize);
    if (ivalue==0x10000) ivalue=0xffff; // so close it might have been a rounding error, make it legal
    if ((ivalue<0)||(ivalue>0xffff)) {
      fprintf(stderr,"Value %f out of range for this envelope.\n",value);
      return -1;
    }
    if (sr_encode_intbe(dst,delay,2)<0) return -1;
    if (sr_encode_intbe(dst,ivalue,2)<0) return -1;
    ptc++;
  }
  if (ptc>0xff) {
    fprintf(stderr,"Too many envelope points. %d, limit 255\n",ptc);
    return -1;
  }
  ((uint8_t*)dst->v)[ptcp]=ptc;
  if (time>compiler->voicelen) compiler->voicelen=time;
  return 0;
}

/* Compile an LFO command.
 */
 
static int pcmprint_compile_lfo(struct sr_encoder *dst,struct pcmprint_compiler *compiler,uint8_t opcode,float mltsize,const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *token=src+srcp;
  int tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  double rate;
  if (sr_double_eval(&rate,token,tokenc)<0) {
    fprintf(stderr,"Expected floating-point LFO rate, found '%.*s'\n",tokenc,token);
    return -1;
  }
  int irate=(int)(rate*256.0f);
  if ((irate<0)||(irate>0xffff)) {
    fprintf(stderr,"LFO rate %f must be in 0..256 hz\n",rate);
    return -1;
  }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  double amp;
  if (sr_double_eval(&amp,token,tokenc)<0) {
    fprintf(stderr,"Expected floating-point LFO amplitude, found '%.*s'\n",tokenc,token);
    return -1;
  }
  int iamp=(int)(amp*mltsize);
  if ((iamp<0)||(iamp>0xffff)) {
    fprintf(stderr,"Amplitude %f out of range for this LFO\n",amp);
    return -1;
  }
  if (sr_encode_u8(dst,opcode)<0) return -1;
  if (sr_encode_intbe(dst,irate,2)<0) return -1;
  if (sr_encode_intbe(dst,iamp,2)<0) return -1;
  return 0;
}

/* Compile "delay".
 */
 
static int pcmprint_compile_delay(struct sr_encoder *dst,struct pcmprint_compiler *compiler,const char *src,int srcc) {
  int len,dry,wet,sto,fbk,srcp=0;
  #define RDTOKEN \
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; \
    const char *token=src+srcp; \
    int tokenc=0; \
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++; \
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  #define RDINT(name) { \
    RDTOKEN \
    if (sr_int_eval(&name,token,tokenc)<2) { \
      fprintf(stderr,"Expected integer, found '%.*s'\n",tokenc,token); \
      return -1; \
    } \
  }
  #define RDFLT(name) { \
    RDTOKEN \
    double f; \
    if ((sr_double_eval(&f,token,tokenc)<0)||(f<0.0)||(f>1.0)) { \
      fprintf(stderr,"Expected float in 0..1, found '%.*s'\n",tokenc,token); \
      return -1; \
    } \
    if (f<=0.0) name=0; \
    else if (f>=1.0) name=0xff; \
    else name=(int)(f*255.0); \
  }
  RDINT(len)
  RDFLT(dry)
  RDFLT(wet)
  RDFLT(sto)
  RDFLT(fbk)
  #undef RDFLT
  #undef RDINT
  #undef RDTOKEN
  if (srcp<srcc) {
    fprintf(stderr,"Unexpected tokens after 'delay' command\n");
    return -1;
  }
  if ((len<0)||(len>0xffff)) {
    fprintf(stderr,"Delay length must be in 0..65535, found %d\n",len);
    return -1;
  }
  if (sr_encode_u8(dst,0x0d)<0) return -1;
  if (sr_encode_intbe(dst,len,2)<0) return -1;
  if (sr_encode_u8(dst,dry)<0) return -1;
  if (sr_encode_u8(dst,wet)<0) return -1;
  if (sr_encode_u8(dst,sto)<0) return -1;
  if (sr_encode_u8(dst,fbk)<0) return -1;
  return 0;
}

/* Compile "bandpass" or "notch".
 */
 
static int pcmprint_compile_filter(struct sr_encoder *dst,struct pcmprint_compiler *compiler,uint8_t opcode,const char *src,int srcc) {
  
  const char *token;
  int srcp=0,tokenc,mid,wid;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  if ((sr_int_eval(&mid,token,tokenc)<2)||(mid<0)||(mid>0xffff)) {
    fprintf(stderr,"Expected frequency in 0..65535, found '%.*s'\n",tokenc,token);
    return -1;
  }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  if ((sr_int_eval(&wid,token,tokenc)<2)||(mid<0)||(mid>0xffff)) {
    fprintf(stderr,"Expected frequency in 0..65535, found '%.*s'\n",tokenc,token);
    return -1;
  }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp<srcc) {
    fprintf(stderr,"Unexpected extra tokens after filter command.\n");
    return -1;
  }
  
  if (sr_encode_u8(dst,opcode)<0) return -1;
  if (sr_encode_intbe(dst,mid,2)<0) return -1;
  if (sr_encode_intbe(dst,wid,2)<0) return -1;
  return 0;
}

/* Compile command with a single argument.
 */
 
static int pcmprint_compile_scalar(struct sr_encoder *dst,struct pcmprint_compiler *compiler,uint8_t opcode,int wordsize,float fractsize,const char *src,int srcc) {
  double v;
  if (sr_double_eval(&v,src,srcc)<0) {
    fprintf(stderr,"Expected one float, found '%.*s'\n",srcc,src);
    return -1;
  }
  v*=fractsize;
  int i=(int)v;
  int limit=1<<(wordsize*8);
  if (i>=limit) i=limit-1;
  else if (i<0) i=0;
  if (sr_encode_u8(dst,opcode)<0) return -1;
  if (sr_encode_intbe(dst,i,wordsize)<0) return -1;
  return 0;
}

/* Apply one command to the voice in progress.
 */
 
static int pcmprint_compile_command(
  struct pcmprint_compiler *compiler,
  const char *kw,int kwc,
  const char *arg,int argc,
  const char *path,int lineno
) {
  int err;
  
  if ((kwc==5)&&!memcmp(kw,"level",5)) {
    return pcmprint_compile_env(&compiler->cmd,compiler,0x0a,65536.0f,arg,argc);
  }
  if ((kwc==4)&&!memcmp(kw,"gain",4)) {
    return pcmprint_compile_scalar(&compiler->cmd,compiler,0x0b,2,256.0f,arg,argc);
  }
  if ((kwc==4)&&!memcmp(kw,"clip",4)) {
    return pcmprint_compile_scalar(&compiler->cmd,compiler,0x0c,1,256.0f,arg,argc);
  }
  if ((kwc==5)&&!memcmp(kw,"delay",5)) {
    return pcmprint_compile_delay(&compiler->cmd,compiler,arg,argc);
  }
  if ((kwc==8)&&!memcmp(kw,"bandpass",8)) {
    return pcmprint_compile_filter(&compiler->cmd,compiler,0x0e,arg,argc);
  }
  if ((kwc==5)&&!memcmp(kw,"notch",5)) {
    return pcmprint_compile_filter(&compiler->cmd,compiler,0x0f,arg,argc);
  }
  if ((kwc==6)&&!memcmp(kw,"lopass",6)) {
    return pcmprint_compile_scalar(&compiler->cmd,compiler,0x10,2,1.0f,arg,argc);
  }
  if ((kwc==6)&&!memcmp(kw,"hipass",6)) {
    return pcmprint_compile_scalar(&compiler->cmd,compiler,0x11,2,1.0f,arg,argc);
  }
  
  #define SAVE_FOR_LATER(tag) if ((kwc==sizeof(#tag)-1)&&!memcmp(kw,#tag,kwc)) { \
    if (compiler->tag##c) { \
      fprintf(stderr,"%s:%d: Multiple '%s' commands, limit 1.\n",path,lineno,#tag); \
      return -2; \
    } \
    if (argc>sizeof(compiler->tag)) { \
      fprintf(stderr,"%s:%d: Limit %d bytes for '%s' command args, found %d.\n",path,lineno,(int)sizeof(compiler->tag),#tag,argc); \
      return -2; \
    } \
    memcpy(compiler->tag,arg,argc); \
    compiler->tag##c=argc; \
    return 0; \
  }
  SAVE_FOR_LATER(wave)
  SAVE_FOR_LATER(rate)
  SAVE_FOR_LATER(ratelfo)
  SAVE_FOR_LATER(fmrange)
  SAVE_FOR_LATER(fmlfo)
  #undef SAVE_FOR_LATER
  
  fprintf(stderr,"%s:%d: Unknown keyword '%.*s'\n",path,lineno,kwc,kw);
  return -2;
}

/* Finish voice in progress and reset state for the next one.
 */
 
static int pcmprint_compiler_finish_voice(struct pcmprint_compiler *compiler) {
  int err;

  // Nothing significant since the last voice command? Do nothing.
  if (
    !compiler->wavec&&
    !compiler->ratec&&
    !compiler->ratelfoc&&
    !compiler->fmrangec&&
    !compiler->fmlfoc&&
    !compiler->cmd.c
  ) return 0;
  
  int dstc0=compiler->dst->c;
  
  /* Wave and rate are both required, and must come first and second in the binary.
   * Except actually rate is not required for noise.
   */
  if ((err=pcmprint_compile_wave(compiler->dst,compiler,compiler->wave,compiler->wavec))<0) {
    if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error compiling 'wave' command.\n",compiler->res->path,compiler->res->lineno);
    return -2;
  }
  if ((compiler->wavec==5)&&!memcmp(compiler->wave,"noise",5)) {
    if (compiler->ratec) {
      fprintf(stderr,"%s:%d:WARNING: Ignoring 'rate' command for noise voice.\n",compiler->res->path,compiler->res->lineno);
    }
  } else {
    if ((err=pcmprint_compile_env(compiler->dst,compiler,0x06,1.0f,compiler->rate,compiler->ratec))<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error compiling 'rate' command.\n",compiler->res->path,compiler->res->lineno);
      return -2;
    }
  }
  
  /* ratelfo, fmrange, fmlfo: Separated because they may only appear once.
   */
  if (compiler->ratelfoc) {
    if ((err=pcmprint_compile_lfo(compiler->dst,compiler,0x07,1.0f,compiler->ratelfo,compiler->ratelfoc))<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error compiling 'ratelfo' command.\n",compiler->res->path,compiler->res->lineno);
      return -2;
    }
  }
  if (compiler->fmrangec) {
    if ((err=pcmprint_compile_env(compiler->dst,compiler,0x08,65536.0f,compiler->fmrange,compiler->fmrangec))<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error compiling 'fmrange' command.\n",compiler->res->path,compiler->res->lineno);
      return -2;
    }
  }
  if (compiler->fmlfoc) {
    if ((err=pcmprint_compile_lfo(compiler->dst,compiler,0x09,256.0f,compiler->fmlfo,compiler->fmlfoc))<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error compiling 'fmlfo' command.\n",compiler->res->path,compiler->res->lineno);
      return -2;
    }
  }
  
  /* All other commands are encoded as we receive them, copy verbatim now.
   */
  if (sr_encode_raw(compiler->dst,compiler->cmd.v,compiler->cmd.c)<0) return -1;
  
  /* If we have a master trim, apply it.
   */
  if (compiler->master<1.0f) {
    int v=(int)(compiler->master*256.0f);
    if (sr_encode_u8(compiler->dst,0x0b)<0) return -1;
    if (sr_encode_intbe(compiler->dst,v,2)<0) return -1;
  }

  /* Terminate the binary with a VOICE command.
   */
  if (sr_encode_u8(compiler->dst,0x01)<0) return -1;
  
  /* If the wave shape was "silence", it will have emitted garbage and we should now back up over it.
   * It's important that we test-compile the whole thing anyway, in order to capture the longest envelope length.
   */
  if ((compiler->wavec==7)&&!memcmp(compiler->wave,"silence",7)) {
    compiler->dst->c=dstc0;
  }
  
  /* Compare this voice length to whatever we encoded in the header, replace if longer.
   */
  {
    uint8_t *B=((uint8_t*)compiler->dst->v)+compiler->lenp;
    int havelen=(B[0]<<8)|B[1];
    if (compiler->voicelen>havelen) {
      if (compiler->voicelen>0xffff) {
        fprintf(stderr,"%s:%d: Voice length %d exceeds limit of 65535\n",compiler->res->path,compiler->res->lineno,compiler->voicelen);
        return -2;
      }
      B[0]=compiler->voicelen>>8;
      B[1]=compiler->voicelen;
    }
  }
  
  /* Finally, return compiler to the no-voice state.
   */
  compiler->wavec=0;
  compiler->ratec=0;
  compiler->ratelfoc=0;
  compiler->fmrangec=0;
  compiler->fmlfoc=0;
  compiler->cmd.c=0;
  compiler->voicelen=0;
  
  return 0;
}

/* Compile, in context.
 */
 
static int pcmprint_compile(struct pcmprint_compiler *compiler) {

  compiler->lenp=compiler->dst->c;
  if (sr_encode_raw(compiler->dst,"\x00\x01",2)<0) return -1; // length placeholder

  int masterok=1,err;
  struct sr_decoder decoder={.v=compiler->res->serial,.c=compiler->res->serialc};
  int linec,lineno=compiler->res->lineno;
  const char *line;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    int i=0; for (;i<linec;i++) if (line[i]=='#') linec=i;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec) continue;
    
    // We are always interested in a space-delimited keyword.
    const char *kw=line;
    int kwc=0,linep=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) kwc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    
    // The first line can be 'master'.
    if (masterok) {
      if ((kwc==6)&&!memcmp(kw,"master",6)) {
        double v;
        if ((sr_double_eval(&v,line+linep,linec-linep)<0)||(v<0.0)||(v>1.0)) {
          fprintf(stderr,"%s:%d: Expected float in 0..1, found '%.*s'\n",compiler->res->path,lineno,linec-linep,line+linep);
          return -2;
        }
        compiler->master=v;
        masterok=0;
        continue;
      }
      masterok=0;
    }
    
    // 'voice' begins a new voice.
    if ((kwc==5)&&!memcmp(kw,"voice",5)) {
      if ((err=pcmprint_compiler_finish_voice(compiler))<0) {
        if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error finishing voice.\n",compiler->res->path,lineno);
        return -2;
      }
      continue;
    }
    
    // All other commands, dispatch separately.
    if ((err=pcmprint_compile_command(compiler,kw,kwc,line+linep,linec-linep,compiler->res->path,lineno))<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error compiling sound.\n",compiler->res->path,lineno);
      return -2;
    }
  }
  // Wrap up the last voice if there is one.
  if ((err=pcmprint_compiler_finish_voice(compiler))<0) {
    if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error finishing voice.\n",compiler->res->path,lineno);
    return -2;
  }
  
  // If we have a spurious VOICE command at the end, likely, drop it.
  // It's not illegal or anything, just not necessary.
  if ((compiler->dst->c-compiler->lenp>=3)&&(((uint8_t*)compiler->dst->v)[compiler->dst->c-1]==0x01)) compiler->dst->c--;
  
  return 0;
}

/* Reencode sound if necessary.
 */
 
int eggrom_sound_compile(struct sr_encoder *dst,const struct romw_res *res) {
  
  if ((res->serialc>=12)&&!memcmp(res->serial,"RIFF",4)&&!memcmp((char*)res->serial+8,"WAVE",4)) {
    fprintf(stderr,"%s: WAV files not yet supported.\n",res->path);//TODO
    return -2;
  }
  
  struct pcmprint_compiler compiler={
    .dst=dst,
    .res=res,
    .master=1.0f,
  };
  int err=pcmprint_compile(&compiler);
  pcmprint_compiler_cleanup(&compiler);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error compiling sound text.\n",res->path,res->lineno);
    return -2;
  }
  return 0;
}

/* Expand file.
 */
 
int eggrom_sound_expand(const char *src,int srcc,uint16_t qual,const char *path) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int linec,lineno=0,olineno=0;
  const char *line;
  struct romw_res *inprogress=0;
  struct sr_encoder body={0};
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    int i=0;
    for (;i<linec;i++) if (line[i]=='#') linec=i;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec) continue;
    
    const char *kw=line;
    int kwc=0;
    while ((kwc<linec)&&((unsigned char)kw[kwc]>0x20)) kwc++;
    
    if ((kwc==3)&&!memcmp(kw,"end",3)) {
      if (!inprogress) {
        fprintf(stderr,"%s:%d: Unexpected 'end'\n",path,lineno);
        sr_encoder_cleanup(&body);
        return -2;
      }
      if (romw_res_handoff_serial(inprogress,body.v,body.c)<0) {
        sr_encoder_cleanup(&body);
        return -1;
      }
      memset(&body,0,sizeof(struct sr_encoder));
      inprogress=0;
      olineno=0;
      continue;
    }
    
    if ((kwc==5)&&!memcmp(kw,"sound",5)) {
      if (inprogress) {
        fprintf(stderr,"%s:%d: Unclosed sound block (a new one tries to begin at line %d).\n",path,olineno,lineno);
        sr_encoder_cleanup(&body);
        return -2;
      }
      if (!(inprogress=romw_res_add(eggrom.romw))) {
        sr_encoder_cleanup(&body);
        return -1;
      }
      inprogress->tid=EGG_TID_sound;
      inprogress->lineno=lineno;
      romw_res_set_path(inprogress,path,-1);
      int linep=kwc;
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
      const char *name=line+linep;
      int namec=linec-linep;
      if (namec<1) {
        fprintf(stderr,"%s:%d: ID or Name required after 'sound'\n",path,lineno);
        sr_encoder_cleanup(&body);
        return -2;
      }
      if ((name[0]>='0')&&(name[0]<='9')) {
        int rid=0;
        if ((sr_int_eval(&rid,name,namec)<2)||(rid<1)||(rid>0xffff)) {
          fprintf(stderr,"%s:%d: ID must be an integer in 1..65535 (found '%.*s')\n",path,lineno,namec,name);
          sr_encoder_cleanup(&body);
          return -2;
        }
        inprogress->rid=rid;
      } else {
        if (romw_res_set_name(inprogress,name,namec)<0) {
          sr_encoder_cleanup(&body);
          return -1;
        }
      }
      continue;
    }
    
    if (
      (sr_encode_raw(&body,line,linec)<0)||
      (sr_encode_u8(&body,0x0a)<0)
    ) {
      sr_encoder_cleanup(&body);
      return -1;
    }
  }
  sr_encoder_cleanup(&body);
  if (inprogress) {
    fprintf(stderr,"%s:%d: Unclosed sound block.\n",path,olineno);
    return -2;
  }
  return 0;
}
