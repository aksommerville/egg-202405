#include "rawimg.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

/* Delete.
 */

void rawimg_cleanup(struct rawimg *rawimg) {
  if (!rawimg) return;
  if (rawimg->v&&rawimg->ownv) free(rawimg->v);
  if (rawimg->ctab) free(rawimg->ctab);
}

void rawimg_del(struct rawimg *rawimg) {
  if (!rawimg) return;
  if (rawimg->v&&rawimg->ownv) free(rawimg->v);
  if (rawimg->ctab) free(rawimg->ctab);
  free(rawimg);
}

/* New.
 */

struct rawimg *rawimg_new_alloc(int w,int h,int pixelsize) {
  if ((h<1)||(h>0x7fff)) return 0;
  int stride=rawimg_minimum_stride(w,pixelsize);
  if (stride<1) return 0;
  if (stride>INT_MAX/h) return 0;
  struct rawimg *rawimg=calloc(1,sizeof(struct rawimg));
  if (!rawimg) return 0;
  if (!(rawimg->v=calloc(stride,h))) {
    free(rawimg);
    return 0;
  }
  rawimg->ownv=1;
  rawimg->w=w;
  rawimg->h=h;
  rawimg->stride=stride;
  rawimg->pixelsize=pixelsize;
  if (pixelsize<8) rawimg->bitorder='>';
  return rawimg;
}

struct rawimg *rawimg_new_handoff(void *v,int w,int h,int stride,int pixelsize) {
  if (!v) return 0;
  if ((h<1)||(h>0x7fff)) return 0;
  int minstride=rawimg_minimum_stride(w,pixelsize);
  if (minstride<1) return 0;
  if (stride<=0) stride=minstride;
  else if (stride<minstride) return 0;
  if (stride>INT_MAX/h) return 0;
  struct rawimg *rawimg=calloc(1,sizeof(struct rawimg));
  if (!rawimg) return 0;
  rawimg->v=v; // HANDOFF
  rawimg->ownv=1;
  rawimg->w=w;
  rawimg->h=h;
  rawimg->stride=stride;
  rawimg->pixelsize=pixelsize;
  if (pixelsize<8) rawimg->bitorder='>';
  return rawimg;
}

struct rawimg *rawimg_new_borrow(void *v,int w,int h,int stride,int pixelsize) {
  if (!v) return 0;
  if ((h<1)||(h>0x7fff)) return 0;
  int minstride=rawimg_minimum_stride(w,pixelsize);
  if (minstride<1) return 0;
  if (stride<=0) stride=minstride;
  else if (stride<minstride) return 0;
  if (stride>INT_MAX/h) return 0;
  struct rawimg *rawimg=calloc(1,sizeof(struct rawimg));
  if (!rawimg) return 0;
  rawimg->v=v; // BORROW
  rawimg->ownv=0;
  rawimg->w=w;
  rawimg->h=h;
  rawimg->stride=stride;
  rawimg->pixelsize=pixelsize;
  if (pixelsize<8) rawimg->bitorder='>';
  return rawimg;
}

struct rawimg *rawimg_new_copy(const struct rawimg *src) {
  if (!src) return 0;
  struct rawimg *dst=calloc(1,sizeof(struct rawimg));
  if (!dst) return 0;
  memcpy(dst,src,sizeof(struct rawimg));
  dst->v=0;
  dst->ctab=0;
  
  int dststride=rawimg_minimum_stride(dst->w,dst->pixelsize);
  if ((dststride<1)||(dststride>src->stride)) {
    rawimg_del(dst);
    return 0;
  }
  if (!(dst->v=calloc(dst->stride,dst->h))) {
    rawimg_del(dst);
    return 0;
  }
  if (dststride!=src->stride) {
    const uint8_t *srcrow=src->v;
    uint8_t *dstrow=dst->v;
    int yi=dst->h;
    for (;yi-->0;srcrow+=src->stride,dstrow+=dst->stride) {
      memcpy(dstrow,srcrow,dststride);
    }
  } else {
    memcpy(dst->v,src->v,dst->stride*dst->h);
  }
  
  if (dst->ctabc>0) {
    if (!(dst->ctab=malloc(dst->ctabc<<2))) {
      rawimg_del(dst);
      return 0;
    }
    memcpy(dst->ctab,src->ctab,dst->ctabc<<2);
  } else {
    dst->ctabc=0;
  }
  
  return dst;
}

/* Set color table.
 */
 
int rawimg_set_ctab(struct rawimg *rawimg,const void *rgba,int colorc) {
  if (colorc<0) return -1;
  void *nv=0;
  if (colorc) {
    if (!rgba) return -1;
    if (!(nv=malloc(colorc<<2))) return -1;
    memcpy(nv,rgba,colorc<<2);
  }
  if (rawimg->ctab) free(rawimg->ctab);
  rawimg->ctab=nv;
  rawimg->ctabc=colorc;
  return 0;
}

int rawimg_require_ctab(struct rawimg *rawimg,int colorc) {
  if (colorc<0) return -1;
  if (colorc<=rawimg->ctabc) return 0;
  void *nv=realloc(rawimg->ctab,colorc<<2);
  if (!nv) return -1;
  memset(nv+(rawimg->ctabc<<2),0,(colorc-rawimg->ctabc)<<2);
  rawimg->ctab=nv;
  rawimg->ctabc=colorc;
  return 0;
}

/* Minimum stride.
 */
 
int rawimg_minimum_stride(int w,int pixelsize) {
  if ((w<1)||(w>0x7fff)) return -1;
  if (pixelsize<1) return -1;
  switch (pixelsize) {
    case 1: return (w+7)>>3;
    case 2: return (w+3)>>2;
    case 4: return (w+1)>>1;
    case 8: return w;
    default: if (pixelsize&7) return -1; if (pixelsize>0x80) return -1; return w*(pixelsize>>3);
  }
  return -1;
}
