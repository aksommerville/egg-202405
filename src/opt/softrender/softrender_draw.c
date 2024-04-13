#include "softrender_internal.h"

/* Generic pixel converter.
 */
 
typedef uint32_t (*softrender_pixcvt_fn)(uint32_t src,struct softrender *softrender,const struct rawimg *dstimg,const struct rawimg *srcimg);

static uint32_t softrender_pixcvt_1_8(uint32_t src,struct softrender *softrender,const struct rawimg *dstimg,const struct rawimg *srcimg) {
  return src>>7;
}

static uint32_t softrender_pixcvt_8_1(uint32_t src,struct softrender *softrender,const struct rawimg *dstimg,const struct rawimg *srcimg) {
  return src?0xff:0x00;
}

static uint32_t softrender_pixcvt_a1_rgba(uint32_t src,struct softrender *softrender,const struct rawimg *dstimg,const struct rawimg *srcimg) {
  return (src&0x00000080)?1:0;
}

static uint32_t softrender_pixcvt_y1_rgba(uint32_t src,struct softrender *softrender,const struct rawimg *dstimg,const struct rawimg *srcimg) {
  // 1 if at least two of the channel MSBs are 1.
  if (src&0x80000000) return (src&0x00808000)?1:0;
  if (src&0x00800000) return (src&0x80008000)?1:0;
  if (src&0x00008000) return (src&0x80800000)?1:0;
  return 0;
}

static uint32_t softrender_pixcvt_a8_rgba(uint32_t src,struct softrender *softrender,const struct rawimg *dstimg,const struct rawimg *srcimg) {
  return src&0xff;
}

static uint32_t softrender_pixcvt_y8_rgba(uint32_t src,struct softrender *softrender,const struct rawimg *dstimg,const struct rawimg *srcimg) {
  uint8_t r=src>>24,g=src>>16,b=src>>8;
  return (r+g+b)/3;
}

static uint32_t softrender_pixcvt_rgba_a1(uint32_t src,struct softrender *softrender,const struct rawimg *dstimg,const struct rawimg *srcimg) {
  return src?0x000000ff:0x00000000;
}

static uint32_t softrender_pixcvt_rgba_a8(uint32_t src,struct softrender *softrender,const struct rawimg *dstimg,const struct rawimg *srcimg) {
  return src;
}

static uint32_t softrender_pixcvt_rgba_y1(uint32_t src,struct softrender *softrender,const struct rawimg *dstimg,const struct rawimg *srcimg) {
  return src?0xffffffff:0x000000ff;
}

static uint32_t softrender_pixcvt_rgba_y8(uint32_t src,struct softrender *softrender,const struct rawimg *dstimg,const struct rawimg *srcimg) {
  return (src<<24)|(src<<16)|(src<<8)|0xff;
}

/* Returns null if no conversion is necessary.
 */
static softrender_pixcvt_fn softrender_get_pixcvt(const struct rawimg *dstimg,const struct rawimg *srcimg,const struct softrender *softrender) {
  if (dstimg->pixelsize==srcimg->pixelsize) return 0;
  switch (dstimg->pixelsize) {
    case 1: switch (srcimg->pixelsize) {
        case 8: return softrender_pixcvt_1_8;
        case 32: if (dstimg->amask) return softrender_pixcvt_a1_rgba; return softrender_pixcvt_y1_rgba;
      } break;
    case 8: switch (srcimg->pixelsize) {
        case 1: return softrender_pixcvt_8_1;
        case 32: if (dstimg->amask) return softrender_pixcvt_a8_rgba; return softrender_pixcvt_y8_rgba;
      } break;
    case 32: switch (srcimg->pixelsize) {
        case 1: if (srcimg->amask) return softrender_pixcvt_rgba_a1; return softrender_pixcvt_rgba_y1;
        case 8: if (srcimg->amask) return softrender_pixcvt_rgba_y1; return softrender_pixcvt_rgba_y8;
      } break;
  }
  return 0;
}

/* Generic pixel blender.
 * Blender receives the destination format for incoming pixels, they've already been thru a converter.
 */
 
typedef uint32_t (*softrender_blend_fn)(uint32_t prv,uint32_t src,struct softrender *softrender,const struct rawimg *img);

static uint32_t softrender_blend_rgba_zeroalpha(uint32_t prv,uint32_t src,struct softrender *softrender,const struct rawimg *img) {
  return src?src:prv;
}

static uint32_t softrender_blend_rgba_opaque(uint32_t prv,uint32_t src,struct softrender *softrender,const struct rawimg *img) {
  #if BYTE_ORDER==BIG_ENDIAN
    return src|0x000000ff;
  #else
    return src|0xff000000;
  #endif
}

static uint32_t softrender_blend_rgbx_rgba(uint32_t prv,uint32_t src,struct softrender *softrender,const struct rawimg *img) {
  #if BYTE_ORDER==BIG_ENDIAN
    uint8_t srca=src;
    if (!srca) return prv;
    if (srca==0xff) return src;
    uint8_t dsta=0xff-srca;
    uint8_t dstr=prv>>24,srcr=src>>24; dstr=(dstr*dsta+srcr*srca)>>8;
    uint8_t dstg=prv>>16,srcg=src>>16; dstg=(dstg*dsta+srcg*srca)>>8;
    uint8_t dstb=prv>> 8,srcb=src>> 8; dstb=(dstb*dsta+srcb*srca)>>8;
    return (dstr<<24)|(dstg<<16)|(dstb<<8)|0xff;
  #else
    uint8_t srca=src>>24;
    if (!srca) return prv;
    if (srca==0xff) return src;
    uint8_t dsta=0xff-srca;
    uint8_t dstr=prv    ,srcr=src    ; dstr=(dstr*dsta+srcr*srca)>>8;
    uint8_t dstg=prv>> 8,srcg=src>> 8; dstg=(dstg*dsta+srcg*srca)>>8;
    uint8_t dstb=prv>>16,srcb=src>>16; dstb=(dstb*dsta+srcb*srca)>>8;
    return dstr|(dstg<<8)|(dstb<<16)|0xff000000;
  #endif
}

//TODO Proper RGBA blending. For now, we will treat all (dst) as opaque.
#define softrender_blend_rgba_rgba softrender_blend_rgbx_rgba

static softrender_blend_fn softrender_get_blend(const struct rawimg *dstimg,const struct rawimg *srcimg,const struct softrender *softrender) {
  //TODO (softrender->alpha)
  //TODO (softrender->tint), is that our problem here?
  if (dstimg->pixelsize==srcimg->pixelsize) switch (dstimg->pixelsize) {
    case 1: break;
    case 8: break;
    case 32: {
        if (srcimg->encfmt==softrender_hint_zeroalpha) return softrender_blend_rgba_zeroalpha;
        if (dstimg->encfmt==softrender_hint_opaque) {
          if (srcimg->encfmt==softrender_hint_opaque) return 0;
          return softrender_blend_rgbx_rgba;
        }
        if (srcimg->encfmt==softrender_hint_opaque) return softrender_blend_rgba_opaque;
        return softrender_blend_rgba_rgba;
      } break;
  }
  return 0;
}

/* Draw rect 1-bit, with pre-clipped bounds.
 */
 
static void softrender_draw_rect_1(struct rawimg *dstimg,int x,int y,int w,int h,uint32_t pixel) {
  // Split into up to 3 columns, where the middle column is byte-aligned.
  if (x&7) {
    int subw=8-(x&7);
    if (subw>w) subw=w;
    uint8_t mask=(1<<subw)-1;
    mask<<=8-subw-(x&7);
    uint8_t *row=((uint8_t*)dstimg->v)+y*dstimg->stride+(x>>3);
    int yi=h;
    if (pixel) {
      for (;yi-->0;row+=dstimg->stride) (*row)|=mask;
    } else {
      for (;yi-->0;row+=dstimg->stride) (*row)&=!mask;
    }
    if ((w-=subw)<1) return;
    x+=subw;
  }
  if (w>=8) {
    int subw=w&~7;
    int bytec=subw>>3;
    uint8_t *row=((uint8_t*)dstimg->v)+y*dstimg->stride+(x>>3);
    int yi=h;
    if (pixel) {
      for (;yi-->0;row+=dstimg->stride) memset(row,0xff,bytec);
    } else {
      for (;yi-->0;row+=dstimg->stride) memset(row,0x00,bytec);
    }
    if ((w-=subw)<1) return;
    x+=subw;
  }
  if (w>0) {
    uint8_t mask=(1<<w)-1;
    mask<<=(8-w);
    uint8_t *row=((uint8_t*)dstimg->v)+y*dstimg->stride+(x>>3);
    int yi=h;
    if (pixel) {
      for (;yi-->0;row+=dstimg->stride) (*row)|=mask;
    } else {
      for (;yi-->0;row+=dstimg->stride) (*row)&=!mask;
    }
  }
}
 
static void softrender_draw_rect_blend_1(struct rawimg *dstimg,int x,int y,int w,int h,uint32_t pixel,uint8_t a) {
  if (a<0x80) return;
  softrender_draw_rect_1(dstimg,x,y,w,h,pixel);
}

/* Draw rect 8-bit.
 */
 
static void softrender_draw_rect_8(struct rawimg *dstimg,int x,int y,int w,int h,uint32_t pixel) {
  uint8_t *row=((uint8_t*)dstimg->v)+y*dstimg->stride+x;
  int yi=h;
  for (;h-->0;row+=dstimg->stride) memset(row,pixel,w);
}
 
static void softrender_draw_rect_blend_8(struct rawimg *dstimg,int x,int y,int w,int h,uint32_t pixel,uint8_t a) {
  uint8_t *row=((uint8_t*)dstimg->v)+y*dstimg->stride+x;
  int yi=h;
  for (;h-->0;row+=dstimg->stride) memset(row,pixel,w);//TODO blend
}

/* Draw rect 32-bit.
 */
 
static void softrender_draw_rect_32(struct softrender *softrender,struct rawimg *dstimg,int x,int y,int w,int h,uint32_t pixel) {
  uint8_t srca=pixel;
  if (srca==0xff) srca=softrender->alpha;
  else if (softrender->alpha!=0xff) srca=(srca*softrender->alpha)>>8;
  if (!srca) return;
  if (srca==0xff) {
    if (softrender->pxcvt) pixel=softrender->pxcvt(pixel);
    int wstride=dstimg->stride>>2;
    uint32_t *row=((uint32_t*)dstimg->v)+y*wstride+x;
    int yi=h;
    for (;yi-->0;row+=wstride) {
      uint32_t *dstp=row;
      int xi=w;
      for (;xi-->0;dstp++) *dstp=pixel;
    }
  } else {
    uint8_t dsta=0xff-srca;
    int srcr=pixel>>24,srcg=(pixel>>16)&0xff,srcb=(pixel>>8)&0xff;
    srcr*=srca;
    srcg*=srca;
    srcb*=srca;
    uint8_t *row=((uint8_t*)dstimg->v)+y*dstimg->stride+(x<<2);
    int yi=h;
    for (;yi-->0;row+=dstimg->stride) {
      uint8_t *dstp=row;
      int xi=w;
      for (;xi-->0;dstp+=4) {
        dstp[0]=(dstp[0]*dsta+srcr)>>8;
        dstp[1]=(dstp[1]*dsta+srcg)>>8;
        dstp[2]=(dstp[2]*dsta+srcb)>>8;
        dstp[3]=0xff;
      }
    }
  }
}

/* Draw flat rect, dispatch on format and feature selection.
 */

void softrender_draw_rect(struct softrender *softrender,int texid,int x,int y,int w,int h,uint32_t pixel) {
  if (!softrender->alpha) return;
  if ((texid<1)||(texid>softrender->texturec)) return;
  struct rawimg *rawimg=softrender->texturev[texid-1];
  if (!rawimg) return;
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>rawimg->w-w) w=rawimg->w-x;
  if (y>rawimg->h-h) h=rawimg->h-y;
  if ((w<1)||(h<1)) return;
  if (softrender->alpha==0xff) {
    switch (rawimg->pixelsize) {
      case 1: softrender_draw_rect_1(rawimg,x,y,w,h,pixel); break;
      case 8: softrender_draw_rect_8(rawimg,x,y,w,h,pixel); break;
      case 32: softrender_draw_rect_32(softrender,rawimg,x,y,w,h,pixel); break;
    }
  } else {
    switch (rawimg->pixelsize) {
      case 1: softrender_draw_rect_blend_1(rawimg,x,y,w,h,pixel,softrender->alpha); break;
      case 8: softrender_draw_rect_blend_8(rawimg,x,y,w,h,pixel,softrender->alpha); break;
      case 32: softrender_draw_rect_32(softrender,rawimg,x,y,w,h,pixel); break;
    }
  }
}

/* Clip output bounds.
 */
 
#define CLIPDST \
  if (dstx<0) { \
    if (dstxform&EGG_XFORM_SWAP) { \
      srch+=dstx; \
      if (!(srcxform&EGG_XFORM_YREV)) srcy-=dstx; \
    } else { \
      srcw+=dstx; \
      if (!(srcxform&EGG_XFORM_XREV)) srcx-=dstx; \
    } \
    dstw+=dstx; \
    dstx=0; \
  } \
  if (dsty<0) { \
    if (dstxform&EGG_XFORM_SWAP) { \
      srcw+=dsty; \
      if (!(srcxform&EGG_XFORM_XREV)) srcx-=dsty; \
    } else { \
      srch+=dsty; \
      if (!(srcxform&EGG_XFORM_YREV)) srcy-=dsty; \
    } \
    dsth+=dsty; \
    dsty=0; \
  } \
  if (dstx+dstw>dstimg->w) { \
    int clipc=dstx+dstw-dstimg->w; \
    if (dstxform&EGG_XFORM_SWAP) { \
      srch-=clipc; \
      if (srcxform&EGG_XFORM_YREV) srcy+=clipc; \
    } else { \
      srcw-=clipc; \
      if (srcxform&EGG_XFORM_XREV) srcx+=clipc; \
    } \
    dstw-=clipc; \
  } \
  if (dsty+dsth>dstimg->h) { \
    int clipc=dsty+dsth-dstimg->h; \
    if (dstxform&EGG_XFORM_SWAP) { \
      srcw-=clipc; \
      if (srcxform&EGG_XFORM_XREV) srcx+=clipc; \
    } else { \
      srch-=clipc; \
      if (srcxform&EGG_XFORM_YREV) srcy+=clipc; \
    } \
    dsth-=clipc; \
  }

/* Draw decal.
 */

void softrender_draw_decal(
  struct softrender *softrender,
  int dsttexid,int srctexid,
  int dstx,int dsty,
  int srcx,int srcy,
  int w,int h,
  int xform
) {
  if ((dsttexid<1)||(dsttexid>softrender->texturec)) return;
  if ((srctexid<1)||(srctexid>softrender->texturec)) return;
  struct rawimg *dstimg=softrender->texturev[dsttexid-1];
  struct rawimg *srcimg=softrender->texturev[srctexid-1];
  if (!dstimg||!srcimg) return;
  
  // Clip (dst). Assume that (src) is in bounds. It will fail fully when we initialize iterators, if not.
  int dstw=w,dsth=h,srcw=w,srch=h;
  uint8_t dstxform=0;
  uint8_t srcxform=xform;
  if (xform&EGG_XFORM_SWAP) {
    dstw=h;
    dsth=w;
    dstxform=EGG_XFORM_SWAP;
    srcxform&=~EGG_XFORM_SWAP;
  }
  CLIPDST
  
  struct rawimg_iterator dstiter,srciter;
  if (rawimg_iterate(&dstiter,dstimg,dstx,dsty,dstw,dsth,dstxform)<0) return;
  if (rawimg_iterate(&srciter,srcimg,srcx,srcy,srcw,srch,srcxform)<0) return;
  
  softrender_pixcvt_fn pixcvt=softrender_get_pixcvt(dstimg,srcimg,softrender);
  softrender_blend_fn blend=softrender_get_blend(dstimg,srcimg,softrender);
  if (pixcvt) {
    if (blend) {
      do {
        uint32_t pixel=rawimg_iterator_read(&srciter);
        pixel=pixcvt(pixel,softrender,dstimg,srcimg);
        pixel=blend(rawimg_iterator_read(&dstiter),pixel,softrender,dstimg);
        rawimg_iterator_write(&dstiter,pixel);
      } while (rawimg_iterator_next(&dstiter)&&rawimg_iterator_next(&srciter));
    } else {
      do {
        uint32_t pixel=rawimg_iterator_read(&srciter);
        pixel=pixcvt(pixel,softrender,dstimg,srcimg);
        rawimg_iterator_write(&dstiter,pixel);
      } while (rawimg_iterator_next(&dstiter)&&rawimg_iterator_next(&srciter));
    }
  } else {
    if (blend) {
      do {
        uint32_t pixel=rawimg_iterator_read(&srciter);
        pixel=blend(rawimg_iterator_read(&dstiter),pixel,softrender,dstimg);
        rawimg_iterator_write(&dstiter,pixel);
      } while (rawimg_iterator_next(&dstiter)&&rawimg_iterator_next(&srciter));
    } else {
      do {
        uint32_t pixel=rawimg_iterator_read(&srciter);
        rawimg_iterator_write(&dstiter,pixel);
      } while (rawimg_iterator_next(&dstiter)&&rawimg_iterator_next(&srciter));
    }
  }
}

/* Draw tiles.
 */

void softrender_draw_tile(
  struct softrender *softrender,
  int dsttexid,int srctexid,
  const struct egg_draw_tile *v,int c
) {
  if ((dsttexid<1)||(dsttexid>softrender->texturec)) return;
  if ((srctexid<1)||(srctexid>softrender->texturec)) return;
  struct rawimg *dstimg=softrender->texturev[dsttexid-1];
  struct rawimg *srcimg=softrender->texturev[srctexid-1];
  if (!dstimg||!srcimg) return;
  
  int colw=srcimg->w>>4;
  int rowh=srcimg->h>>4;
  int halfcolw=colw>>1;
  int halfrowh=rowh>>1;
  
  #define ITERATE(perpixel) { \
    for (;c-->0;v++) { \
      int srcx=(v->tileid&0x0f)*colw; \
      int srcy=(v->tileid>>4)*rowh; \
      int dstx=v->x-halfcolw; \
      int dsty=v->y-halfrowh; \
      int srcw=colw,srch=rowh,dstw=colw,dsth=rowh; \
      uint8_t dstxform=0; \
      uint8_t srcxform=v->xform; \
      if (v->xform&EGG_XFORM_SWAP) { \
        dstw=rowh; \
        dsth=colw; \
        dstxform=EGG_XFORM_SWAP; \
        srcxform&=~EGG_XFORM_SWAP; \
      } \
      CLIPDST \
      struct rawimg_iterator srciter,dstiter; \
      if (rawimg_iterate(&dstiter,dstimg,dstx,dsty,dstw,dsth,dstxform)<0) continue; \
      if (rawimg_iterate(&srciter,srcimg,srcx,srcy,srcw,srch,srcxform)<0) continue; \
      do { perpixel } while (rawimg_iterator_next(&dstiter)&&rawimg_iterator_next(&srciter)); \
    } \
  }
  
  softrender_pixcvt_fn pixcvt=softrender_get_pixcvt(dstimg,srcimg,softrender);
  softrender_blend_fn blend=softrender_get_blend(dstimg,srcimg,softrender);
  if (pixcvt) {
    if (blend) {
      ITERATE({
        uint32_t pixel=rawimg_iterator_read(&srciter);
        pixel=pixcvt(pixel,softrender,dstimg,srcimg);
        pixel=blend(rawimg_iterator_read(&dstiter),pixel,softrender,dstimg);
        rawimg_iterator_write(&dstiter,pixel);
      })
    } else {
      ITERATE({
        uint32_t pixel=rawimg_iterator_read(&srciter);
        pixel=pixcvt(pixel,softrender,dstimg,srcimg);
        rawimg_iterator_write(&dstiter,pixel);
      })
    }
  } else {
    if (blend) {
      ITERATE({
        uint32_t pixel=rawimg_iterator_read(&srciter);
        pixel=blend(rawimg_iterator_read(&dstiter),pixel,softrender,dstimg);
        rawimg_iterator_write(&dstiter,pixel);
      })
    } else {
      ITERATE({
        uint32_t pixel=rawimg_iterator_read(&srciter);
        rawimg_iterator_write(&dstiter,pixel);
      })
    }
  }
  #undef ITERATE
}

/* Framebuffer conversion.
 */
 
void softrender_fbcvt_shl8(uint32_t *src,int w,int h,int stridewords) {
  for (;h-->0;src+=stridewords) {
    uint32_t *p=src;
    int xi=w;
    for (;xi-->0;p++) *p=*p<<8;
  }
}

void softrender_fbcvt_shr8(uint32_t *src,int w,int h,int stridewords) {
  for (;h-->0;src+=stridewords) {
    uint32_t *p=src;
    int xi=w;
    for (;xi-->0;p++) *p=*p>>8;
  }
}

void softrender_fbcvt_swap02(uint32_t *src,int w,int h,int stridewords) {
  for (;h-->0;src+=stridewords) {
    uint32_t *p=src;
    int xi=w;
    for (;xi-->0;p++) *p=(*p&0x00ff00ff)|((*p&0xff000000)>>16)|((*p&0xff00)<<16);
  }
}

void softrender_fbcvt_swap13(uint32_t *src,int w,int h,int stridewords) {
  for (;h-->0;src+=stridewords) {
    uint32_t *p=src;
    int xi=w;
    for (;xi-->0;p++) *p=(*p&0xff00ff00)|((*p&0xff0000)>>16)|((*p&0xff)<<16);
  }
}

void softrender_fbcvt_swap(uint32_t *src,int w,int h,int stridewords) {
  for (;h-->0;src+=stridewords) {
    uint32_t *p=src;
    int xi=w;
    for (;xi-->0;p++) *p=(*p>>24)|((*p&0xff0000)>>8)|((*p&0xff00)<<8)|(*p<<24);
  }
}

uint32_t softrender_pxcvt_swap(uint32_t src) {
  return (src>>24)|((src&0xff0000)>>8)|((src&0xff00)<<8)|(src<<24);
}
