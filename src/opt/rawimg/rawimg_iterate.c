#include "rawimg.h"
#include <stdio.h>
#include <string.h>

/* Accessors.
 */
 
#define SRC ((const uint8_t*)src)
#define DST ((uint8_t*)dst)

static int rawimg_pxr_1(const struct rawimg_iterator *iter) {
  const uint8_t *src=((uint8_t*)iter->pixels)+(iter->minor.pbit>>3);
  uint8_t mask;
  if (iter->sizebit<0) mask=0x01<<(iter->minor.pbit&7);
  else mask=0x80>>(iter->minor.pbit&7);
  return ((*src)&mask)?1:0;
}

static void rawimg_pxw_1(struct rawimg_iterator *iter,int pixel) {
  uint8_t *dst=((uint8_t*)iter->pixels)+(iter->minor.pbit>>3);
  uint8_t mask;
  if (iter->sizebit<0) mask=0x01<<(iter->minor.pbit&7);
  else mask=0x80>>(iter->minor.pbit&7);
  if (pixel) *DST|=mask;
  else *DST&=~mask;
}
 
static int rawimg_pxr_2(const struct rawimg_iterator *iter) {
  const uint8_t *src=((uint8_t*)iter->pixels)+(iter->minor.pbit>>3);
  if (iter->sizebit<0) switch (iter->minor.pbit&7) {
    case 0: return ((*src)&3);
    case 2: return ((*src)>>2)&3;
    case 4: return ((*src)>>4)&3;
    case 6: return ((*src)>>6)&3;
  } else switch (iter->minor.pbit&7) {
    case 0: return ((*src)>>6)&3;
    case 2: return ((*src)>>4)&3;
    case 4: return ((*src)>>2)&3;
    case 6: return ((*src)&3);
  }
  return 0;
}

static void rawimg_pxw_2(struct rawimg_iterator *iter,int pixel) {
  uint8_t *dst=((uint8_t*)iter->pixels)+(iter->minor.pbit>>3);
  if (iter->sizebit<0) switch (iter->minor.pbit&7) {
    case 0: *dst=((*dst)&0xfc)|(pixel&3); break;
    case 2: *dst=((*dst)&0xf3)|((pixel&3)<<2); break;
    case 4: *dst=((*dst)&0xcf)|((pixel&3)<<4); break;
    case 6: *dst=((*dst)&0x3f)|(pixel<<6); break;
  } else switch (iter->minor.pbit&7) {
    case 0: *dst=((*dst)&0x3f)|(pixel<<6); break;
    case 2: *dst=((*dst)&0xcf)|((pixel&3)<<4); break;
    case 4: *dst=((*dst)&0xf3)|((pixel&3)<<2); break;
    case 6: *dst=((*dst)&0xfc)|(pixel&3); break;
  }
}
 
static int rawimg_pxr_4(const struct rawimg_iterator *iter) {
  const uint8_t *src=((uint8_t*)iter->pixels)+(iter->minor.pbit>>3);
  int hi=iter->minor.pbit&4;
  if (iter->sizebit>0) hi=!hi;
  if (hi) return (*src)>>4;
  return (*src)&15;
}

static void rawimg_pxw_4(struct rawimg_iterator *iter,int pixel) {
  uint8_t *dst=((uint8_t*)iter->pixels)+(iter->minor.pbit>>3);
  int hi=iter->minor.pbit&4;
  if (iter->sizebit>0) hi=!hi;
  if (hi) *dst=((*dst)&0x0f)|(pixel<<4);
  else *dst=((*dst)&0xf0)|(pixel&15);
}
 
static int rawimg_pxr_8(const struct rawimg_iterator *iter) {
  const void *src=iter->minor.p;
  return *(uint8_t*)src;
}

static void rawimg_pxw_8(struct rawimg_iterator *iter,int pixel) {
  void *dst=iter->minor.p;
  *(uint8_t*)dst=pixel;
}
 
static int rawimg_pxr_16(const struct rawimg_iterator *iter) {
  const void *src=iter->minor.p;
  return *(uint16_t*)src;
}

static void rawimg_pxw_16(struct rawimg_iterator *iter,int pixel) {
  void *dst=iter->minor.p;
  *(uint16_t*)dst=pixel;
}
 
static int rawimg_pxr_32(const struct rawimg_iterator *iter) {
  const void *src=iter->minor.p;
  return *(uint32_t*)src;
}

static void rawimg_pxw_32(struct rawimg_iterator *iter,int pixel) {
  void *dst=iter->minor.p;
  *(uint32_t*)dst=pixel;
}

static int rawimg_pxr_24(const struct rawimg_iterator *iter) {
  const uint8_t *src=iter->minor.p;
  return src[0]|(src[1]<<8)|(src[2]<<16);
}
static void rawimg_pxw_24(struct rawimg_iterator *iter,int pixel) {
  uint8_t *dst=iter->minor.p;
  dst[0]=pixel;
  dst[1]=pixel>>8;
  dst[2]=pixel>>16;
}

// 48 and 64 are odd cases that only come up in PNG, far as I've seen.
// Assume that they are 3 or 4 channels of big-endian 16-bit integers, and repack with the most significant bytes only.
static int rawimg_pxr_48(const struct rawimg_iterator *iter) {
  const void *src=iter->minor.p;
  return SRC[0]|(SRC[2]<<8)|(SRC[4]<<16);
}
static void rawimg_pxw_48(struct rawimg_iterator *iter,int pixel) {
  void *dst=iter->minor.p;
  DST[0]=DST[1]=pixel;
  DST[2]=DST[3]=pixel>>8;
  DST[4]=DST[5]=pixel>>16;
}
static int rawimg_pxr_64(const struct rawimg_iterator *iter) {
  const void *src=iter->minor.p;
  return SRC[0]|(SRC[2]<<8)|(SRC[4]<<16)|(SRC[6]<<24);
}
static void rawimg_pxw_64(struct rawimg_iterator *iter,int pixel) {
  void *dst=iter->minor.p;
  DST[0]=DST[1]=pixel;
  DST[2]=DST[3]=pixel>>8;
  DST[4]=DST[5]=pixel>>16;
  DST[6]=DST[7]=pixel>>24;
}
 
static int rawimg_pxr_bytewise(const struct rawimg_iterator *iter) {
  const void *src=iter->minor.p;
  int pixel=0;
  int bytec=iter->pixelsize>>3;
  while (bytec-->0) {
    pixel<<=8;
    pixel|=*SRC;
    src=SRC+1;
  }
  return pixel;
}

static void rawimg_pxw_bytewise(struct rawimg_iterator *iter,int pixel) {
  void *dst=iter->minor.p;
  int bytec=iter->pixelsize>>3;
  uint8_t *p=DST+bytec;
  while (bytec-->0) {
    p--;
    *p=pixel;
    pixel>>=8;
  }
}
 
static int rawimg_iterator_get_accessors(struct rawimg_iterator *iter,int pixelsize) {
  switch (pixelsize) {
    case 1: iter->read=rawimg_pxr_1; iter->write=rawimg_pxw_1; return 0;
    case 2: iter->read=rawimg_pxr_2; iter->write=rawimg_pxw_2; return 0;
    case 4: iter->read=rawimg_pxr_4; iter->write=rawimg_pxw_4; return 0;
    case 8: iter->read=rawimg_pxr_8; iter->write=rawimg_pxw_8; return 0;
    case 16: iter->read=rawimg_pxr_16; iter->write=rawimg_pxw_16; return 0;
    case 24: iter->read=rawimg_pxr_24; iter->write=rawimg_pxw_24; return 0;
    case 32: iter->read=rawimg_pxr_32; iter->write=rawimg_pxw_32; return 0;
    case 48: iter->read=rawimg_pxr_48; iter->write=rawimg_pxw_48; return 0;
    case 64: iter->read=rawimg_pxr_64; iter->write=rawimg_pxw_64; return 0;
    default: {
        if (pixelsize&3) return -1;
        if (pixelsize<1) return -1;
        iter->read=rawimg_pxr_bytewise;
        iter->write=rawimg_pxw_bytewise;
      } return 0;
  }
  return -1;
}

/* Begin iteration.
 */
 
int rawimg_iterate(
  struct rawimg_iterator *iter,
  const struct rawimg *image,
  int x,int y,int w,int h,
  uint8_t xform
) {
  if (!iter||!image) return -1;
  if ((x<0)||(y<0)||(w<1)||(h<1)||(x>image->w-w)||(y>image->h-h)) return -1;
  
  memset(iter,0,sizeof(struct rawimg_iterator));
  if (rawimg_iterator_get_accessors(iter,image->pixelsize)<0) return -1;
  iter->pixelsize=image->pixelsize;
  iter->pixels=image->v;
  
  if (image->pixelsize<8) {
    int dxx=1,dyx=0,dxy=0,dyy=1;
    int startx=x,starty=y;
    if (xform&RAWIMG_XFORM_XREV) {
      startx+=w-1;
      dxx=-1;
    }
    if (xform&RAWIMG_XFORM_YREV) {
      starty+=h-1;
      dyy=-1;
    }
    if (xform&RAWIMG_XFORM_SWAP) {
      dyx=dyy;
      dxy=dxx;
      dyy=0;
      dxx=0;
      iter->major.c=w;
      iter->minor.c=h;
    } else {
      iter->major.c=h;
      iter->minor.c=w;
    }
    iter->major.pbit=iter->minor.pbit=((starty*image->stride)<<3)+startx*image->pixelsize;
    iter->minor.dpbit=((dyx*image->stride)<<3)+dxx*image->pixelsize;
    iter->major.dpbit=((dyy*image->stride)<<3)+dxy*image->pixelsize;
    iter->sizebit=image->pixelsize;
    if (image->bitorder=='<') iter->sizebit=-iter->sizebit;
    
  } else {
    int xstride=image->pixelsize>>3;
    int dxx=1,dyx=0,dxy=0,dyy=1;
    uint8_t *p=((uint8_t*)image->v)+y*image->stride+x*xstride;
    if (xform&RAWIMG_XFORM_XREV) {
      p+=(w-1)*xstride;
      dxx=-1;
    }
    if (xform&RAWIMG_XFORM_YREV) {
      p+=(h-1)*image->stride;
      dyy=-1;
    }
    if (xform&RAWIMG_XFORM_SWAP) {
      dyx=dyy;
      dxy=dxx;
      dyy=0;
      dxx=0;
      iter->major.c=w;
      iter->minor.c=h;
    } else {
      iter->major.c=h;
      iter->minor.c=w;
    }
    iter->major.p=iter->minor.p=p;
    iter->minor.d=dyx*image->stride+dxx*xstride;
    iter->major.d=dyy*image->stride+dxy*xstride;
  }
  
  iter->minor.c--;
  iter->major.c--;
  iter->minorc=iter->minor.c;
  return 0;
}
