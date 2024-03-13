#include "strfmt.h"
#include "opt/serial/serial.h"
#include <string.h>

#define STRFMT_STATE_OUTER 0
#define STRFMT_STATE_NEED_LEN 1
#define STRFMT_STATE_GOT_LEN 2
#define STRFMT_STATE_NEED_PREC 3
#define STRFMT_STATE_GOT_PREC 4
#define STRFMT_STATE_NEED_V 5
#define STRFMT_STATE_GOT_V 6

/* Test completion.
 */

int strfmt_is_finished(const struct strfmt *strfmt) {
  if (!strfmt->fmt) return 1;
  if (!*(strfmt->fmt)) return 1;
  return 0;
}

/* Produce text.
 */

int strfmt_more(char *dst,int dsta,struct strfmt *strfmt) {
  if (!dst||(dsta<1)) return 0;
  
  // Terminated.
  if (!strfmt->fmt||!*(strfmt->fmt)) {
    dst[0]=0;
    return 0;
  }
  
  // Literal text.
  int literalc=0;
  while ((literalc<dsta)&&strfmt->fmt[literalc]&&(strfmt->fmt[literalc]!='%')) literalc++;
  if (literalc) {
    memcpy(dst,strfmt->fmt,literalc);
    if (literalc<dsta) dst[literalc]=0;
    strfmt->fmt+=literalc;
    return literalc;
  }
  
  // Escaped percent.
  if (strfmt->fmt[1]=='%') {
    dst[0]='%';
    if (dsta>1) dst[1]=0;
    strfmt->fmt+=2;
    return 1;
  }
  
  // Waiting for something from the user.
  if (strfmt->awaiting) {
    if (strfmt->state==STRFMT_STATE_NEED_LEN) return 0;
    if (strfmt->state==STRFMT_STATE_NEED_PREC) return 0;
    if (strfmt->state==STRFMT_STATE_NEED_V) return 0;
  }
  
  // Stopped at a format unit but we haven't examined it yet.
  if (strfmt->state==STRFMT_STATE_OUTER) {
    strfmt->awaiting=0;
    strfmt->align=0;
    strfmt->len=0;
    strfmt->prec=0;
    strfmt->manprec=0;
    const char *src=strfmt->fmt;
    int srcp=1;
    if (src[srcp]=='-') { strfmt->align=-1; srcp++; }
    if (src[srcp]=='*') {
      strfmt->state=STRFMT_STATE_NEED_LEN;
      strfmt->awaiting='i';
      return 0;
    }
    while ((src[srcp]>='0')&&(src[srcp]<='9')) {
      strfmt->len*=10;
      strfmt->len+=src[srcp++]-'0';
    }
    if (src[srcp]=='.') {
      strfmt->manprec=1;
      srcp++;
      if (src[srcp]=='*') {
        strfmt->state=STRFMT_STATE_NEED_PREC;
        strfmt->awaiting='i';
        return 0;
      }
      while ((src[srcp]>='0')&&(src[srcp]<='9')) {
        strfmt->prec*=10;
        strfmt->prec+=src[srcp++]-'0';
      }
    }
    if (!src[srcp]) { strfmt->fmt++; return 0; }
    switch (src[srcp]) {
      case 'd': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'x': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'u': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'c': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'l': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='l'; break;
      case 'p': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='p'; break;
      case 'f': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='f'; break;
      case 's': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='p'; break;
      default: { strfmt->fmt++; return 0; }
    }
    return 0;
  }
  
  // Got length. Check whether we need to ask for precision.
  if (strfmt->state==STRFMT_STATE_GOT_LEN) {
    const char *src=strfmt->fmt;
    int srcp=1;
    if (src[srcp]=='-') srcp++;
    if (src[srcp]=='*') srcp++;
    else while ((src[srcp]>='0')&&(src[srcp]<='9')) srcp++;
    if (src[srcp]=='.') {
      strfmt->manprec=1;
      srcp++;
      if (src[srcp]=='*') {
        strfmt->state=STRFMT_STATE_NEED_PREC;
        strfmt->awaiting='i';
        return 0;
      }
      while ((src[srcp]>='0')&&(src[srcp]<='9')) {
        strfmt->prec*=10;
        strfmt->prec+=src[srcp++]-'0';
      }
    }
    if (!src[srcp]) { strfmt->fmt++; strfmt->state=STRFMT_STATE_OUTER; return 0; }
    switch (src[srcp]) {
      case 'd': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'x': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'u': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'c': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'l': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='l'; break;
      case 'p': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='p'; break;
      case 'f': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='f'; break;
      case 's': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='p'; break;
      default: { strfmt->fmt++; strfmt->state=STRFMT_STATE_OUTER; return 0; }
    }
    return 0;
  }
  
  // Got precision. Read up to the specifier.
  if (strfmt->state==STRFMT_STATE_GOT_PREC) {
    const char *src=strfmt->fmt;
    int srcp=1;
    while (src[srcp]&&((src[srcp]<'a')||(src[srcp]>'z'))) srcp++;
    if (!src[srcp]) { strfmt->fmt++; strfmt->state=STRFMT_STATE_OUTER; return 0; }
    switch (src[srcp]) {
      case 'd': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'x': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'u': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'c': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='i'; break;
      case 'l': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='l'; break;
      case 'p': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='p'; break;
      case 'f': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='f'; break;
      case 's': strfmt->state=STRFMT_STATE_NEED_V; strfmt->awaiting='p'; break;
      default: { strfmt->fmt++; strfmt->state=STRFMT_STATE_OUTER; return 0; }
    }
    return 0;
  }
  
  // Got value, output it.
  if (strfmt->state==STRFMT_STATE_GOT_V) {
    strfmt->state=STRFMT_STATE_OUTER;
    strfmt->awaiting=0;
    const char *src=strfmt->fmt;
    int srcp=1;
    while (src[srcp]&&((src[srcp]<'a')||(src[srcp]>'z'))) srcp++;
    if (!src[srcp]) { strfmt->fmt++; strfmt->state=STRFMT_STATE_OUTER; return 0; }
    strfmt->fmt+=srcp+1;
    switch (src[srcp]) {
      // TODO len,align
      case 'd': return sr_decsint_repr(dst,dsta,strfmt->v.i);
      case 'x': return sr_hexuint_repr(dst,dsta,strfmt->v.i,0,0);
      case 'u': return sr_decuint_repr(dst,dsta,strfmt->v.i,0);
      case 'c': {
          dst[0]=strfmt->v.i;
          if ((dst[0]<0x20)||(dst[0]>0x7e)) dst[0]='?';
          if (dsta>1) dst[1]=0;
          return 1;
        }
      case 'l': return sr_decsint64_repr(dst,dsta,strfmt->v.l);
      case 'p': {
          int dstc=2+(sizeof(void*)<<1);
          if (dstc>dsta) { strfmt->fmt++; return 0; }
          uintptr_t v=(uintptr_t)strfmt->v.p;
          int i=dstc; while (i-->2) {
            dst[i]="0123456789abcdef"[v&15];
            v>>=4;
          }
          dst[0]='0';
          dst[1]='x';
          if (dstc<dsta) dst[dstc]=0;
          return dstc;
        }
      case 'f': return sr_double_repr(dst,dsta,strfmt->v.f);
      case 's': {
          const char *str=strfmt->v.p;
          int strc;
          if (!str) strc=0;
          else if (strfmt->manprec) {
            strc=strfmt->prec;
            if (strc<0) strc=0;
          } else {
            strc=0;
            while (str[strc]) strc++;
          }
          if (strc>dsta) strc=dsta;
          memcpy(dst,str,strc);
          if (strc<dsta) dst[strc]=0;
          return strc;
        }
      default: { strfmt->fmt++; return 0; }
    }
  }
  
  // oops?
  strfmt->fmt="";
  return 0;
}

/* Receive parameters.
 */

void strfmt_provide_i(struct strfmt *strfmt,int v) {
  // 'i' is unlike the others; it might be asking for any of len, prec, v.
  if (strfmt->awaiting!='i') return;
  switch (strfmt->state) {
    case STRFMT_STATE_NEED_LEN: strfmt->len=v; strfmt->state=STRFMT_STATE_GOT_LEN; break;
    case STRFMT_STATE_NEED_PREC: strfmt->prec=v; strfmt->state=STRFMT_STATE_GOT_PREC; break;
    case STRFMT_STATE_NEED_V: strfmt->v.i=v; strfmt->state=STRFMT_STATE_GOT_V; break;
    default: return;
  }
  strfmt->awaiting=0;
}

void strfmt_provide_l(struct strfmt *strfmt,int64_t v) {
  if (strfmt->awaiting!='l') return;
  if (strfmt->state!=STRFMT_STATE_NEED_V) return;
  strfmt->v.l=v;
  strfmt->state=STRFMT_STATE_GOT_V;
  strfmt->awaiting=0;
}

void strfmt_provide_f(struct strfmt *strfmt,double v) {
  if (strfmt->awaiting!='f') return;
  if (strfmt->state!=STRFMT_STATE_NEED_V) return;
  strfmt->v.f=v;
  strfmt->state=STRFMT_STATE_GOT_V;
  strfmt->awaiting=0;
}

void strfmt_provide_p(struct strfmt *strfmt,const void *p) {
  if (strfmt->awaiting!='p') return;
  if (strfmt->state!=STRFMT_STATE_NEED_V) return;
  strfmt->v.p=p;
  strfmt->state=STRFMT_STATE_GOT_V;
  strfmt->awaiting=0;
}
