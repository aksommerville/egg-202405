#include "sfg.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Compiler context.
 */
 
struct sfg_cm {
  struct sr_encoder *dst; // WEAK
  const char *refname;
  int lineno;
  int status;
  int durp;
  int duration; // ms
  int featuresp; // 0 if no voice
  int positional; // Nonzero if we have emitted at least one positional command for current voice.
};

static void sfg_cm_cleanup(struct sfg_cm *cm) {
}

/* Error reporting.
 */
 
#define FAIL(fmt,...) { \
  if (cm->status<0) return cm->status; \
  if (cm->refname) { \
    fprintf(stderr,"%s:%d: "fmt"\n",cm->refname,cm->lineno,##__VA_ARGS__); \
    return cm->status=-2; \
  } \
  return cm->status=-1; \
}

/* Enumerated command. Single parameter u8.
 * Currently only "shape".
 */
 
static int sfg_cm_cmd_enum_(struct sfg_cm *cm,const char *src,int srcc,int opcode,...) {
  if ((opcode>=0)&&(sr_encode_u8(cm->dst,opcode)<0)) return -1;
  va_list vargs;
  va_start(vargs,opcode);
  int v=0;
  for (;;v++) {
    const char *name=va_arg(vargs,const char*);
    if (!name) FAIL("String '%.*s' not in enum for this field.",srcc,src)
    if (!memcmp(name,src,srcc)&&!name[srcc]) {
      return sr_encode_u8(cm->dst,v);
    }
  }
}
#define sfg_cm_cmd_enum(cm,src,srcc,opcode,...) sfg_cm_cmd_enum_(cm,src,srcc,opcode,##__VA_ARGS__,(void*)0)

/* Float list. Emits u8 count, then floating-point tokens of the given shape.
 * Currently only "harmonics".
 */

static int sfg_cm_cmd_floatlist(struct sfg_cm *cm,const char *src,int srcc,int opcode,int wholesize,int fractsize) {
  int cp=cm->dst->c;
  if (sr_encode_u8(cm->dst,0)<0) return -1;
  double scale=(double)(1<<fractsize);
  int wordsize=(wholesize+fractsize)>>3;
  if ((wordsize<1)||(wordsize>3)) return -1;
  int max=(1<<(wholesize+fractsize))-1;
  int srcp=0,c=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) {
      srcp++;
      continue;
    }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    double v;
    if (sr_double_eval(&v,token,tokenc)<0) FAIL("Expected float, found '%.*s'",tokenc,token)
    int i=(int)(v*scale);
    if (i<0) i=0; else if (i>max) i=max;
    if (sr_encode_intbe(cm->dst,i,wordsize)<0) return -1;
    c++;
  }
  if (c>0xff) FAIL("Too many tokens: %d, limit 255",c)
  ((uint8_t*)cm->dst->v)[cp]=c;
  return 0;
}

/* Fixed-format command.
 * List the parameter types as "uWHOLE.FRACT" eg "u0.8" "u16.0".
 * Lots of commands will use this.
 */

static int sfg_cm_cmd_fixed_(struct sfg_cm *cm,const char *src,int srcc,int opcode,...) {
  if ((opcode>=0)&&(sr_encode_u8(cm->dst,opcode)<0)) return -1;
  va_list vargs;
  va_start(vargs,opcode);
  int srcp=0;
  for (;;) {
  
    const char *desc=va_arg(vargs,const char*);
    if (!desc) return 0;
    int wholesize=0,fractsize=0;
    if (*desc!='u') return -1; // We don't have signed parameters anywhere, and forbidding them keeps it simple.
    desc++;
    while ((*desc>='0')&&(*desc<='9')) {
      wholesize*=10;
      wholesize+=(*desc)-'0';
      desc++;
    }
    if (*desc!='.') return -1;
    desc++;
    while ((*desc>='0')&&(*desc<='9')) {
      fractsize*=10;
      fractsize+=(*desc)-'0';
      desc++;
    }
    if (*desc) return -1;
    
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    double v;
    if (sr_double_eval(&v,token,tokenc)<0) FAIL("Expected float, found '%.*s'",tokenc,token)
    v*=(double)(1<<fractsize);
    int i=(int)v;
    int max=(1<<(wholesize+fractsize))-1;
    if (i<0) i=0; else if (i>max) i=max;
    if (sr_encode_intbe(cm->dst,i,(wholesize+fractsize)>>3)<0) return -1;
  }
}
#define sfg_cm_cmd_fixed(cm,src,srcc,opcode,...) sfg_cm_cmd_fixed_(cm,src,srcc,opcode,##__VA_ARGS__,(void*)0)

/* Envelope commands.
 * Emits one value, then u8 count, then (u16:ms, value) per count.
 * Text input looks just like that too, except (count) is omitted.
 * Values must be 16-bit.
 */
 
static int sfg_cm_cmd_env(struct sfg_cm *cm,const char *src,int srcc,int opcode,int fractsize) {
  if ((opcode>=0)&&(sr_encode_u8(cm->dst,opcode)<0)) return -1;
  if ((fractsize<0)||(fractsize>16)) return -1;
  double scale=(double)(1<<fractsize);
  int limit=1<<(16-fractsize);
  int srcp=0,tokenc;
  const char *token;
  #define RDTOKEN { \
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; \
    token=src+srcp; \
    tokenc=0; \
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++; \
  }
  #define VALUE { \
    RDTOKEN \
    double v; \
    if (sr_double_eval(&v,token,tokenc)<0) FAIL("Expected float in 0..%d, found '%.*s'",limit,tokenc,token) \
    int i=(int)(v*scale); \
    if (i==0x10000) i=0xffff; \
    else if ((i<0)||(i>0xffff)) FAIL("Expected float in 0..%d, found '%.*s'",limit,tokenc,token) \
    if (sr_encode_intbe(cm->dst,i,2)<0) return -1; \
  }
  VALUE
  int cp=cm->dst->c,c=0,duration=0;
  if (sr_encode_u8(cm->dst,0)<0) return -1;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) {
      srcp++;
      continue;
    }
    RDTOKEN
    int ms;
    if ((sr_int_eval(&ms,token,tokenc)<2)||(ms<0)||(ms>0xffff)) {
      FAIL("Expected delay in ms 0..65535, found '%.*s'",tokenc,token)
    }
    if (sr_encode_intbe(cm->dst,ms,2)<0) return -1;
    duration+=ms;
    VALUE
    c++;
  }
  if (c>0xff) FAIL("Too many envelope points (%d, limit 255)",c)
  ((uint8_t*)cm->dst->v)[cp]=c;
  #undef RDTOKEN
  #undef VALUE
  if (opcode==0x01) { // "level" command, grow master duration if warranted.
    if (duration>cm->duration) cm->duration=duration;
  }
  return 0;
}

/* Individual commands.
 * Header commands do not validate or update features byte -- outer layer must do that.
 */

static int sfg_cm_cmd_shape(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_enum(cm,src,srcc,-1,"sine","square","sawup","sawdown","triangle","noise","silence");
}

static int sfg_cm_cmd_harmonics(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_floatlist(cm,src,srcc,-1,0,8);
}

static int sfg_cm_cmd_fm(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_fixed(cm,src,srcc,-1,"u8.8","u8.8");
}

static int sfg_cm_cmd_fmenv(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_env(cm,src,srcc,-1,16);
}

static int sfg_cm_cmd_rate(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_env(cm,src,srcc,-1,0);
}

static int sfg_cm_cmd_ratelfo(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_fixed(cm,src,srcc,-1,"u8.8","u16.0");
}

static int sfg_cm_cmd_level(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_env(cm,src,srcc,0x01,16);
}

static int sfg_cm_cmd_gain(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_fixed(cm,src,srcc,0x02,"u8.8");
}

static int sfg_cm_cmd_clip(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_fixed(cm,src,srcc,0x03,"u0.8");
}

static int sfg_cm_cmd_delay(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_fixed(cm,src,srcc,0x04,"u16.0","u0.8","u0.8","u0.8","u0.8");
}

static int sfg_cm_cmd_bandpass(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_fixed(cm,src,srcc,0x05,"u16.0","u16.0");
}

static int sfg_cm_cmd_notch(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_fixed(cm,src,srcc,0x06,"u16.0","u16.0");
}

static int sfg_cm_cmd_lopass(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_fixed(cm,src,srcc,0x07,"u16.0");
}

static int sfg_cm_cmd_hipass(struct sfg_cm *cm,const char *src,int srcc) {
  return sfg_cm_cmd_fixed(cm,src,srcc,0x08,"u16.0");
}

/* Feature bit for keyword, or zero.
 */
 
static int sfg_cm_eval_feature(const char *kw,int kwc) {
  if ((kwc==5)&&!memcmp(kw,"shape",5)) return 0x01;
  if ((kwc==9)&&!memcmp(kw,"harmonics",9)) return 0x02;
  if ((kwc==2)&&!memcmp(kw,"fm",2)) return 0x04;
  if ((kwc==5)&&!memcmp(kw,"fmenv",5)) return 0x08;
  if ((kwc==4)&&!memcmp(kw,"rate",4)) return 0x10;
  if ((kwc==7)&&!memcmp(kw,"ratelfo",7)) return 0x20;
  return 0;
}

/* Receive line of source.
 */
 
static int sfg_cm_line(struct sfg_cm *cm,const char *src,int srcc) {
  const char *kw=src;
  int kwc=0;
  while ((kwc<srcc)&&((unsigned char)kw[kwc]>0x20)) kwc++;
  const char *arg=kw+kwc;
  int argc=srcc-kwc;
  while (argc&&((unsigned char)arg[0]<=0x20)) { argc--; arg++; }
  
  // "master" may only be the first command; we'll determine that by looking at output length.
  // Technically it could go anywhere, but I think users might confuse themselves by putting it inside a voice block.
  if ((kwc==6)&&!memcmp(kw,"master",6)) {
    if (cm->dst->c>cm->durp+4) FAIL("'master' may only be the first command in a sound")
    double v;
    if ((sr_double_eval(&v,arg,argc)<0)||(v<0.0)||(v>256.0)) {
      FAIL("Expected float in 0..256, found '%.*s'",argc,arg)
    }
    int i=(int)(v*256.0);
    if (i<0) i=0; else if (i>0xffff) i=0xffff;
    uint8_t *dst=((uint8_t*)cm->dst->v)+cm->durp+2;
    dst[0]=i>>8;
    dst[1]=i;
    return 0;
  }
  
  // "endvoice" emits 0x00 if a voice is in progress, and clears voice state.
  if ((kwc==8)&&!memcmp(kw,"endvoice",8)) {
    if (!cm->featuresp) return 0;
    if (sr_encode_u8(cm->dst,0x00)<0) return -1;
    cm->featuresp=0;
    cm->positional=0;
    return 0;
  }
  
  // Any other command requires a voice in progress.
  if (!cm->featuresp) {
    cm->featuresp=cm->dst->c;
    if (sr_encode_u8(cm->dst,0x00)<0) return -1;
  }
  
  // Validate order of commands, and update (positional) and features as needed.
  int fbit=sfg_cm_eval_feature(kw,kwc);
  if (fbit) {
    if (cm->positional) FAIL("Command '%.*s' not allowed here, must come earlier.",kwc,kw)
    uint8_t existing=((uint8_t*)cm->dst->v)[cm->featuresp];
    if (fbit&existing) FAIL("Multiple '%.*s' commands in voice.",kwc,kw)
    if (fbit<existing) FAIL("Command '%.*s' not allowed here, must come earlier.",kwc,kw)
    ((uint8_t*)cm->dst->v)[cm->featuresp]|=fbit;
  } else {
    cm->positional=1;
  }
  
  // Compile individual commands.
  #define _(tag) if ((kwc==sizeof(#tag)-1)&&!memcmp(kw,#tag,kwc)) return sfg_cm_cmd_##tag(cm,arg,argc);
  _(shape)
  _(harmonics)
  _(fm)
  _(fmenv)
  _(rate)
  _(ratelfo)
  _(level)
  _(gain)
  _(clip)
  _(delay)
  _(bandpass)
  _(notch)
  _(lopass)
  _(hipass)
  #undef _
  
  FAIL("Unknown command '%.*s'. See etc/doc/sfg.md",kwc,kw)
}

/* Compile in context.
 */
 
static int sfg_cm_inner(struct sfg_cm *cm,const char *src,int srcc) {

  cm->durp=cm->dst->c; // Duration and master placeholders.
  if (sr_encode_raw(cm->dst,"\0\0\1\0",4)<0) return -1;

  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    cm->lineno++;
    int i=0; for (;i<linec;i++) if (line[i]=='#') linec=i;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec) continue;
    int err=sfg_cm_line(cm,line,linec);
    if (err<0) FAIL("Unspecfied error in sfg text.")
  }
  
  // Fill in duration.
  if (cm->duration>0xffff) FAIL("Duration %d ms exceeds limit (65535)",cm->duration)
  uint8_t *durdst=((uint8_t*)cm->dst->v)+cm->durp;
  durdst[0]=cm->duration>>8;
  durdst[1]=cm->duration;
  
  return 0;
}

/* Compile.
 */
 
int sfg_compile(struct sr_encoder *dst,const char *src,int srcc,const char *refname,int lineno0) {
  struct sfg_cm cm={
    .dst=dst,
    .refname=refname,
    .lineno=lineno0,
  };
  int err=sfg_cm_inner(&cm,src,srcc);
  sfg_cm_cleanup(&cm);
  return err;
}

/* Split multi-sound file.
 */
 
int sfg_split(
  const char *src,int srcc,
  const char *refname,
  int (*cb)(const char *src,int srcc,const char *id,int idc,int idn,const char *refname,int lineno0,void *userdata),
  void *userdata
) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct sr_decoder decoder={.v=src,.c=srcc};
  int lineno=0,linec,err;
  const char *line;
  const char *sub=0; // If nonzero, we are mid-block.
  const char *id=0;
  int idc=0,idn=0,lineno0=0,soundc=0;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    const char *start=line;
    int startc=linec;
    int i=0; for (;i<linec;i++) if (line[i]=='#') linec=i;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec) continue;
    
    // Midblock. Send to caller and flush state when we reach "end".
    if (sub) {
      if ((linec==3)&&!memcmp(line,"end",3)) {
        soundc++;
        int subc=start-sub;
        if ((subc<0)||(subc>srcc)) return -1; // oops?
        if (err=cb(sub,subc,id,idc,idn,refname,lineno0,userdata)) return err;
        sub=0;
      }
      continue;
    }
    
    // Begin sound block at outer scope.
    if ((linec>=6)&&!memcmp(line,"sound",5)&&((unsigned char)line[5]<=0x20)) {
      sub=start+startc;
      lineno0=lineno;
      int p=6;
      while ((p<linec)&&((unsigned char)line[p]<=0x20)) p++;
      id=line+p;
      idc=linec-p;
      if ((sr_int_eval(&idn,id,idc)<2)||(idn<1)||(idn>0xffff)) {
        idn=0;
        int okident=1;
        if (idc<1) {
          okident=0;
        } else if ((id[0]>='0')&&(id[0]<='9')) {
          okident=0;
        } else {
          for (i=idc;i-->0;) {
            if ((id[i]>='a')&&(id[i]<='z')) continue;
            if ((id[i]>='A')&&(id[i]<='Z')) continue;
            if ((id[i]>='0')&&(id[i]<='9')) continue;
            if (id[i]=='_') continue;
            okident=0;
            break;
          }
        }
        if (!okident) {
          if (refname) {
            fprintf(stderr,"%s:%d: Expected integer or C identifier for sound name, found '%.*s'\n",refname,lineno,idc,id);
            return -2;
          }
          return -1;
        }
      }
      continue;
    }
    
    // Something else at outer scope.
    // If we haven't received any fenced blocks yet, send the whole thing to the caller.
    // Otherwise fail.
    if (!soundc) {
      return cb(src,srcc,"",0,0,refname,0,userdata);
    }
    if (refname) {
      fprintf(stderr,"%s:%d: Unexpected line at outer scope. Expected 'sound ID'.\n",refname,lineno);
      return -2;
    }
    return -1;
  }
  if (sub) {
    if (refname) {
      fprintf(stderr,"%s:%d: Unclosed sound block.\n",refname,lineno0);
      return -2;
    }
    return -1;
  }
  return 0;
}
