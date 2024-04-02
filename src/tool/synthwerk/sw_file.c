#include "synthwerk_internal.h"

/* Config lines should all be like ".key=VVVVVVVVV,".
 */

struct sw_kv {
  const char *k,*v;
  int kc,vc;
};
static struct sw_kv sw_file_read_line(const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return (struct sw_kv){0};
  if (src[srcp++]!='.') return (struct sw_kv){0};
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  struct sw_kv result={0};
  result.k=src+srcp;
  while ((srcp<srcc)&&(src[srcp++]!='=')) result.kc++;
  while (result.kc&&((unsigned char)result.k[result.kc-1]<=0x20)) result.kc--;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  result.v=src+srcp;
  result.vc=srcc-srcp;
  while (result.vc&&(((unsigned char)result.v[result.vc-1]<=0x20)||(result.v[result.vc-1]==','))) result.vc--;
  return result;
}

/* Evaluate wave coefficients.
 */
 
static int sw_eval_coefv(uint8_t *dst/*8*/,const char *src,int srcc) {
  int dstp=0,srcp=0;
  while (srcp<srcc) {
    if (((unsigned char)src[srcp]<=0x20)||(src[srcp]==',')||(src[srcp]=='{')||(src[srcp]=='}')) {
      srcp++;
      continue;
    }
    if (dstp>=8) {
      fprintf(stderr,"Too many wave coefficients, limit 8.\n");
      return -1;
    }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!=',')&&(src[srcp]!='{')&&(src[srcp]!='}')) {
      srcp++;
      tokenc++;
    }
    int v;
    if ((sr_int_eval(&v,token,tokenc)<2)||(v<0)||(v>0xff)) {
      fprintf(stderr,"Invalid wave coefficient '%.*s', must be integer in 0..255.\n",tokenc,token);
      return -1;
    }
    dst[dstp++]=v;
  }
  return 0;
}

/* Evaluate tiny envelope.
 */
 
static int sw_eval_tinyenv(uint8_t *dst/*1*/,const char *src,int srcc) {
  int srcp=0,shift=-1;
  while (srcp<srcc) {
    if (((unsigned char)src[srcp]<=0x20)||(src[srcp]=='|')||(src[srcp]=='(')||(src[srcp]==')')) {
      srcp++;
      continue;
    }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='|')&&(src[srcp]!='(')&&(src[srcp]!=')')) {
      srcp++;
      tokenc++;
    }
    if (shift>=0) { // Awaiting ATTACK or RELEASE value.
      int v;
      if ((sr_int_eval(&v,token,tokenc)<2)||(v<0)||(v>7)) {
        fprintf(stderr,"Invalid %s value '%.*s', must be integer in 0..7.\n",(shift==3)?"attack":"release",tokenc,token);
        return -1;
      }
      (*dst)|=v<<shift;
      shift=-1;
    } else { // Primary position. Shape name or "ATTACK" or "RELEASE".
      if ((tokenc==7)&&!memcmp(token,"IMPULSE",7)) (*dst)|=0x00;
      else if ((tokenc==5)&&!memcmp(token,"PLUCK",5)) (*dst)|=0x40;
      else if ((tokenc==4)&&!memcmp(token,"TONE",4)) (*dst)|=0x80;
      else if ((tokenc==3)&&!memcmp(token,"BOW",3)) (*dst)|=0xc0;
      else if ((tokenc==6)&&!memcmp(token,"ATTACK",6)) shift=3;
      else if ((tokenc==7)&&!memcmp(token,"RELEASE",7)) shift=0;
      else {
        fprintf(stderr,"Unexpected token '%.*s' in tiny level env. Expected 'IMPULSE', 'PLUCK', 'TONE', 'BOW', 'ATTACK', or 'RELEASE'\n",tokenc,token);
        return -1;
      }
    }
  }
  return 0;
}

/* Evaluate plain integers.
 */
 
static int sw_eval_scalar8(uint8_t *dst,const char *src,int srcc) {
  int v;
  if ((sr_int_eval(&v,src,srcc)<2)||(v<0)||(v>0xff)) {
    fprintf(stderr,"Expected integer in 0..255, found '%.*s'\n",srcc,src);
    return -1;
  }
  *dst=v;
  return 0;
}
 
static int sw_eval_scalar16(uint16_t *dst,const char *src,int srcc) {
  int v;
  if ((sr_int_eval(&v,src,srcc)<2)||(v<0)||(v>0xffff)) {
    fprintf(stderr,"Expected integer in 0..65535, found '%.*s'\n",srcc,src);
    return -1;
  }
  *dst=v;
  return 0;
}

/* Receive config line, with mode known.
 */
 
static int sw_file_cmd_blip(const char *src,int srcc,const char *path,int lineno) {
  fprintf(stderr,"%s:%d: BLIP instruments take no config.\n",path,lineno);
  return -2;
}

static int sw_file_cmd_wave(const char *src,int srcc,const char *path,int lineno) {
  struct sw_kv kv=sw_file_read_line(src,srcc);
  if (!kv.k) return -1;
  if ((kv.kc==4)&&!memcmp(kv.k,"wave",4)) return sw_eval_coefv(sw.ins.wave.wave,kv.v,kv.vc);
  if ((kv.kc==5)&&!memcmp(kv.k,"level",5)) return sw_eval_tinyenv(&sw.ins.wave.level,kv.v,kv.vc);
  fprintf(stderr,"%s:%d: Unexpected key '%.*s' for WAVE instrument.\n",path,lineno,kv.kc,kv.k);
  return -2;
}

static int sw_file_cmd_rock(const char *src,int srcc,const char *path,int lineno) {
  struct sw_kv kv=sw_file_read_line(src,srcc);
  if (!kv.k) return -1;
  if ((kv.kc==4)&&!memcmp(kv.k,"wave",4)) return sw_eval_coefv(sw.ins.rock.wave,kv.v,kv.vc);
  if ((kv.kc==5)&&!memcmp(kv.k,"level",5)) return sw_eval_tinyenv(&sw.ins.rock.level,kv.v,kv.vc);
  if ((kv.kc==3)&&!memcmp(kv.k,"mix",3)) return sw_eval_scalar16(&sw.ins.rock.mix,kv.v,kv.vc);
  fprintf(stderr,"%s:%d: Unexpected key '%.*s' for ROCK instrument.\n",path,lineno,kv.kc,kv.k);
  return -2;
}

static int sw_file_cmd_fmrel(const char *src,int srcc,const char *path,int lineno) {
  struct sw_kv kv=sw_file_read_line(src,srcc);
  if (!kv.k) return -1;
  if ((kv.kc==4)&&!memcmp(kv.k,"rate",4)) return sw_eval_scalar8(&sw.ins.fmrel.rate,kv.v,kv.vc);
  if ((kv.kc==5)&&!memcmp(kv.k,"scale",5)) return sw_eval_scalar8(&sw.ins.fmrel.scale,kv.v,kv.vc);
  if ((kv.kc==5)&&!memcmp(kv.k,"range",5)) return sw_eval_scalar16(&sw.ins.fmrel.range,kv.v,kv.vc);
  if ((kv.kc==5)&&!memcmp(kv.k,"level",5)) return sw_eval_tinyenv(&sw.ins.fmrel.level,kv.v,kv.vc);
  fprintf(stderr,"%s:%d: Unexpected key '%.*s' for FMREL instrument.\n",path,lineno,kv.kc,kv.k);
  return -2;
}

static int sw_file_cmd_fmabs(const char *src,int srcc,const char *path,int lineno) {
  struct sw_kv kv=sw_file_read_line(src,srcc);
  if (!kv.k) return -1;
  if ((kv.kc==4)&&!memcmp(kv.k,"rate",4)) return sw_eval_scalar16(&sw.ins.fmabs.rate,kv.v,kv.vc);
  if ((kv.kc==5)&&!memcmp(kv.k,"scale",5)) return sw_eval_scalar8(&sw.ins.fmabs.scale,kv.v,kv.vc);
  if ((kv.kc==5)&&!memcmp(kv.k,"range",5)) return sw_eval_scalar16(&sw.ins.fmabs.range,kv.v,kv.vc);
  if ((kv.kc==5)&&!memcmp(kv.k,"level",5)) return sw_eval_tinyenv(&sw.ins.fmabs.level,kv.v,kv.vc);
  fprintf(stderr,"%s:%d: Unexpected key '%.*s' for FMABS instrument.\n",path,lineno,kv.kc,kv.k);
  return -2;
}

static int sw_file_cmd_sub(const char *src,int srcc,const char *path,int lineno) {
  struct sw_kv kv=sw_file_read_line(src,srcc);
  if (!kv.k) return -1;
  if ((kv.kc==6)&&!memcmp(kv.k,"width1",6)) return sw_eval_scalar16(&sw.ins.sub.width1,kv.v,kv.vc);
  if ((kv.kc==6)&&!memcmp(kv.k,"width2",6)) return sw_eval_scalar16(&sw.ins.sub.width2,kv.v,kv.vc);
  if ((kv.kc==4)&&!memcmp(kv.k,"gain",4)) return sw_eval_scalar8(&sw.ins.sub.gain,kv.v,kv.vc);
  if ((kv.kc==5)&&!memcmp(kv.k,"level",5)) return sw_eval_tinyenv(&sw.ins.sub.level,kv.v,kv.vc);
  fprintf(stderr,"%s:%d: Unexpected key '%.*s' for SUB instrument.\n",path,lineno,kv.kc,kv.k);
  return -2;
}

static int sw_file_cmd_fx(const char *src,int srcc,const char *path,int lineno) {
  struct sw_kv kv=sw_file_read_line(src,srcc);
  if (!kv.k) return -1;
  if ((kv.kc==8)&&!memcmp(kv.k,"rangeenv",8)) return sw_eval_scalar16(&sw.ins.fx.rangeenv,kv.v,kv.vc);
  if ((kv.kc==8)&&!memcmp(kv.k,"rangelfo",8)) return sw_eval_scalar8(&sw.ins.fx.rangelfo,kv.v,kv.vc);
  if ((kv.kc==14)&&!memcmp(kv.k,"rangelfo_depth",14)) return sw_eval_scalar8(&sw.ins.fx.rangelfo_depth,kv.v,kv.vc);
  if ((kv.kc==4)&&!memcmp(kv.k,"rate",4)) return sw_eval_scalar8(&sw.ins.fx.rate,kv.v,kv.vc);
  if ((kv.kc==5)&&!memcmp(kv.k,"scale",5)) return sw_eval_scalar8(&sw.ins.fx.scale,kv.v,kv.vc);
  if ((kv.kc==5)&&!memcmp(kv.k,"level",5)) return sw_eval_tinyenv(&sw.ins.fx.level,kv.v,kv.vc);
  if ((kv.kc==11)&&!memcmp(kv.k,"detune_rate",11)) return sw_eval_scalar8(&sw.ins.fx.detune_rate,kv.v,kv.vc);
  if ((kv.kc==12)&&!memcmp(kv.k,"detune_depth",12)) return sw_eval_scalar8(&sw.ins.fx.detune_depth,kv.v,kv.vc);
  if ((kv.kc==9)&&!memcmp(kv.k,"overdrive",9)) return sw_eval_scalar8(&sw.ins.fx.overdrive,kv.v,kv.vc);
  if ((kv.kc==10)&&!memcmp(kv.k,"delay_rate",10)) return sw_eval_scalar8(&sw.ins.fx.delay_rate,kv.v,kv.vc);
  if ((kv.kc==11)&&!memcmp(kv.k,"delay_depth",11)) return sw_eval_scalar8(&sw.ins.fx.delay_depth,kv.v,kv.vc);
  fprintf(stderr,"%s:%d: Unexpected key '%.*s' for FX instrument.\n",path,lineno,kv.kc,kv.k);
  return -2;
}

/* Load the file, main entry point.
 */
 
int synthwerk_load_file() {
  void *serial=0;
  int serialc=file_read(&serial,SW_INSTRUMENT_FILE);
  if (serialc<0) return -1;
  memset(&sw.ins,0,sizeof(struct synth_builtin));
  struct sr_decoder decoder={.v=serial,.c=serialc};
  const char *line;
  int lineno=0,linec;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    int i=0; for (;i<linec;i++) {
      if ((line[i]=='/')&&(line[i+1]=='/')) linec=i;
      else if (line[i]=='#') linec=i;
    }
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec) continue;
    if ((linec==3)&&!memcmp(line,"EOF",3)) break;
    //fprintf(stderr,"%s:%d: %.*s\n",SW_INSTRUMENT_FILE,lineno,linec,line);
    
    // Ignore a line containing only a close paren.
    if ((linec==1)&&(line[0]==')')) continue;
    
    // First line should be eg "ROCK(0x12,", naming the mode (and the pid, which we don't care about).
    if (!sw.ins.mode) {
      #define _(tag) if ((linec>=sizeof(#tag)-1)&&!memcmp(line,#tag,sizeof(#tag)-1)) { \
        sw.ins.mode=SYNTH_CHANNEL_MODE_##tag; \
        continue; \
      }
      _(BLIP)
      _(WAVE)
      _(ROCK)
      _(FMREL)
      _(FMABS)
      _(SUB)
      _(FX)
      // Don't allow DRUM or ALIAS
      #undef _
      fprintf(stderr,"%s:%d: Expected instrument mode introducer.\n",SW_INSTRUMENT_FILE,lineno);
      free(serial);
      return -1;
    }
    
    // Furher lines depend on mode.
    int err=0;
    switch (sw.ins.mode) {
      case SYNTH_CHANNEL_MODE_BLIP: err=sw_file_cmd_blip(line,linec,SW_INSTRUMENT_FILE,lineno); break;
      case SYNTH_CHANNEL_MODE_WAVE: err=sw_file_cmd_wave(line,linec,SW_INSTRUMENT_FILE,lineno); break;
      case SYNTH_CHANNEL_MODE_ROCK: err=sw_file_cmd_rock(line,linec,SW_INSTRUMENT_FILE,lineno); break;
      case SYNTH_CHANNEL_MODE_FMREL: err=sw_file_cmd_fmrel(line,linec,SW_INSTRUMENT_FILE,lineno); break;
      case SYNTH_CHANNEL_MODE_FMABS: err=sw_file_cmd_fmabs(line,linec,SW_INSTRUMENT_FILE,lineno); break;
      case SYNTH_CHANNEL_MODE_SUB: err=sw_file_cmd_sub(line,linec,SW_INSTRUMENT_FILE,lineno); break;
      case SYNTH_CHANNEL_MODE_FX: err=sw_file_cmd_fx(line,linec,SW_INSTRUMENT_FILE,lineno); break;
    }
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error processing input line.\n",SW_INSTRUMENT_FILE,lineno);
      free(serial);
      return -1;
    }
  }
  free(serial);
  
  /* Send it to synth.
   */
  if (!sw.audio->type->lock||(sw.audio->type->lock(sw.audio)>=0)) {
    synth_override_pid_0(sw.synth,&sw.ins);
    if (sw.audio->type->unlock) sw.audio->type->unlock(sw.audio);
  }
  return 0;
}
