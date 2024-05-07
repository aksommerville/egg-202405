#include "webtm_internal.h"

/* Compose template, main entry point.
 */
 
int webtm_compose_template(struct sr_encoder *dst,const char *html,int htmlc,const char *js,int jsc) {
  struct sr_decoder decoder={.v=html,.c=htmlc};
  const char *line;
  int lineno=0,linec;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec) continue;
    
    if ((linec==18)&&!memcmp(line,"INSERT PLATFORM JS",18)) {
      if (sr_encode_raw(dst,js,jsc)<0) return -1;
      // (js) should end with a newline, don't emit another.
      continue;
    }
    
    if (sr_encode_raw(dst,line,linec)<0) return -1;
    if (sr_encode_u8(dst,0x0a)<0) return -1;
  }
  return 0;
}

/* Encode a potentially huge binary input as rows of base64, of tasteful length.
 */
 
static const char webtm_base64_alphabet[64]=
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789"
  "+/"
;
 
static void webtm_base64_encode_1(char *dst,const uint8_t *src) {
  dst[0]=webtm_base64_alphabet[src[0]>>2];
  dst[1]=webtm_base64_alphabet[((src[0]<<4)|(src[1]>>4))&0x3f];
  dst[2]=webtm_base64_alphabet[((src[1]<<2)|(src[2]>>6))&0x3f];
  dst[3]=webtm_base64_alphabet[src[2]&0x3f];
}
 
static int webtm_encode_base64(struct sr_encoder *dst,const uint8_t *src,int srcc) {
  int partialsrcc=(srcc/3)*3;
  int srcp=0,linelen=0;
  for (;srcp<partialsrcc;srcp+=3) {
    char text[4];
    webtm_base64_encode_1(text,src+srcp);
    if (sr_encode_raw(dst,text,4)<0) return -1;
    linelen+=4;
    if (linelen>=120) {
      if (sr_encode_u8(dst,0x0a)<0) return -1;
      linelen=0;
    }
  }
  const uint8_t *tail=src+partialsrcc;
  int tailc=srcc-partialsrcc;
  if (tailc) {
    uint8_t tmp[3]={0};
    memcpy(tmp,tail,tailc);
    char text[4];
    webtm_base64_encode_1(text,tmp);
    switch (tailc) {
      case 1: text[2]=text[3]='='; break;
      case 2: text[3]='='; break;
    }
    if (sr_encode_raw(dst,text,4)<0) return -1;
    linelen+=4;
  }
  if (linelen) {
    if (sr_encode_u8(dst,0x0a)<0) return -1;
  }
  return 0;
}

/* Compose bundle, main entry point.
 */
 
int webtm_compose_bundle(struct sr_encoder *dst,const char *html,int htmlc,const void *rom,int romc) {
  struct sr_decoder decoder={.v=html,.c=htmlc};
  const char *line;
  int lineno=0,linec;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec) continue;
    
    if ((linec==10)&&!memcmp(line,"INSERT ROM",10)) {
      if (webtm_encode_base64(dst,rom,romc)<0) return -1;
      continue;
    }
    
    if (sr_encode_raw(dst,line,linec)<0) return -1;
    if (sr_encode_u8(dst,0x0a)<0) return -1;
  }
  return 0;
}
