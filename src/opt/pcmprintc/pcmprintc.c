#include "pcmprintc_internal.h"

/* Cleanup.
 */
 
void pcmprintc_del(struct pcmprintc *ppc) {
  if (!ppc) return;
  sr_encoder_cleanup(&ppc->dst);
  if (ppc->commandv) free(ppc->commandv);
  free(ppc);
}

/* New.
 */

struct pcmprintc *pcmprintc_new(const char *src,int srcc,const char *refname,int lineno0) {
  struct pcmprintc *ppc=calloc(1,sizeof(struct pcmprintc));
  if (!ppc) return 0;
  ppc->src.v=src;
  ppc->src.c=srcc;
  if (refname) ppc->refname=refname;
  else ppc->refname="<anonymous>";
  ppc->lineno=lineno0;
  return ppc;
}

/* Helpers for command parsing.
 */
 
#define RDINT(lo,hi) ({ \
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; \
  const char *_t=src+srcp; \
  int _tc=0; \
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) _tc++; \
  int _i; \
  if ((sr_int_eval(&_i,_t,_tc)<2)||(_i<lo)||(_i>hi)) { \
    fprintf(stderr,"%s:%d: Expected integer in %d..%d, found '%.*s'\n",ppc->refname,lineno,lo,hi,_tc,_t); \
    return -2; \
  } \
  (_i); \
})
 
#define RDFLOAT ({ \
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; \
  const char *_t=src+srcp; \
  int _tc=0; \
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) _tc++; \
  double _f; \
  if (sr_double_eval(&_f,_t,_tc)<0) { \
    fprintf(stderr,"%s:%d: Expected float, found '%.*s'\n",ppc->refname,lineno,_tc,_t); \
    return -2; \
  } \
  (float)(_f); \
})

/* wave sine|square|sawup|sawdown|triangle|noise|silence
 * wave COEFFICIENTS...
 * wave fm=RATE,RANGE
 */
 
static int pcmprintc_output_wave(struct pcmprintc *ppc,int lineno,const char *src,int srcc) {
  if (srcc<1) goto _malformed_;
  
  // Starts with a digit, it must be coefficients.
  if ((src[0]>='0')&&(src[0]<='9')) {
    int srcp=0;
    if (sr_encode_u8(&ppc->dst,0x04)<0) return -1;
    int lenp=ppc->dst.c;
    if (sr_encode_u8(&ppc->dst,0x00)<0) return -1; // length placeholder
    while (srcp<srcc) {
      float f=RDFLOAT;
      uint8_t v=(f<=0.0f)?0x00:(f>=1.0f)?0xff:((uint8_t)(f*256.0f));
      if (sr_encode_u8(&ppc->dst,v)<0) return -1;
    }
    int coefc=ppc->dst.c-lenp-1;
    if (coefc>0xff) {
      fprintf(stderr,"%s:%d: Too many coefficients (%d, limit 255).\n",ppc->refname,lineno,coefc);
      return -2;
    }
    ((uint8_t*)ppc->dst.v)[lenp]=coefc;
    return 0;
  }
  
  // "fm=RATE,RANGE"
  if ((srcc>3)&&!memcmp(src,"fm=",3)) {
    int srcp=3;
    double rate,range;
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&(src[srcp++]!=',')) tokenc++;
    if ((sr_double_eval(&rate,token,tokenc)<0)||(rate<0.0)||(rate>256.0)) {
      fprintf(stderr,"%s:%d: Expected FM rate in 0.0 .. 256.0, found '%.*s'\n",ppc->refname,lineno,tokenc,token);
      return -2;
    }
    token=src+srcp;
    tokenc=srcc-srcp;
    if ((sr_double_eval(&range,token,tokenc)<0)||(range<0.0)||(range>256.0)) {
      fprintf(stderr,"%s:%d: Expected FM rate in 0.0 .. 256.0, found '%.*s'\n",ppc->refname,lineno,tokenc,token);
      return -2;
    }
    int irate=(int)(rate*256.0); if (irate<0) irate=0; else if (irate>0xffff) irate=0xffff;
    int irange=(int)(range*256.0); if (irange<0) irange=0; else if (irange>0xffff) irange=0xffff;
    if (sr_encode_u8(&ppc->dst,0x05)<0) return -1;
    if (sr_encode_intbe(&ppc->dst,irate,2)<0) return -1;
    if (sr_encode_intbe(&ppc->dst,irange,2)<0) return -1;
    return 0;
  }
  
  // Named wave: sine square sawup sawdown triangle noise silence
  // (0,1,2,3,4)=(sine,square,sawup,sawdown,triangle)
  if ((srcc==4)&&!memcmp(src,"sine",4)) return sr_encode_raw(&ppc->dst,"\x02\x00",2);
  if ((srcc==6)&&!memcmp(src,"square",6)) return sr_encode_raw(&ppc->dst,"\x02\x01",2);
  if ((srcc==5)&&!memcmp(src,"sawup",5)) return sr_encode_raw(&ppc->dst,"\x02\x02",2);
  if ((srcc==7)&&!memcmp(src,"sawdown",7)) return sr_encode_raw(&ppc->dst,"\x02\x03",2);
  if ((srcc==8)&&!memcmp(src,"triangle",8)) return sr_encode_raw(&ppc->dst,"\x02\x04",2);
  if ((srcc==5)&&!memcmp(src,"noise",5)) return sr_encode_raw(&ppc->dst,"\x03",1);
  if ((srcc==7)&&!memcmp(src,"silence",7)) { ppc->discard_voice=1; return 0; }
  
 _malformed_:;
  fprintf(stderr,"%s:%d: Expected 'fm=RATE,RANGE', wave coefficients, or a shape name, after 'wave'.\n",ppc->refname,lineno);
  return -2;
}

/* rate HZ [MS HZ|@MS HZ...] => 0x06
 * fmrange V [MS V|@MS V...] => 0x08
 * level V [MS V|@MS V...] => 0x0a
 */
 
static int pcmprintc_output_env(struct pcmprintc *ppc,int lineno,uint8_t opcode,float scale,const char *src,int srcc) {
  int srcp=0;
  if (opcode==0x0a) ppc->recent_level_length=0;
  if (sr_encode_u8(&ppc->dst,opcode)<0) return -1;
  #define VALUE { \
    float v=RDFLOAT; \
    v*=scale; \
    if (opcode==0x0a) v*=ppc->master; \
    int vi=(int)(v); \
    if (vi==0x10000) vi=0xffff; \
    else if ((vi<0)||(vi>0xffff)) { \
      fprintf(stderr,"%s:%d: Value out of range for envelope.\n",ppc->refname,lineno); \
      return -2; \
    } \
    if (sr_encode_intbe(&ppc->dst,vi,2)<0) return -1; \
  }
  VALUE
  int ptc=0,ptcp=ppc->dst.c;
  if (sr_encode_u8(&ppc->dst,0)<0) return -1; // ptc placeholder
  while (srcp<srcc) {
    int ms=RDINT(0,0xffff);
    if (opcode==0x0a) ppc->recent_level_length+=ms;
    if (sr_encode_intbe(&ppc->dst,ms,2)<0) return -1;
    VALUE
    ptc++;
  }
  #undef VALUE
  if (ptc>0xff) {
    fprintf(stderr,"%s:%d: Too many points in envelope (%d, limit 255)\n",ppc->refname,lineno,ptc);
    return -2;
  }
  ((uint8_t*)ppc->dst.v)[ptcp]=ptc;
  return 0;
}

/* ratelfo HZ CENTS => 0x07
 * fmlfo HZ MLT => 0x09
 */
 
static int pcmprintc_output_lfo(struct pcmprintc *ppc,int lineno,uint8_t opcode,float scale,const char *src,int srcc) {
  if (sr_encode_u8(&ppc->dst,opcode)<0) return -1;
  int srcp=0;
  float rate=RDFLOAT;
  float v=RDFLOAT;
  v*=scale;
  int irate=(int)rate;
  if ((irate<0)||(irate>0xffff)) {
    fprintf(stderr,"%s:%d: LFO rate out of range (0..256)\n",ppc->refname,lineno);
    return -2;
  }
  if (sr_encode_intbe(&ppc->dst,irate,2)<0) return -1;
  int iv=(int)v;
  if ((iv<0)||(iv>0xffff)) {
    fprintf(stderr,"%s:%d: LFO amplitude out of range.\n",ppc->refname,lineno);
    return -2;
  }
  if (sr_encode_intbe(&ppc->dst,iv,2)<0) return -1;
  return 0;
}

/* Scalars.
 */
 
static int pcmprintc_output_u88(struct pcmprintc *ppc,int lineno,uint8_t opcode,const char *src,int srcc) {
  int srcp=0;
  float v=RDFLOAT;
  int vi=(int)(v*256.0f);
  if ((vi<0)||(vi>0xffff)) {
    fprintf(stderr,"%s:%d: Scalar out of range (0..256)\n",ppc->refname,lineno);
    return -2;
  }
  if (sr_encode_u8(&ppc->dst,opcode)<0) return -1;
  if (sr_encode_intbe(&ppc->dst,vi,2)<0) return -1;
  return 0;
}
 
static int pcmprintc_output_u08(struct pcmprintc *ppc,int lineno,uint8_t opcode,const char *src,int srcc) {
  int srcp=0;
  float v=RDFLOAT;
  int vi=(int)(v*256.0f);
  if (vi==0x100) vi=0xff;
  else if ((vi<0)||(vi>0xff)) {
    fprintf(stderr,"%s:%d: Scalar out of range (0..1)\n",ppc->refname,lineno);
    return -2;
  }
  if (sr_encode_u8(&ppc->dst,opcode)<0) return -1;
  if (sr_encode_u8(&ppc->dst,vi)<0) return -1;
  return 0;
}

/* delay MS DRY WET STORE FEEDBACK
 */
 
static int pcmprintc_output_delay(struct pcmprintc *ppc,int lineno,const char *src,int srcc) {
  int srcp=0;
  int ms=RDINT(0,0xffff);
  float dry=RDFLOAT;
  float wet=RDFLOAT;
  float sto=RDFLOAT;
  float fbk=RDFLOAT;
  if (sr_encode_u8(&ppc->dst,0x0d)<0) return -1;
  if (sr_encode_intbe(&ppc->dst,ms,2)<0) return -1;
  if (sr_encode_u8(&ppc->dst,(int)(dry*256.0f))<0) return -1;
  if (sr_encode_u8(&ppc->dst,(int)(wet*256.0f))<0) return -1;
  if (sr_encode_u8(&ppc->dst,(int)(sto*256.0f))<0) return -1;
  if (sr_encode_u8(&ppc->dst,(int)(fbk*256.0f))<0) return -1;
  if (srcp<srcc) {
    fprintf(stderr,"%s:%d: Unexpected tokens after 'delay' command.\n",ppc->refname,lineno);
    return -2;
  }
  return 0;
}

/* bandpass MID WIDTH => 0x0e
 * notch MID WIDTH => 0x0f
 * lopass HZ => 0x10
 * hipass HZ => 0x11
 */
 
static int pcmprintc_output_filter(struct pcmprintc *ppc,int lineno,uint8_t opcode,int paramc,const char *src,int srcc) {
  int srcp=0;
  if (sr_encode_u8(&ppc->dst,opcode)<0) return -1;
  while (paramc-->0) {
    int v=RDINT(0,0xffff);
    if (sr_encode_intbe(&ppc->dst,v,2)<0) return -1;
  }
  if (srcp<srcc) {
    fprintf(stderr,"%s:%d: Unexpected tokens after filter command.\n",ppc->refname,lineno);
    return -2;
  }
  return 0;
}

/* Emit one command.
 */
 
static int pcmprintc_output_command(struct pcmprintc *ppc,const struct pcmprintc_command *cmd) {
  if ((cmd->kwc==4)&&!memcmp(cmd->kw,"rate",4)) return pcmprintc_output_env(ppc,cmd->lineno,0x06,1.0f,cmd->arg,cmd->argc);
  if ((cmd->kwc==7)&&!memcmp(cmd->kw,"fmrange",7)) return pcmprintc_output_env(ppc,cmd->lineno,0x08,65536.0f,cmd->arg,cmd->argc);
  if ((cmd->kwc==5)&&!memcmp(cmd->kw,"level",5)) return pcmprintc_output_env(ppc,cmd->lineno,0x0a,65536.0f,cmd->arg,cmd->argc);
  if ((cmd->kwc==7)&&!memcmp(cmd->kw,"ratelfo",7)) return pcmprintc_output_lfo(ppc,cmd->lineno,0x07,1.0f,cmd->arg,cmd->argc);
  if ((cmd->kwc==5)&&!memcmp(cmd->kw,"fmlfo",5)) return pcmprintc_output_lfo(ppc,cmd->lineno,0x09,256.0f,cmd->arg,cmd->argc);
  if ((cmd->kwc==4)&&!memcmp(cmd->kw,"gain",4)) return pcmprintc_output_u88(ppc,cmd->lineno,0x0b,cmd->arg,cmd->argc);
  if ((cmd->kwc==4)&&!memcmp(cmd->kw,"clip",4)) return pcmprintc_output_u08(ppc,cmd->lineno,0x0c,cmd->arg,cmd->argc);
  if ((cmd->kwc==5)&&!memcmp(cmd->kw,"delay",5)) return pcmprintc_output_delay(ppc,cmd->lineno,cmd->arg,cmd->argc);
  if ((cmd->kwc==8)&&!memcmp(cmd->kw,"bandpass",8)) return pcmprintc_output_filter(ppc,cmd->lineno,0x0e,2,cmd->arg,cmd->argc);
  if ((cmd->kwc==5)&&!memcmp(cmd->kw,"notch",5)) return pcmprintc_output_filter(ppc,cmd->lineno,0x0f,2,cmd->arg,cmd->argc);
  if ((cmd->kwc==6)&&!memcmp(cmd->kw,"lopass",6)) return pcmprintc_output_filter(ppc,cmd->lineno,0x10,1,cmd->arg,cmd->argc);
  if ((cmd->kwc==6)&&!memcmp(cmd->kw,"hipass",6)) return pcmprintc_output_filter(ppc,cmd->lineno,0x11,1,cmd->arg,cmd->argc);
  fprintf(stderr,"%s:%d: Unknown command '%.*s'. See etc/doc/pcmprint.md.\n",ppc->refname,cmd->lineno,cmd->kwc,cmd->kw);
  return -2;
}

/* Finish pending voice if there is one.
 */
 
static int pcmprintc_finish_voice(struct pcmprintc_output *output,struct pcmprintc *ppc) {
  if (ppc->commandc) {
    struct pcmprintc_command *cmd;
    int i,err;
    ppc->discard_voice=0;
    int dstc0=ppc->dst.c;
    
    // "wave" is required and must go out first.
    int got_wave=0;
    for (cmd=ppc->commandv,i=ppc->commandc;i-->0;cmd++) {
      if ((cmd->kwc==4)&&!memcmp(cmd->kw,"wave",4)) {
        if (got_wave) {
          fprintf(stderr,"%s:%d: Multiple 'wave' commands in voice.\n",ppc->refname,cmd->lineno);
          return -2;
        }
        got_wave=1;
        if ((err=pcmprintc_output_wave(ppc,cmd->lineno,cmd->arg,cmd->argc))<0) return err;
        cmd->kwc=0;
      }
    }
    if (!got_wave) {
      fprintf(stderr,"%s:%d: Voice requires exactly one 'wave' command.\n",ppc->refname,ppc->commandv[0].lineno);
      return -2;
    }
    
    // A few other commands should only appear once, and we'll front-load them.
    // We ought to check for duplicates on these too but meh.
    for (cmd=ppc->commandv,i=ppc->commandc;i-->0;cmd++) {
      if (
        ((cmd->kwc==4)&&!memcmp(cmd->kw,"rate",4))||
        ((cmd->kwc==7)&&!memcmp(cmd->kw,"ratelfo",7))||
        ((cmd->kwc==7)&&!memcmp(cmd->kw,"fmrange",7))||
        ((cmd->kwc==5)&&!memcmp(cmd->kw,"fmlfo",5))
      ) {
        if ((err=pcmprintc_output_command(ppc,cmd))<0) return err;
        cmd->kwc=0;
      }
    }
    
    // Every other command is part of a pipeline. Emit them all in order.
    int level_length=0;
    for (cmd=ppc->commandv,i=ppc->commandc;i-->0;cmd++) {
      if (cmd->kwc) {
        ppc->recent_level_length=0;
        if ((err=pcmprintc_output_command(ppc,cmd))<0) return err;
        if (ppc->recent_level_length>level_length) {
          level_length=ppc->recent_level_length;
        }
      }
    }
    if (!level_length) {
      fprintf(stderr,"%s:%d: Voice is missing required 'level' command.\n",ppc->refname,ppc->commandv[0].lineno);
      return -2;
    }
    if (level_length>ppc->duration) {
      ppc->duration=level_length;
    }
    
    if (sr_encode_u8(&ppc->dst,0x01)<0) return -1; // VOICE
    
    if (ppc->discard_voice) {
      ppc->dst.c=dstc0;
    }
  }
  ppc->commandc=0;
  ppc->discard_voice=0;
  return 0;
}

/* Finalize any processing for one sound.
 */
 
static int pcmprintc_finish(struct pcmprintc_output *output,struct pcmprintc *ppc) {
  int err=pcmprintc_finish_voice(output,ppc);
  if (err<0) return err;
  if ((ppc->dst.c>=3)&&(((uint8_t*)ppc->dst.v)[ppc->dst.c-1]==0x01)) ppc->dst.c; // drop trailing VOICE if present
  if (ppc->dst.c<2) {
    fprintf(stderr,"%s:%d: Invalid empty sound.\n",ppc->refname,ppc->lineno);
    return -2;
  }
  ((uint8_t*)ppc->dst.v)[0]=ppc->duration>>8;
  ((uint8_t*)ppc->dst.v)[1]=ppc->duration;
  output->bin=ppc->dst.v;
  output->binc=ppc->dst.c;
  return 1;
}

/* Reset compiler and output state for maybe another sound.
 */
 
static void pcmprintc_reset(struct pcmprintc_output *output,struct pcmprintc *ppc) {
  memset(output,0,sizeof(struct pcmprintc_output));
  output->refname=ppc->refname;
  output->lineno=ppc->lineno;
  ppc->dst.c=0;
  ppc->master=1.0f;
  ppc->commandc=0;
  ppc->duration=0;
}

/* Next.
 */

int pcmprintc_next(struct pcmprintc_output *output,struct pcmprintc *ppc) {
  pcmprintc_reset(output,ppc);
  const char *line;
  int linec;
  while ((linec=sr_decode_line(&line,&ppc->src))>0) {
    ppc->lineno++;
    int i=0; for (;i<linec;i++) if (line[i]=='#') linec=i;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec) continue;
    
    /* Split keyword and args. Everything we take in will have a meaningful keyword.
     */
    const char *kw=line;
    int kwc=0;
    while ((kwc<linec)&&((unsigned char)kw[kwc]>0x20)) kwc++;
    const char *arg=kw+kwc;
    int argc=linec-kwc;
    while (argc&&((unsigned char)arg[0]<=0x20)) { arg++; argc--; }
    
    /* If we don't have an ID or content yet, allow a "sound ID" line.
     */
    if (!ppc->dst.c&&!output->idc&&(kwc==5)&&!memcmp(kw,"sound",5)) {
      if (!argc) {
        fprintf(stderr,"%s:%d: ID or name required after 'sound'\n",ppc->refname,ppc->lineno);
        return -2;
      }
      output->lineno=ppc->lineno;
      output->id=arg;
      output->idc=argc;
      if (sr_int_eval(&output->idn,arg,argc)<2) {
        output->idn=0;
      } else if ((output->idn<1)||(output->idn>0xffff)) {
        fprintf(stderr,"%s:%d: Sound ID must be in 0..65535, found '%.*s'\n",ppc->refname,ppc->lineno,argc,arg);
        return -2;
      }
      continue;
    }
    
    /* If we have an ID, "end" closes the block.
     * Note that a sound consisting only of "sound ID" and "end" is illegal.
     */
    if (output->idc&&(kwc==3)&&!memcmp(kw,"end",3)) {
      if (argc) {
        fprintf(stderr,"%s:%d: Unexpected tokens after 'end'\n",ppc->refname,ppc->lineno);
        return -2;
      }
      return pcmprintc_finish(output,ppc);
    }
    
    /* OK we have content.
     * Emit a placeholder duration, if we haven't yet.
     */
    if (!ppc->dst.c) {
      if (sr_encode_raw(&ppc->dst,"\0\0",2)<0) return -1;
    }
    
    /* "master": Hold value globally. It will get baked into level envelopes as we finish voices.
     */
    if ((kwc==6)&&!memcmp(kw,"master",6)) {
      double v;
      if (sr_double_eval(&v,arg,argc)<0) {
        fprintf(stderr,"%s:%d: Expected float after 'master', found '%.*s'\n",ppc->refname,ppc->lineno,argc,arg);
        return -2;
      }
      ppc->master=v;
      continue;
    }
    
    /* "voice": Finish current voice and clear state.
     */
    if ((kwc==5)&&!memcmp(kw,"voice",5)) {
      if (pcmprintc_finish_voice(output,ppc)<0) return -1;
      continue;
    }
    
    /* Everything else gets added to (commandv) for processing at end of voice.
     */
    if (ppc->commandc>=ppc->commanda) {
      int na=ppc->commanda+16;
      if (na>INT_MAX/sizeof(struct pcmprintc_command)) return -1;
      void *nv=realloc(ppc->commandv,sizeof(struct pcmprintc_command)*na);
      if (!nv) return -1;
      ppc->commandv=nv;
      ppc->commanda=na;
    }
    struct pcmprintc_command *command=ppc->commandv+ppc->commandc++;
    command->kw=kw;
    command->kwc=kwc;
    command->arg=arg;
    command->argc=argc;
    command->lineno=ppc->lineno;
  }
  if (!ppc->dst.c) return 0; // Must be EOF.
  return pcmprintc_finish(output,ppc);
}

/* Next, asserting exactly one resource.
 */

int pcmprintc_next_final(struct pcmprintc_output *output,struct pcmprintc *ppc) {
  int err=pcmprintc_next(output,ppc);
  if (err<0) return err;
  if (!err) return -1;
  const char *line;
  int linec;
  while ((linec=sr_decode_line(&line,&ppc->src))>0) {
    ppc->lineno++;
    int i=0; for (;i<linec;i++) if (line[i]=='#') linec=i;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec) continue;
    fprintf(stderr,"%s:%d: Unexpected tokens after pcmprint block.\n",ppc->refname,ppc->lineno);
    return -2;
  }
  return 1;
}
