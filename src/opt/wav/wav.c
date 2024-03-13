#include "wav.h"
#include "opt/serial/serial.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

/* Cleanup.
 */

void wav_file_cleanup(struct wav_file *file) {
  if (file->v&&file->ownv) free(file->v);
}

void wav_file_del(struct wav_file *file) {
  if (!file) return;
  if (file->v&&file->ownv) free(file->v);
  free(file);
}

/* Force ownership.
 */

int wav_file_own(struct wav_file *file) {
  if (!file) return -1;
  if (file->ownv) return 0;
  if (file->samplec<1) return -1;
  if (file->samplesize<8) return -1;
  int bytec=file->samplec*(file->samplesize>>3);
  void *nv=malloc(bytec);
  if (!nv) return -1;
  memcpy(nv,file->v,bytec);
  file->v=nv;
  file->ownv=1;
  return 0;
}

/* Encode.
 */

int wav_file_encode(struct sr_encoder *dst,const struct wav_file *file) {
  if (!dst||!file) return -1;
  
  // (v,samplesize) are mandatory, nothing we can do without them.
  if (!file->v) return -1;
  if ((file->samplesize<1)||(file->samplesize&7)) return -1;
  
  // (chanc,samplec,framec) we can infer if missing.
  int chanc=file->chanc;
  int samplec=file->samplec;
  int framec=file->framec;
  if (chanc<1) {
    if ((samplec>0)&&(framec>0)) {
      if ((chanc=samplec/framec)<1) return -1;
    } else chanc=1;
  }
  if (samplec<1) {
    if (framec<1) return -1;
    samplec=framec/chanc;
  }
  if (framec<1) {
    framec=samplec*chanc;
  }
  
  // (rate) we'll accept anything, it doesn't matter for encoding.
  // (datarate,blockalign) don't even exist in our model, we calculate here.
  int blockalign=(file->samplesize>>3)*file->chanc;
  int datarate=blockalign*file->rate; // we haven't checked for overflow. but this number is meaningless anyway, innit?
  
  int datalen=(file->samplesize>>3)*samplec;
  int flen=4+8+16+8+datalen; // "WAVE" + fmt header + fmt + data header + data
  if (sr_encode_raw(dst,"RIFF",4)<0) return -1;
  if (sr_encode_intle(dst,flen,4)<0) return -1;
  if (sr_encode_raw(dst,"WAVE",4)<0) return -1;
  
  if (sr_encode_raw(dst,"fmt ",4)<0) return -1;
  if (sr_encode_intle(dst,16,4)<0) return -1;
  if (sr_encode_intle(dst,1,2)<0) return -1; // LPCM
  if (sr_encode_intle(dst,chanc,2)<0) return -1;
  if (sr_encode_intle(dst,file->rate,4)<0) return -1;
  if (sr_encode_intle(dst,datarate,4)<0) return -1;
  if (sr_encode_intle(dst,blockalign,2)<0) return -1;
  if (sr_encode_intle(dst,file->samplesize,2)<0) return -1;
  
  if (sr_encode_raw(dst,"data",4)<0) return -1;
  if (sr_encode_intle(dst,datalen,4)<0) return -1;
  if (sr_encode_raw(dst,file->v,datalen)<0) return -1;
  
  return 0;
}

/* "fmt "
 */
 
static int wav_file_decode_fmt(struct wav_file *file,const uint8_t *src,int srcc) {
  if (file->rate) return -1; // Multiple "fmt "
  if (srcc<16) return -1;
  
  int type=src[0]|(src[1]<<8);
  int chanc=src[2]|(src[3]<<8);
  int rate=src[4]|(src[5]<<8)|(src[6]<<16)|(src[7]<<24);
  // data rate and block align, i'm guessing we can ignore?
  int samplesize=src[14]|(src[15]<<8);
  
  if (type!=1) return -1; // Only 1=LPCM is supported.
  if (chanc<1) return -1;
  if (rate<1) return -1;
  if ((samplesize<1)||(samplesize&7)) return -1;
  
  file->chanc=chanc;
  file->rate=rate;
  file->samplesize=samplesize;
  
  return 0;
}

/* "data"
 * We'll borrow the first one, and realistically there should be only one.
 * But if multiple, we will concatenate.
 * During decode, (framec) is unused and (samplec) is the data length in bytes.
 */
 
static int wav_file_decode_data(struct wav_file *file,const uint8_t *src,int srcc) {
  if (!file->v) {
    file->v=(void*)src;
    file->samplec=srcc;
    file->ownv=0;
  } else {
    if (file->samplec>INT_MAX-srcc) return -1;
    int nc=file->samplec+srcc;
    char *nv=malloc(nc);
    if (!nv) return -1;
    memcpy(nv,file->v,file->samplec);
    memcpy(nv+file->samplec,src,srcc);
    if (file->ownv) free(file->v);
    file->v=nv;
    file->ownv=1;
    file->samplec+=srcc;
  }
  return 0;
}

/* Decode in context.
 * Caller skips the RIFF and WAVE signatures, and initializes (file) to zeroes.
 */
 
static int wav_file_decode_inner(struct wav_file *file,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-8) return -1;
    char chunktype=0;
    if (!memcmp(src+srcp,"fmt ",4)) chunktype='f';
    else if (!memcmp(src+srcp,"data",4)) chunktype='d';
    int chunklen=src[srcp+4]|(src[srcp+5]<<8)|(src[srcp+6]<<16)|(src[srcp+7]<<24);
    srcp+=8;
    if ((chunklen<0)||(srcp>srcc-chunklen)) return -1;
    switch (chunktype) {
      case 'f': if (wav_file_decode_fmt(file,src+srcp,chunklen)<0) return -1; break;
      case 'd': if (wav_file_decode_data(file,src+srcp,chunklen)<0) return -1; break;
    }
    srcp+=chunklen;
  }
  if (!file->rate) return -1; // no "fmt "
  if (!file->samplec) return -1; // no "data"
  // Rephrase data length in samples and frames, from bytes, now that everything's settled.
  int ssbytes=file->samplesize>>3;
  if (ssbytes<1) return -1;
  file->samplec=file->samplec/ssbytes;
  file->framec=file->samplec/file->chanc;
  return 0;
}

/* Decode.
 */

struct wav_file *wav_file_decode(const void *src,int srcc) {
  const uint8_t *SRC=src;
  
  // RIFF and WAVE signatures.
  if ((srcc<8)||memcmp(src,"RIFF",8)) return 0;
  int riffsize=SRC[4]|(SRC[5]<<8)|(SRC[6]<<16)|(SRC[7]<<24);
  int srcp=8;
  if ((riffsize<0)||(srcp>srcc-riffsize)) return 0;
  srcc=srcp+riffsize;
  if ((srcp>srcc-4)||memcmp(src,"WAVE",4)) return 0;
  srcp+=4;
  
  // Remainder is in blocks. Ordinarily a "fmt " followed by a "data", but we'll take them in any order, and any count of "data".
  struct wav_file *file=calloc(1,sizeof(struct wav_file));
  if (!file) return 0;
  if (wav_file_decode_inner(file,SRC+srcp,srcc-srcp)<0) {
    wav_file_del(file);
    return 0;
  }
  return file;
}

/* 16-bit mono from 8-bit mono (requires reallocation).
 */
 
static int wav_file_16bit_from_8bit(struct wav_file *file) {
  uint8_t *nv=malloc(file->framec*2);
  if (!nv) return -1;
  uint8_t *dstp=nv;
  const uint8_t *srcp=file->v;
  int i=file->framec;
  for (;i-->0;dstp+=2,srcp++) {
    dstp[0]=dstp[1]=srcp[0];
  }
  if (file->ownv) free(file->v);
  file->v=nv;
  file->ownv=1;
  file->samplesize=16;
  return 0;
}

/* Force 16-bit mono.
 */
 
int wav_file_force_16bit_mono(struct wav_file *file) {
  if (!file) return -1;
  if ((file->samplesize==16)&&(file->chanc==1)) return 0;
  
  // There's only one case where we grow: From 8-bit mono.
  if ((file->samplesize==8)&&(file->chanc==1)) return wav_file_16bit_from_8bit(file);
  
  // Every remaining case, we can overwrite the existing buffer.
  // Ensure we own it first.
  if (wav_file_own(file)<0) return -1;
  
  int ossbytes=file->samplesize>>3;
  if (ossbytes<1) return -1;
  uint8_t *dstp=file->v;
  const uint8_t *srcp=file->v;
  int srcstride=ossbytes*file->chanc;
  if (srcstride<2) return -1; // oops i missed something...
  if (ossbytes>2) srcp+=ossbytes-2; // Copy from most significant 2 bytes of first channel.
  int i=file->framec;
  if (ossbytes==1) { // Expanding samples.
    for (;i-->0;dstp+=2,srcp+=srcstride) {
      dstp[0]=dstp[1]=srcp[0];
    }
  } else { // Truncating or preserving samples.
    for (;i-->0;dstp+=2,srcp+=srcstride) {
      dstp[0]=srcp[0];
      dstp[1]=srcp[1];
    }
  }
  
  file->samplesize=16;
  file->chanc=1;
  file->samplec=file->framec;
  return 0;
}
