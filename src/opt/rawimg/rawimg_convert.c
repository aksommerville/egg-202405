#include "rawimg.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>

#if USE_macos
  #include <machine/endian.h>
#elif USE_mswin
  #define LITTLE_ENDIAN 1234
  #define BIG_ENDIAN 4321
  #define BYTE_ORDER LITTLE_ENDIAN
#else
  #include <endian.h>
#endif

#if BYTE_ORDER==BIG_ENDIAN
  static int rawimg_masks_32[4]={
    0xff000000,
    0x00ff0000,
    0x0000ff00,
    0x000000ff,
  };
  static int rawimg_masks_16[2]={
    0xff00,
    0x00ff,
  };
#else
  static int rawimg_masks_32[4]={
    0x000000ff,
    0x0000ff00,
    0x00ff0000,
    0xff000000,
  };
  static int rawimg_masks_16[2]={
    0x00ff,
    0xff00,
  };
#endif

/* Convert, with xform and per-pixel callback.
 * TODO Do we anticipate any use for this? I'm not sure it's helpful.
 */

struct rawimg *rawimg_new_convert(
  const struct rawimg *src,
  int pixelsize,
  int x,int y,int w,int h,
  uint8_t xform,
  int (*cvt)(int srcpx,void *userdata),
  void *userdata
) {
  struct rawimg_iterator srciter;
  if (rawimg_iterate(&srciter,src,x,y,w,h,xform&(RAWIMG_XFORM_XREV|RAWIMG_XFORM_YREV))<0) return 0;
  int dstw,dsth;
  if (xform&RAWIMG_XFORM_SWAP) {
    dstw=h;
    dsth=w;
  } else {
    dstw=w;
    dsth=h;
  }
  struct rawimg *dst=rawimg_new_alloc(dstw,dsth,pixelsize);
  if (!dst) return 0;
  
  // If pixelsize remains constant, assume the rest of the format specifiers do too.
  // Changed, just leave them all zero, as we would for a new image.
  if (pixelsize==src->pixelsize) {
    dst->rmask=src->rmask;
    dst->gmask=src->gmask;
    dst->bmask=src->bmask;
    dst->amask=src->amask;
    memcpy(dst->chorder,src->chorder,sizeof(src->chorder));
    dst->bitorder=src->bitorder;
  }
  
  dst->encfmt=src->encfmt;
  if (rawimg_set_ctab(dst,src->ctab,src->ctabc)<0) {
    rawimg_del(dst);
    return 0;
  }
  
  struct rawimg_iterator dstiter;
  if (rawimg_iterate(&dstiter,dst,0,0,dstw,dsth,xform&RAWIMG_XFORM_SWAP)<0) {
    rawimg_del(dst);
    return 0;
  }
  if (cvt) {
    do {
      int pixel=rawimg_iterator_read(&srciter);
      pixel=cvt(pixel,userdata);
      rawimg_iterator_write(&dstiter,pixel);
    } while (rawimg_iterator_next(&srciter)&&rawimg_iterator_next(&dstiter));
  } else {
    do {
      int pixel=rawimg_iterator_read(&srciter);
      rawimg_iterator_write(&dstiter,pixel);
    } while (rawimg_iterator_next(&srciter)&&rawimg_iterator_next(&dstiter));
  }
  return dst;
}

/* Test for canonical formats.
 */
 
int rawimg_is_rgba(const struct rawimg *rawimg) {
  if (!rawimg) return 0;
  if (rawimg->pixelsize!=32) return 0;
  if (rawimg->rmask) { // use masks...
    if (rawimg->rmask!=rawimg_masks_32[0]) return 0;
    if (rawimg->gmask!=rawimg_masks_32[1]) return 0;
    if (rawimg->bmask!=rawimg_masks_32[2]) return 0;
    if (rawimg->amask!=rawimg_masks_32[3]) return 0;
    return 2;
  }
  if (rawimg->chorder[0]) { // use channel order
    if (!memcmp(rawimg->chorder,"RGBA",4)) return 2;
    if (!memcmp(rawimg->chorder,"rgba",4)) return 2;
    return 0;
  }
  // 32 bits but format unspecified? Yeah, let's go with it.
  return 1;
}

int rawimg_is_rgb(const struct rawimg *rawimg) {
  if (!rawimg) return 0;
  if (rawimg->pixelsize!=24) return 0;
  if (!memcmp(rawimg->chorder,"rgb\0",4)) return 2;
  if (!memcmp(rawimg->chorder,"RGB\0",4)) return 2;
  if (!rawimg->chorder[0]) return 1;
  return 0;
}

int rawimg_is_y8(const struct rawimg *rawimg) {
  if (!rawimg) return 0;
  if (rawimg->pixelsize!=8) return 0;
  if (rawimg->rmask) {
    if (rawimg->rmask==0xff) return 2;
    return 0;
  }
  if (rawimg->chorder[0]) {
    if (
      ((rawimg->chorder[0]=='y')||(rawimg->chorder[0]=='Y'))&&
      !rawimg->chorder[1]
    ) return 2;
    return 0;
  }
  return 1;
}

int rawimg_is_y1(const struct rawimg *rawimg) {
  if (!rawimg) return 0;
  if (rawimg->pixelsize!=1) return 0;
  if (rawimg->bitorder=='<') return 0;
  if (rawimg->ctabc>0) return 1;
  return 2;
}

/* Force a 32-bit image to canonical RGBA, in place.
 */
 
static int rawimg_swizzle_to_rgba(struct rawimg *rawimg) {
  if (!memcmp(rawimg->chorder,"RGBA",4)) return 0;
  if (!memcmp(rawimg->chorder,"rgba",4)) return 0;
  uint32_t bodetect=0x04030201,cr,cg,cb,ca;
  if (*(uint8_t*)&bodetect==0x04) {
    cr=0xff000000;
    cg=0x00ff0000;
    cb=0x0000ff00;
    ca=0x000000ff;
  } else {
    cr=0x000000ff;
    cg=0x0000ff00;
    cb=0x00ff0000;
    ca=0xff000000;
  }
  if ((rawimg->rmask==cr)&&(rawimg->gmask==cg)&&(rawimg->bmask==cb)&&(rawimg->amask==ca)) return 0;
  uint8_t srcp_by_dstp[4]={0xff,0xff,0xff,0xff};
  int i=0; for (;i<4;i++) switch (rawimg->chorder[i]) {
    case 'r': case 'R': srcp_by_dstp[0]=i; break;
    case 'g': case 'G': srcp_by_dstp[1]=i; break;
    case 'b': case 'B': srcp_by_dstp[2]=i; break;
    case 'a': case 'A': srcp_by_dstp[3]=i; break;
  }
  #define position_for_mask(m) ({ \
    int _p=0; \
         if (m==cr) _p=0; \
    else if (m==cg) _p=1; \
    else if (m==cb) _p=2; \
    else if (m==ca) _p=3; \
    _p; \
  })
  if (srcp_by_dstp[0]>3) srcp_by_dstp[0]=position_for_mask(rawimg->rmask);
  if (srcp_by_dstp[1]>3) srcp_by_dstp[1]=position_for_mask(rawimg->gmask);
  if (srcp_by_dstp[2]>3) srcp_by_dstp[2]=position_for_mask(rawimg->bmask);
  if (srcp_by_dstp[3]>3) srcp_by_dstp[3]=position_for_mask(rawimg->amask);
  #undef position_for_mask
  uint8_t *row=rawimg->v;
  int yi=rawimg->h;
  for (;yi-->0;row+=rawimg->stride) {
    uint8_t *p=row;
    int xi=rawimg->w;
    for (;xi-->0;p+=4) {
      uint8_t tmp[4]={
        p[srcp_by_dstp[0]],
        p[srcp_by_dstp[1]],
        p[srcp_by_dstp[2]],
        p[srcp_by_dstp[3]],
      };
      memcpy(p,tmp,4);
    }
  }
  memcpy(rawimg->chorder,"rgba",4);
  rawimg->rmask=cr;
  rawimg->gmask=cg;
  rawimg->bmask=cb;
  rawimg->amask=ca;
  return 0;
}

/* Rewrite as RGBA from anything, replacing buffer.
 */
 
static int rawimg_force_to_rgba(struct rawimg *rawimg) {
  struct rawimg_iterator iter;
  if (rawimg_iterate(&iter,rawimg,0,0,rawimg->w,rawimg->h,0)<0) return -1;
  uint32_t *nv=malloc(rawimg->w*rawimg->h*4);
  if (!nv) return -1;
  uint32_t *dstp=nv;
  int i=rawimg->w*rawimg->h;
  
  // Use masks if present, and pixelsize 32 or smaller.
  if ((rawimg->pixelsize<=32)&&rawimg->rmask) {
    uint32_t tmp;
    int rp=0,rc=0,gp=0,gc=0,bp=0,bc=0,ap=0,ac=0;
    #define MEASURECHANNEL(ch) if (tmp=rawimg->ch##mask) { \
      while (!(tmp&1)) { tmp>>=1; ch##p++; } \
      while (tmp) { tmp>>=1; ch##c++; } \
    }
    MEASURECHANNEL(r)
    MEASURECHANNEL(g)
    MEASURECHANNEL(b)
    MEASURECHANNEL(a)
    #undef MEASURECHANNEL
    for (;i-->0;dstp++) {
      int pixel=rawimg_iterator_read(&iter);
      int r,g,b,a;
      #define READCHANNEL(ch,ifunset) if (ch##c) { \
        ch=(pixel>>ch##p)&0xff; \
        if (ch##c<8) { \
          ch<<=8-ch##c; \
          ch&=0xff; \
          int have=ch##c; \
          while (have<8) { \
            ch|=ch>>have; \
            have<<=1; \
          } \
        } else if (ch##c>8) { \
          ch>>=ch##c-8; \
        } \
      } else ch=ifunset;
      READCHANNEL(r,0x00)
      READCHANNEL(g,0x00)
      READCHANNEL(b,0x00)
      READCHANNEL(a,0xff)
      #undef READCHANNEL
      ((uint8_t*)dstp)[0]=r;
      ((uint8_t*)dstp)[1]=g;
      ((uint8_t*)dstp)[2]=b;
      ((uint8_t*)dstp)[3]=a;
      if (!rawimg_iterator_next(&iter)) break;
    }
    
  // Use chorder if channels align bytewise.
  } else if (!(rawimg->pixelsize&7)&&rawimg->chorder[0]) {
    int chanc=1;
    while ((chanc<4)&&rawimg->chorder[chanc]) chanc++;
    if (rawimg->pixelsize%chanc) goto _fallback_;
    int bits_per_channel=rawimg->pixelsize/chanc;
    if (bits_per_channel&7) goto _fallback_;
    int bytes_per_channel=bits_per_channel>>3;
    int leoffset=bytes_per_channel-1;
    int rp=-1,gp=-1,bp=-1,ap=-1;
    int j=0; for (;j<chanc;j++) {
      switch (rawimg->chorder[j]) {
        case 'R': rp=j*bytes_per_channel; break;
        case 'r': rp=j*bytes_per_channel+leoffset; break;
        case 'G': gp=j*bytes_per_channel; break;
        case 'g': gp=j*bytes_per_channel+leoffset; break;
        case 'B': bp=j*bytes_per_channel; break;
        case 'b': bp=j*bytes_per_channel+leoffset; break;
        case 'A': ap=j*bytes_per_channel; break;
        case 'a': ap=j*bytes_per_channel+leoffset; break;
        case 'Y': rp=gp=bp=j*bytes_per_channel; break;
        case 'y': rp=gp=bp=j*bytes_per_channel+leoffset; break;
      }
    }
    for (;i-->0;dstp++) {
      if (rp>=0) ((uint8_t*)dstp)[0]=((uint8_t*)iter.minor.p)[rp]; else ((uint8_t*)dstp)[0]=0x00;
      if (gp>=0) ((uint8_t*)dstp)[1]=((uint8_t*)iter.minor.p)[gp]; else ((uint8_t*)dstp)[1]=0x00;
      if (bp>=0) ((uint8_t*)dstp)[2]=((uint8_t*)iter.minor.p)[bp]; else ((uint8_t*)dstp)[2]=0x00;
      if (ap>=0) ((uint8_t*)dstp)[3]=((uint8_t*)iter.minor.p)[ap]; else ((uint8_t*)dstp)[3]=0xff;
      if (!rawimg_iterator_next(&iter)) break;
    }
    
  // Pixels 8 bits or smaller, treat as gray.
  } else if (rawimg->pixelsize<=8) {
    uint32_t alpha;
    uint32_t bodetect=0x04030201;
    if (*(uint8_t*)&bodetect==0x04) alpha=0x000000ff; else alpha=0xff000000;
    switch (rawimg->pixelsize) {
      case 1: {
          for (;i-->0;dstp++) {
            int pixel=rawimg_iterator_read(&iter);
            if (pixel) *dstp=0xffffffff; else *dstp=alpha;
            if (!rawimg_iterator_next(&iter)) break;
          }
        } break;
      case 2: {
          for (;i-->0;dstp++) {
            int pixel=rawimg_iterator_read(&iter);
            pixel|=pixel<<2;
            pixel|=pixel<<4;
            pixel|=pixel<<8;
            pixel|=pixel<<16;
            *dstp=pixel|alpha;
            if (!rawimg_iterator_next(&iter)) break;
          }
        } break;
      case 4: {
          for (;i-->0;dstp++) {
            int pixel=rawimg_iterator_read(&iter);
            pixel|=pixel<<4;
            pixel|=pixel<<8;
            pixel|=pixel<<16;
            *dstp=pixel|alpha;
            if (!rawimg_iterator_next(&iter)) break;
          }
        } break;
      case 8: {
          for (;i-->0;dstp++) {
            int pixel=rawimg_iterator_read(&iter);
            pixel|=pixel<<8;
            pixel|=pixel<<16;
            *dstp=pixel|alpha;
            if (!rawimg_iterator_next(&iter)) break;
          }
        } break;
    }
  
  // Welp I'm not sure how to handle it. Fail.
  // There should still be cases we can handle. Not sure exactly what.
  } else {
   _fallback_:;
    free(nv);
    return -1;
  }

  if (rawimg->ownv) free(rawimg->v);
  rawimg->v=nv;
  rawimg->ownv=1;
  rawimg->stride=rawimg->w<<2;
  rawimg->pixelsize=32;
  memcpy(rawimg->chorder,"rgba",4);
  uint32_t bodetect=0x04030201;
  if (*(uint8_t*)&bodetect==0x04) {
    rawimg->rmask=0xff000000;
    rawimg->gmask=0x00ff0000;
    rawimg->bmask=0x0000ff00;
    rawimg->amask=0x000000ff;
  } else {
    rawimg->rmask=0x000000ff;
    rawimg->gmask=0x0000ff00;
    rawimg->bmask=0x00ff0000;
    rawimg->amask=0xff000000;
  }
  return 0;
}

/* Swizzle to RGB.
 */
 
static int rawimg_swizzle_to_rgb(struct rawimg *rawimg) {
  int rp=0,gp=0,bp=0,i;
  for (i=0;i<4;i++) {
    switch (rawimg->chorder[i]) {
      case 'r': case 'R': rp=i; break;
      case 'g': case 'G': gp=i; break;
      case 'b': case 'B': bp=i; break;
    }
  }
  uint8_t *row=rawimg->v;
  int yi=rawimg->h;
  for (;yi-->0;row+=rawimg->stride) {
    uint8_t *p=row;
    int xi=rawimg->w;
    for (;xi-->0;p+=3) {
      uint8_t r=p[rp],g=p[gp],b=p[bp];
      p[0]=r;
      p[1]=g;
      p[2]=b;
    }
  }
  memcpy(rawimg->chorder,"rgb\0",4);
  return 0;
}

/* Force to RGB.
 */
 
static int rawimg_force_to_rgb(struct rawimg *rawimg) {
  struct rawimg_iterator iter={0};
  if (rawimg_iterate(&iter,rawimg,0,0,rawimg->w,rawimg->h,0)<0) return -1;
  int nstride=rawimg->w*3;
  uint8_t *nv=malloc(nstride*rawimg->h);
  if (!nv) return -1;
  uint8_t *dstp=nv;
  
  // We will operate only off channel masks.
  // If they're absent, assume grayscale.
  int rp=0,rc=0,gp=0,gc=0,bp=0,bc=0;
  if (rawimg->rmask) {
    uint32_t tmp;
    if (tmp=rawimg->rmask) { while (!(tmp&1)) { tmp>>=1; rp++; } while (tmp&1) { tmp>>=1; rc++; } }
    if (tmp=rawimg->gmask) { while (!(tmp&1)) { tmp>>=1; gp++; } while (tmp&1) { tmp>>=1; gc++; } }
    if (tmp=rawimg->bmask) { while (!(tmp&1)) { tmp>>=1; bp++; } while (tmp&1) { tmp>>=1; bc++; } }
    if (rc>8) { rp+=rc-8; rc=8; }
    if (gc>8) { gp+=gc-8; gc=8; }
    if (bc>8) { bp+=gc-8; gc=8; }
  } else {
    rc=gc=bc=rawimg->pixelsize;
  }
  
  do {
    uint32_t pixel=rawimg_iterator_read(&iter);
    #define EXPAND(ch) if (ch##c&&(ch##c<8)) { \
      ch<<=(8-ch##c); \
      int havec=ch##c; \
      while (havec<8) { \
        ch|=ch>>havec; \
        havec<<=1; \
      } \
    }
    uint8_t r=pixel>>rp; EXPAND(r)
    uint8_t g=pixel>>gp; EXPAND(g)
    uint8_t b=pixel>>bp; EXPAND(b)
    #undef EXPAND
    *(dstp++)=r;
    *(dstp++)=g;
    *(dstp++)=b;
  } while (rawimg_iterator_next(&iter));
  
  if (rawimg->ownv) free(rawimg->v);
  rawimg->v=nv;
  rawimg->ownv=1;
  rawimg->stride=nstride;
  rawimg->pixelsize=24;
  memcpy(rawimg->chorder,"rgb\0",4);
  rawimg->rmask=rawimg->gmask=rawimg->bmask=rawimg->amask=0;
  return 0;
}

/* Rewrite as y8. Incoming pixels must be smaller than 8.
 */
 
static int rawimg_expand_to_y8(struct rawimg *rawimg) {
  struct rawimg_iterator iter;
  if (rawimg_iterate(&iter,rawimg,0,0,rawimg->w,rawimg->h,0)<0) return -1;
  uint8_t *nv=malloc(rawimg->w*rawimg->h);
  if (!nv) return -1;
  uint8_t *dstp=nv;
  int i=rawimg->w*rawimg->h;
  switch (rawimg->pixelsize) {
    case 1: {
        for (;i-->0;dstp++) {
          int pixel=rawimg_iterator_read(&iter);
          *dstp=pixel?0xff:0x00;
          if (!rawimg_iterator_next(&iter)) break;
        }
      } break;
    case 2: {
        for (;i-->0;dstp++) {
          int pixel=rawimg_iterator_read(&iter);
          *dstp=pixel|(pixel<<2)|(pixel<<4)|(pixel<<6);
          if (!rawimg_iterator_next(&iter)) break;
        }
      } break;
    case 4: {
        for (;i-->0;dstp++) {
          int pixel=rawimg_iterator_read(&iter);
          *dstp=pixel|(pixel<<4);
          if (!rawimg_iterator_next(&iter)) break;
        }
      } break;
  }
  free(rawimg->v);
  rawimg->v=nv;
  rawimg->stride=rawimg->w;
  rawimg->pixelsize=8;
  memcpy(rawimg->chorder,"y\0\0\0",4);
  rawimg->rmask=rawimg->gmask=rawimg->bmask=0xff;
  rawimg->amask=0;
  return 0;
}

/* Rewrite as RGBA by expanding color table.
 */
 
static int rawimg_expand_ctab(struct rawimg *rawimg) {
  if (!rawimg->ctab) return -1;
  struct rawimg_iterator iter;
  if (rawimg_iterate(&iter,rawimg,0,0,rawimg->w,rawimg->h,0)<0) return -1;
  uint32_t *nv=malloc(rawimg->w*rawimg->h*4);
  if (!nv) return -1;
  uint32_t *dstp=nv;
  int i=rawimg->w*rawimg->h;
  while (i-->0) {
    int ix=rawimg_iterator_read(&iter);
    if ((ix>=0)&&(ix<rawimg->ctabc)) *dstp=((uint32_t*)rawimg->ctab)[ix];
    else *dstp=0;
    if (!rawimg_iterator_next(&iter)) break;
    dstp++;
  }
  free(rawimg->v);
  rawimg->v=nv;
  rawimg->stride=rawimg->w<<2;
  rawimg->pixelsize=32;
  memcpy(rawimg->chorder,"rgba",4);
  uint32_t bodetect=0x04030201;
  if (*(uint8_t*)&bodetect==0x04) {
    rawimg->rmask=0xff000000;
    rawimg->gmask=0x00ff0000;
    rawimg->bmask=0x0000ff00;
    rawimg->amask=0x000000ff;
  } else {
    rawimg->rmask=0x000000ff;
    rawimg->gmask=0x0000ff00;
    rawimg->bmask=0x00ff0000;
    rawimg->amask=0xff000000;
  }
  free(rawimg->ctab);
  rawimg->ctab=0;
  rawimg->ctabc=0;
  return 0;
}

/* Rewrite as RGB by expanding color table.
 */
 
static int rawimg_expand_ctab_rgb(struct rawimg *rawimg) {
  if (!rawimg->ctab) return -1;
  struct rawimg_iterator iter;
  if (rawimg_iterate(&iter,rawimg,0,0,rawimg->w,rawimg->h,0)<0) return -1;
  uint8_t *nv=malloc(rawimg->w*rawimg->h*3);
  if (!nv) return -1;
  uint8_t *dstp=nv;
  int i=rawimg->w*rawimg->h;
  while (i-->0) {
    int ix=rawimg_iterator_read(&iter);
    if ((ix>=0)&&(ix<rawimg->ctabc)) {
      memcpy(dstp,((uint32_t*)rawimg->ctab)+ix,3);
    } else {
      dstp[0]=dstp[1]=dstp[2]=0;
    }
    if (!rawimg_iterator_next(&iter)) break;
    dstp+=3;
  }
  free(rawimg->v);
  rawimg->v=nv;
  rawimg->stride=rawimg->w*3;
  rawimg->pixelsize=24;
  memcpy(rawimg->chorder,"rgb\0",4);
  rawimg->rmask=rawimg->gmask=rawimg->bmask=rawimg->amask=0;
  free(rawimg->ctab);
  rawimg->ctab=0;
  rawimg->ctabc=0;
  return 0;
}

/* Rewrite as y8 by expanding color table.
 */
 
static int rawimg_expand_ctab_y8(struct rawimg *rawimg) {
  if (!rawimg->ctab) return -1;
  struct rawimg_iterator iter;
  if (rawimg_iterate(&iter,rawimg,0,0,rawimg->w,rawimg->h,0)<0) return -1;
  uint8_t *nv=malloc(rawimg->w*rawimg->h);
  if (!nv) return -1;
  uint8_t *dstp=nv;
  int i=rawimg->w*rawimg->h;
  while (i-->0) {
    int ix=rawimg_iterator_read(&iter);
    if ((ix>=0)&&(ix<rawimg->ctabc)) {
      const uint8_t *src=rawimg->ctab+ix*4;
      *dstp=(src[0]+src[1]+src[2])/3;
    } else *dstp=0;
    if (!rawimg_iterator_next(&iter)) break;
    dstp++;
  }
  free(rawimg->v);
  rawimg->v=nv;
  rawimg->stride=rawimg->w;
  rawimg->pixelsize=8;
  memcpy(rawimg->chorder,"y\0\0\0",4);
  rawimg->rmask=rawimg->gmask=rawimg->bmask=0xff;
  rawimg->amask=0;
  free(rawimg->ctab);
  rawimg->ctab=0;
  rawimg->ctabc=0;
  return 0;
}
 
static int rawimg_expand_ctab_y1(struct rawimg *rawimg) {
  if (!rawimg->ctab) return -1;
  struct rawimg_iterator iter;
  if (rawimg_iterate(&iter,rawimg,0,0,rawimg->w,rawimg->h,0)<0) return -1;
  int nstride=(rawimg->w+7)>>3;
  uint8_t *nv=calloc(nstride,rawimg->h);
  if (!nv) return -1;
  uint8_t *dstrow=nv;
  int yi=rawimg->h;
  for (;yi-->0;dstrow+=nstride) {
    uint8_t *dstp=dstrow;
    uint8_t dstmask=0x80;
    int xi=rawimg->w;
    for (;xi-->0;) {
      int ix=rawimg_iterator_read(&iter);
      if ((ix>=0)&&(ix<rawimg->ctabc)) {
        const uint8_t *src=rawimg->ctab+ix*4;
        if (src[0]+src[1]+src[2]>=384) (*dstp)|=dstmask;
      }
      if (dstmask==1) { dstp++; dstmask=0x80; }
      else dstmask>>=1;
      if (!rawimg_iterator_next(&iter)) break;
    }
  }
  free(rawimg->v);
  rawimg->v=nv;
  rawimg->stride=nstride;
  rawimg->pixelsize=1;
  memcpy(rawimg->chorder,"y\0\0\0",4);
  rawimg->rmask=rawimg->gmask=rawimg->bmask=rawimg->amask=0;
  free(rawimg->ctab);
  rawimg->ctab=0;
  rawimg->ctabc=0;
  return 0;
}

/* Reverse order of each byte.
 */
 
static int rawimg_reverse_bits_1(struct rawimg *rawimg) {
  uint8_t *p=rawimg->v;
  int i=rawimg->stride*rawimg->h;
  for (;i-->0;p++) {
    *p=(
      (((*p)&0x80)>>7)|
      (((*p)&0x40)>>5)|
      (((*p)&0x20)>>3)|
      (((*p)&0x10)>>1)|
      (((*p)&0x08)<<1)|
      (((*p)&0x04)<<3)|
      (((*p)&0x02)<<5)|
      (((*p)&0x01)<<7)
    );
  }
  switch (rawimg->bitorder) {
    case '>': rawimg->bitorder='<'; break;
    case '<': rawimg->bitorder='>'; break;
  }
  return 0;
}

/* y8 or y1 from any other format except index.
 */
 
static uint8_t rawimg_luma_from_pixel(const struct rawimg *rawimg,int pixel) {
  #define TRYMASK(ch) if (rawimg->ch##mask) { \
    uint32_t tmp=rawimg->ch##mask; \
    while (!(tmp&1)) { tmp>>=1; pixel>>=1; } \
    pixel&=tmp; \
    int morec=0; \
    while (!(tmp&0x80)) { tmp<<=1; pixel<<=1; morec++; } \
    while (morec>0) { \
      pixel|=pixel>>(8-morec); \
      morec-=8-morec; \
    } \
    return pixel; \
  }
  TRYMASK(r)
  TRYMASK(g)
  TRYMASK(b)
  TRYMASK(a)
  #undef TRYMASK
  switch (rawimg->pixelsize) {
    case 1: return pixel?0xff:0x00;
    case 2: pixel|=pixel<<2; pixel|=pixel<<4; return pixel;
    case 4: return pixel|(pixel<<4);
    case 8: return pixel;
    case 16: return pixel>>8;
    case 24: return ((pixel&0xff)+((pixel>>8)&0xff)+((pixel>>16)&0xff))/3;
    case 32: return ((pixel&0xff)+((pixel>>8)&0xff)+((pixel>>16)&0xff)+((pixel>>24)&0xff))>>2;
  }
  return pixel;
}
 
static int rawimg_y8_from_large(struct rawimg *rawimg) {
  struct rawimg_iterator iter;
  if (rawimg_iterate(&iter,rawimg,0,0,rawimg->w,rawimg->h,0)<0) return -1;
  uint8_t *nv=malloc(rawimg->w*rawimg->h);
  if (!nv) return -1;
  uint8_t *dstp=nv;
  int i=rawimg->w*rawimg->h;
  while (i-->0) {
    int pixel=rawimg_iterator_read(&iter);
    *dstp=rawimg_luma_from_pixel(rawimg,pixel);
    dstp++;
  }
  free(rawimg->v);
  rawimg->v=nv;
  rawimg->stride=rawimg->w;
  rawimg->pixelsize=8;
  memcpy(rawimg->chorder,"y\0\0\0",4);
  rawimg->rmask=rawimg->gmask=rawimg->bmask=0xff;
  rawimg->amask=0;
  return 0;
}

static int rawimg_y1_from_large(struct rawimg *rawimg) {
  struct rawimg_iterator iter;
  if (rawimg_iterate(&iter,rawimg,0,0,rawimg->w,rawimg->h,0)<0) return -1;
  int nstride=(rawimg->w+7)>>3;
  uint8_t *nv=calloc(nstride,rawimg->h);
  if (!nv) return -1;
  uint8_t *dstrow=nv;
  int yi=rawimg->h;
  for (;yi-->0;dstrow+=nstride) {
    uint8_t *dstp=dstrow;
    uint8_t dstmask=0x80;
    int xi=rawimg->w;
    for (;xi-->0;) {
      int pixel=rawimg_iterator_read(&iter);
      int luma=rawimg_luma_from_pixel(rawimg,pixel);
      if (luma&0x80) (*dstp)|=dstmask;
      if (dstmask==1) { dstp++; dstmask=0x80; }
      else dstmask>>=1;
      if (!rawimg_iterator_next(&iter)) break;
    }
  }
  free(rawimg->v);
  rawimg->v=nv;
  rawimg->stride=nstride;
  rawimg->pixelsize=1;
  memcpy(rawimg->chorder,"y\0\0\0",4);
  rawimg->bitorder='>';
  rawimg->rmask=rawimg->gmask=rawimg->bmask=rawimg->amask=0;
  return 0;
}

/* Canonicalize.
 */
 
int rawimg_canonicalize(struct rawimg *rawimg) {
  if (!rawimg) return -1;
  
  /* If a color table is in play, expand it to RGBA.
   */
  if (rawimg->ctabc) {
    return rawimg_expand_ctab(rawimg);
  }
  
  /* 1-bit pixels, just swizzle bits if little-endian, beyond that anything goes.
   */
  if (rawimg->pixelsize==1) {
    if (rawimg->bitorder=='<') {
      return rawimg_reverse_bits_1(rawimg);
    }
    return 0;
  }
  
  /* Any other pixel size below 8, assume it's gray and expand to 8.
   * Or if exactly 8 already, assume it's gray.
   */
  if (rawimg->pixelsize<8) {
    return rawimg_expand_to_y8(rawimg);
  }
  if (rawimg->pixelsize==8) return 0;
  
  /* 32-bit pixels we only need to swizzle or do nothing.
   */
  if (rawimg->pixelsize==32) {
    return rawimg_swizzle_to_rgba(rawimg);
  }
  
  /* Everything else goes to RGBA by force.
   */
  return rawimg_force_to_rgba(rawimg);
}

/* Canonicalize, to a specific form.
 * These are basically the same as canonicalize, just some choices are removed.
 */
 
int rawimg_force_rgba(struct rawimg *rawimg) {
  if (!rawimg) return -1;
  if (rawimg_is_rgba(rawimg)) return 0;
  if (rawimg->ctabc) {
    return rawimg_expand_ctab(rawimg);
  }
  if (rawimg->pixelsize==32) {
    return rawimg_swizzle_to_rgba(rawimg);
  }
  return rawimg_force_to_rgba(rawimg);
}

int rawimg_force_rgb(struct rawimg *rawimg) {
  if (!rawimg) return -1;
  if (rawimg_is_rgb(rawimg)) return 0;
  if (rawimg->ctabc) {
    return rawimg_expand_ctab_rgb(rawimg);
  }
  if (rawimg->pixelsize==24) {
    return rawimg_swizzle_to_rgb(rawimg);
  }
  return rawimg_force_to_rgb(rawimg);
}

int rawimg_force_y8(struct rawimg *rawimg) {
  if (!rawimg) return -1;
  if (rawimg_is_y8(rawimg)) return 0;
  if (rawimg->ctabc) {
    return rawimg_expand_ctab_y8(rawimg);
  }
  if (rawimg->pixelsize<8) {
    return rawimg_expand_to_y8(rawimg);
  }
  if (rawimg->pixelsize==8) return 0;
  return rawimg_y8_from_large(rawimg);
}

int rawimg_force_y1(struct rawimg *rawimg) {
  if (!rawimg) return -1;
  if (rawimg_is_y1(rawimg)>=2) return 0; // Don't take ambiguous here; that means there's a color table.
  if (rawimg->ctabc) {
    return rawimg_expand_ctab_y1(rawimg);
  }
  if (rawimg->pixelsize==1) {
    if (rawimg->bitorder=='<') {
      return rawimg_reverse_bits_1(rawimg);
    }
    return 0;
  }
  return rawimg_y1_from_large(rawimg);
}
