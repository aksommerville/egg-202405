#include "rawimg.h"
#include "opt/serial/serial.h"
#include <string.h>
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

/* rawimg, our private format.
 * The codec is trivial, that's expected since it's meant to be like the shortest serial path to our live object.
 ************************************************************************************/
 
static int rawimg_encode_rawimg(struct sr_encoder *encoder,const struct rawimg *rawimg) {

  // Validate carefully.
  if ((rawimg->w<1)||(rawimg->w>0x7fff)) return -1;
  if ((rawimg->h<1)||(rawimg->h>0x7fff)) return -1;
  int dststride=rawimg_minimum_stride(rawimg->w,rawimg->pixelsize);
  if ((dststride<1)||(dststride>rawimg->stride)) return -1;
  if (dststride>INT_MAX/rawimg->h) return -1;
  if ((rawimg->ctabc<0)||(rawimg->ctabc>0xffff)||(rawimg->ctabc&&!rawimg->ctab)) return -1;

  // 32-byte header, ouch.
  if (sr_encode_raw(encoder,"\x00raw",4)<0) return -1;
  if (sr_encode_intbe(encoder,rawimg->w,2)<0) return -1;
  if (sr_encode_intbe(encoder,rawimg->h,2)<0) return -1;
  if (sr_encode_raw(encoder,&rawimg->rmask,4)<0) return -1; // sic raw not intbe: This must match layout of the actual pixels.
  if (sr_encode_raw(encoder,&rawimg->gmask,4)<0) return -1;
  if (sr_encode_raw(encoder,&rawimg->bmask,4)<0) return -1;
  if (sr_encode_raw(encoder,&rawimg->amask,4)<0) return -1;
  if (sr_encode_raw(encoder,rawimg->chorder,4)<0) return -1;
  if (sr_encode_u8(encoder,rawimg->bitorder)<0) return -1;
  if (sr_encode_u8(encoder,rawimg->pixelsize)<0) return -1;
  if (sr_encode_intbe(encoder,rawimg->ctabc,2)<0) return -1;
  
  // Pixels.
  if (dststride==rawimg->stride) {
    if (sr_encode_raw(encoder,rawimg->v,dststride*rawimg->h)<0) return -1;
  } else {
    const uint8_t *srcrow=rawimg->v;
    int yi=rawimg->h;
    for (;yi-->0;srcrow+=rawimg->stride) {
      if (sr_encode_raw(encoder,srcrow,dststride)<0) return -1;
    }
  }
  
  // Color table.
  if (sr_encode_raw(encoder,rawimg->ctab,rawimg->ctabc<<2)<0) return -1;

  return 0;
}

static struct rawimg *rawimg_decode_rawimg(const uint8_t *src,int srcc) {

  // Read and validate header.
  const int hdrlen=32;
  if (srcc<hdrlen) return 0;
  if (memcmp(src,"\x00raw",4)) return 0;
  int w=(src[4]<<8)|src[5];
  int h=(src[6]<<8)|src[7];
  // rmask,gmask,bmask,amask,chorder,bitorder: Anything goes.
  int pixelsize=src[0x1d];
  int ctabc=(src[0x1e]<<8)|src[0x1f];
  int stride=rawimg_minimum_stride(w,pixelsize);
  if (stride<1) return 0;
  if ((h<1)||(h>0x7fff)) return 0;
  if (stride>INT_MAX/h) return 0;
  int pixelslen=stride*h;
  int ctablen=ctabc<<2;
  if (hdrlen>srcc-pixelslen) return 0;
  if (hdrlen+pixelslen>srcc-ctablen) return 0;
  
  // Allocate image, then copy pixels and ctab verbatim.
  struct rawimg *rawimg=rawimg_new_alloc(w,h,pixelsize);
  if (!rawimg) return 0;
  rawimg->encfmt="rawimg";
  memcpy(rawimg->v,src+hdrlen,pixelslen);
  if (rawimg_set_ctab(rawimg,src+hdrlen+pixelslen,ctabc)<0) {
    rawimg_del(rawimg);
    return 0;
  }
  
  // Back to the header to fill in pixel format stuff.
  memcpy(&rawimg->rmask,src+8,4);
  memcpy(&rawimg->gmask,src+12,4);
  memcpy(&rawimg->bmask,src+16,4);
  memcpy(&rawimg->amask,src+20,4);
  memcpy(rawimg->chorder,src+24,4);
  rawimg->bitorder=src[28];
  
  // Finally, when pixelsize is 8 or 16, shuffle masks to orient them at the low end.
  // The idea of these masks is you write them as pixels are laid out on your machine, 
  // and anyone else can grok the layout from that, regardless of byte order.
  // Which would work fine if it was a fixed length, but it's not.
  if (pixelsize==8) {
    // Rare to use channel masks with 8-bit pixels, but not unheard of.
    if ((rawimg->rmask&0xff000000)&&!(rawimg->rmask&0x000000ff)) rawimg->rmask>>=24;
    if ((rawimg->gmask&0xff000000)&&!(rawimg->gmask&0x000000ff)) rawimg->gmask>>=24;
    if ((rawimg->bmask&0xff000000)&&!(rawimg->bmask&0x000000ff)) rawimg->bmask>>=24;
    if ((rawimg->amask&0xff000000)&&!(rawimg->amask&0x000000ff)) rawimg->amask>>=24;
  } else if (pixelsize==16) {
    // Much more common than 8.
    if ((rawimg->rmask&0xffff0000)&&!(rawimg->rmask&0x0000ffff)) rawimg->rmask>>=16;
    if ((rawimg->gmask&0xffff0000)&&!(rawimg->gmask&0x0000ffff)) rawimg->gmask>>=16;
    if ((rawimg->bmask&0xffff0000)&&!(rawimg->bmask&0x0000ffff)) rawimg->bmask>>=16;
    if ((rawimg->amask&0xffff0000)&&!(rawimg->amask&0x0000ffff)) rawimg->amask>>=16;
  }

  return rawimg;
}

/* PNG
 **************************************************************************/
 
#if USE_png

#include "opt/png/png.h"

// (dst) is blank initially. OK to allocate pixels or borrow from (src), caller figures it out.
static int png_image_from_rawimg(struct png_image *dst,const struct rawimg *src) {

  switch (src->pixelsize) {
    case 1: case 2: case 4: case 8: { // Index or Gray.
        if (src->bitorder!='>') return -1;
        dst->depth=src->pixelsize;
        if (src->ctabc>=(1<<src->pixelsize)) dst->colortype=3;
        else dst->colortype=0;
      } break;
    case 16: { // YA or Gray.
        if ((src->chorder[0]=='Y')&&!src->chorder[1]) { // only big Y; 16-bit channel so byte order matters
          dst->depth=16;
          dst->colortype=0;
        } else if (src->rmask==0xffff) { // masks are ambiguous about byte order. give it the benefit of the doubt.
          dst->depth=16;
          dst->colortype=0;
        } else if (!memcmp(src->chorder,"YA",2)||!memcmp(src->chorder,"ya",2)) {
          dst->depth=8;
          dst->colortype=4;
        #if BYTE_ORDER==BIG_ENDIAN
        } else if ((src->rmask==0xff00)&&(src->amask==0x00ff)) {
          dst->depth=8;
          dst->colortype=4;
        #else
        } else if ((src->rmask==0x00ff)&&(src->amask==0xff00)) {
          dst->depth=8;
          dst->colortype=4;
        #endif
        }
      } break;
    case 24: { // RGB
        if (!memcmp(src->chorder,"RGB",3)||!memcmp(src->chorder,"rgb",3)) {
          dst->depth=8;
          dst->colortype=2;
        }
      } break;
    case 32: { // RGBA or YA
        if ((src->chorder[0]=='Y')&&(src->chorder[1]=='A')) {
          dst->depth=16;
          dst->colortype=4;
        } else if (!memcmp(src->chorder,"RGBA",4)||!memcmp(src->chorder,"rgba",4)) {
          dst->depth=8;
          dst->colortype=6;
        #if BYTE_ORDER==BIG_ENDIAN
        } else if ((src->rmask==0xffff0000)&&(src->amask==0x0000ffff)) {
          dst->depth=16;
          dst->colortype=4;
        } else if ((src->rmask==0xff000000)&&(src->gmask==0x00ff0000)&&(src->bmask==0x0000ff00)&&(src->amask==0x000000ff)) {
          dst->depth=8;
          dst->colortype=6;
        #else
        } else if ((src->rmask==0x0000ffff)&&(src->amask==0xffff0000)) {
          dst->depth=16;
          dst->colortype=4;
        } else if ((src->rmask==0x000000ff)&&(src->gmask==0x0000ff00)&&(src->bmask==0x00ff0000)&&(src->amask==0xff000000)) {
          dst->depth=8;
          dst->colortype=6;
        #endif
        }
      } break;
    case 48: { // RGB, big-endian only
        if (!memcmp(src->chorder,"RGB",3)) {
          dst->depth=16;
          dst->colortype=2;
        }
      } break;
    case 64: { // RGBA, big-endian only
        if (!memcmp(src->chorder,"RGBA",4)) {
          dst->depth=16;
          dst->colortype=6;
        }
      } break;
  }
  
  // If it didn't match, copy and force to RGBA.
  if (!dst->depth) {
    struct rawimg *tmp=rawimg_new_copy(src);
    if (!tmp) return -1;
    if (rawimg_force_rgba(tmp)<0) {
      rawimg_del(tmp);
      return -1;
    }
    dst->v=tmp->v;
    tmp->v=0;
    dst->stride=tmp->stride;
    rawimg_del(tmp);
    dst->depth=8;
    dst->colortype=6;
    dst->pixelsize=32;
    
  } else {
    dst->v=src->v;
    dst->stride=src->stride;
    dst->pixelsize=src->pixelsize;
  }
  dst->w=src->w;
  dst->h=src->h;
    
  // Preserve color table only for INDEX output.
  // PNG allows a color table as "suggestion quantization"; I don't think that's valuable.
  // And we might accidentally copy color tables that aren't needed anymore.
  if (src->ctabc&&(dst->colortype==3)) {
    uint8_t rgb[768];
    uint8_t a[256];
    uint8_t *rgbp=rgb,*ap=a;
    const uint8_t *ctabp=src->ctab;
    int i=src->ctabc;
    if (i>256) i=256;
    int rgbc=i,ac=i;
    rgbc*=3;
    for (;i-->0;rgbp+=3,ap++,ctabp+=4) {
      rgbp[0]=ctabp[0];
      rgbp[1]=ctabp[1];
      rgbp[2]=ctabp[2];
      *ap=ctabp[3];
    }
    while (ac&&(a[ac-1]==0xff)) ac--;
    if (png_image_add_chunk(dst,"PLTE",rgb,rgbc)<0) return -1;
    if (ac) {
      if (png_image_add_chunk(dst,"tRNS",a,ac)<0) return -1;
    }
  }
  
  return 0;
}

static int rawimg_encode_png(struct sr_encoder *encoder,const struct rawimg *rawimg) {
  struct png_image pngimage={0};
  int err=-1;
  if (png_image_from_rawimg(&pngimage,rawimg)>=0) {
    err=png_encode(encoder,&pngimage);
  }
  if (pngimage.v==rawimg->v) pngimage.v=0;
  png_image_cleanup(&pngimage);
  return err;
}

static int rawimg_acquire_png_ctab(struct rawimg *rawimg,const struct png_image *pngimage) {
  const uint8_t *plte=0,*trns=0;
  int pltec=0,trnsc=0;
  const struct png_chunk *chunk=pngimage->chunkv;
  int i=pngimage->chunkc;
  for (;i-->0;chunk++) {
    if (!memcmp(chunk->chunktype,"PLTE",4)) {
      plte=chunk->v;
      pltec=chunk->c;
    } else if (!memcmp(chunk->chunktype,"tRNS",4)) {
      trns=chunk->v;
      trnsc=chunk->c;
    }
  }
  if (!pltec) return -1;
  int ctabc=pltec/3;
  if (ctabc>256) ctabc=256;
  uint8_t tmp[1024];
  uint8_t *dstp=tmp;
  for (i=0;i<ctabc;i++) {
    *(dstp++)=*(plte++);
    *(dstp++)=*(plte++);
    *(dstp++)=*(plte++);
    if (trnsc) {
      *(dstp++)=*(trns++);
      trnsc--;
    } else {
      *(dstp++)=0xff;
    }
  }
  return rawimg_set_ctab(rawimg,tmp,ctabc);
}

static struct rawimg *rawimg_decode_png(const uint8_t *src,int srcc) {
  struct png_image *pngimage=png_decode(src,srcc);
  if (!pngimage) return 0;
  
  struct rawimg *rawimg=rawimg_new_handoff(pngimage->v,pngimage->w,pngimage->h,pngimage->stride,pngimage->pixelsize);
  if (!rawimg) {
    png_image_del(pngimage);
    return 0;
  }
  pngimage->v=0; // handed off
  if (pngimage->colortype==3) {
    rawimg_acquire_png_ctab(rawimg,pngimage);
  }
  
  rawimg->bitorder='>';
  switch (pngimage->colortype) {
    case 0: memcpy(rawimg->chorder,"Y\0\0\0",4); break;
    case 2: memcpy(rawimg->chorder,"RGB\0",4); break;
    case 3: break;
    case 4: {
        memcpy(rawimg->chorder,"YA\0\0",4);
        if (pngimage->depth==8) {
          uint8_t __attribute__((aligned(4))) layout[2]={0xff,0x00};
          rawimg->rmask=rawimg->gmask=rawimg->bmask=*(uint16_t*)layout;
          rawimg->amask=(~rawimg->rmask)&0xffff;
        }
      } break;
    case 6: {
        memcpy(rawimg->chorder,"RGBA",4);
        if (pngimage->depth==8) {
          uint8_t __attribute__((aligned(4))) layout[4]={0xff,0x00,0x00,0x00};
          rawimg->rmask=*(uint32_t*)layout;
          rawimg->gmask=(rawimg->rmask>>8)|(rawimg->rmask<<8); // one of those will be zero, get it?
          rawimg->bmask=(rawimg->rmask>>16)|(rawimg->rmask<<16);
          rawimg->amask=(rawimg->rmask>>24)|(rawimg->rmask<<24);
        }
      } break;
  }
  
  png_image_del(pngimage);
  return rawimg;
}

#endif

/* ico
 ******************************************************************************/
 
#if USE_ico

#include "opt/ico/ico.h"

static int rawimg_encode_ico(struct sr_encoder *encoder,const struct rawimg *rawimg) {
  struct rawimg *killme=0;
  if (!rawimg_is_rgba(rawimg)) {
    if (!(killme=rawimg_new_copy(rawimg))) return -1;
    if (rawimg_force_rgba(killme)<0) {
      rawimg_del(killme);
      return -1;
    }
    rawimg=killme;
  }
  struct ico_file file={0};
  struct ico_image *image=ico_file_add_image(&file,rawimg->w,rawimg->h);
  if (!image) {
    ico_file_cleanup(&file);
    rawimg_del(killme);
    return -1;
  }
  const uint8_t *srcrow=rawimg->v;
  uint8_t *dstrow=image->v;
  int yi=image->h;
  for (;yi-->0;srcrow+=rawimg->stride,dstrow+=image->stride) {
    memcpy(dstrow,srcrow,image->stride);
  }
  int err=ico_encode(encoder,&file);
  ico_file_cleanup(&file);
  rawimg_del(killme);
  return err;
}

static struct rawimg *rawimg_decode_ico(const uint8_t *src,int srcc) {
  struct ico_file *file=ico_decode(src,srcc);
  if (!file) return 0;
  struct ico_image *image=ico_file_get_image(file,INT_MAX,INT_MAX,1);
  if (!image) {
    ico_file_del(file);
    return 0;
  }
  fprintf(stderr,"ico image: %dx%d\n",image->w,image->h);
  struct rawimg *rawimg=rawimg_new_handoff(image->v,image->w,image->h,image->stride,image->pixelsize);
  if (!rawimg) {
    ico_file_del(file);
    return 0;
  }
  image->v=0; // handed off
  
  //TODO !!! Need color table.
  
  if (image->pixelsize==32) {
    memcpy(rawimg->chorder,"RGBA",4);
    uint8_t __attribute__((aligned(4))) layout[4]={0xff,0x00,0x00,0x00};
    rawimg->rmask=*(uint32_t*)layout;
    rawimg->gmask=(rawimg->rmask>>8)|(rawimg->rmask<<8); // one of those will be zero, get it?
    rawimg->bmask=(rawimg->rmask>>16)|(rawimg->rmask<<16);
    rawimg->amask=(rawimg->rmask>>24)|(rawimg->rmask<<24);
  } else {
    // TODO Need ico to pass along more details from underlying PNG and BMP, so we can populate format.
  }

  ico_file_del(file);
  return rawimg;
}

#endif

/* QOI
 ***************************************************************************/
 
#if USE_qoi

#include "opt/qoi/qoi.h"

static int rawimg_encode_qoi(struct sr_encoder *encoder,const struct rawimg *rawimg) {
  struct rawimg *killme=0;
  if (!rawimg_is_rgba(rawimg)) {
    if (!(killme=rawimg_new_copy(rawimg))) return -1;
    if (rawimg_force_rgba(killme)<0) {
      rawimg_del(killme);
      return -1;
    }
    rawimg=killme;
  }
  if (rawimg->stride!=rawimg->w<<2) {
    rawimg_del(killme);
    return -1;
  }
  struct qoi_image qoi={
    .v=rawimg->v,
    .w=rawimg->w,
    .h=rawimg->h,
  };
  int err=qoi_encode(encoder,&qoi);
  rawimg_del(killme);
  return err;
}

static struct rawimg *rawimg_decode_qoi(const uint8_t *src,int srcc) {
  struct qoi_image *qoi=qoi_decode(src,srcc);
  if (!qoi) return 0;
  struct rawimg *rawimg=rawimg_new_handoff(qoi->v,qoi->w,qoi->h,qoi->w<<2,32);
  if (!rawimg) {
    qoi_image_del(qoi);
    return 0;
  }
  qoi->v=0; // handed off
  qoi_image_del(qoi);
  
  memcpy(rawimg->chorder,"RGBA",4);
  uint8_t __attribute__((aligned(4))) layout[4]={0xff,0x00,0x00,0x00};
  rawimg->rmask=*(uint32_t*)layout;
  rawimg->gmask=(rawimg->rmask>>8)|(rawimg->rmask<<8); // one of those will be zero, get it?
  rawimg->bmask=(rawimg->rmask>>16)|(rawimg->rmask<<16);
  rawimg->amask=(rawimg->rmask>>24)|(rawimg->rmask<<24);
  
  return rawimg;
}

#endif

/* rlead
 **************************************************************************/
 
#if USE_rlead

#include "opt/rlead/rlead.h"

static int rawimg_encode_rlead(struct sr_encoder *encoder,const struct rawimg *rawimg) {
  struct rawimg *killme=0;
  if (rawimg_is_y1(rawimg)<2) {
    if (!(killme=rawimg_new_copy(rawimg))) return -1;
    if (rawimg_force_y1(killme)<0) {
      rawimg_del(killme);
      return -1;
    }
    rawimg=killme;
  }
  struct rlead_image rlead={
    .v=rawimg->v,
    .w=rawimg->w,
    .h=rawimg->h,
    .stride=rawimg->stride,
  };
  int err=rlead_encode(encoder,&rlead);
  rawimg_del(killme);
  return err;
}

static struct rawimg *rawimg_decode_rlead(const uint8_t *src,int srcc) {
  struct rlead_image *rlead=rlead_decode(src,srcc);
  if (!rlead) return 0;
  struct rawimg *rawimg=rawimg_new_handoff(rlead->v,rlead->w,rlead->h,rlead->stride,1);
  if (!rawimg) {
    rlead_image_del(rlead);
    return 0;
  }
  rlead->v=0; // handed off
  rlead_image_del(rlead);
  
  rawimg->bitorder='>';
  
  return rawimg;
}

#endif

/* JPEG
 ***************************************************************************/
 
#if USE_jpeg

#include "opt/jpeg/jpeg.h"

static int rawimg_encode_jpeg(struct sr_encoder *encoder,const struct rawimg *rawimg) {
  struct rawimg *killme=0;
  if (!rawimg_is_rgb(rawimg)&&!rawimg_is_y8(rawimg)) {
    if (!(killme=rawimg_new_copy(rawimg))||(rawimg_force_rgb(killme)<0)) return -1;
    rawimg=killme;
  }
  struct jpeg_image jpeg={
    .v=rawimg->v,
    .w=rawimg->w,
    .h=rawimg->h,
    .stride=rawimg->stride,
    .chanc=rawimg->pixelsize>>3,
  };
  int err=jpeg_encode(encoder,&jpeg);
  rawimg_del(killme);
  return err;
}

static struct rawimg *rawimg_decode_jpeg(const uint8_t *src,int srcc) {
  struct jpeg_image *jpeg=jpeg_decode(src,srcc);
  if (!jpeg) return 0;
  int pixelsize=jpeg->chanc*8;
  struct rawimg *rawimg=rawimg_new_handoff(jpeg->v,jpeg->w,jpeg->h,jpeg->stride,pixelsize);
  if (!rawimg) {
    jpeg_image_del(jpeg);
    return 0;
  }
  jpeg->v=0; // handed off
  switch (jpeg->chanc) {
    case 1: {
        memcpy(rawimg->chorder,"y\0\0\0",4);
        rawimg->rmask=rawimg->gmask=rawimg->bmask=0xff;
        rawimg->amask=0;
      } break;
    case 3: {
        memcpy(rawimg->chorder,"rgb\0",4);
        rawimg->rmask=rawimg->gmask=rawimg->bmask=rawimg->amask=0;
      } break;
  }
  jpeg_image_del(jpeg);
  return rawimg;
}

#endif

/* BMP
 *************************************************************************/
 
#if USE_bmp

#include "opt/bmp/bmp.h"

static int rawimg_encode_bmp(struct sr_encoder *encoder,const struct rawimg *rawimg) {
  struct rawimg *killme=0;
  
  // I think BMP does not support grayscale. Convert those to RGBA.
  if ((rawimg->chorder[0]=='y')||(rawimg->chorder[0]=='Y')) {
    if (!(killme=rawimg_new_copy(rawimg))||(rawimg_force_rgba(killme)<0)) {
      rawimg_del(killme);
      return -1;
    }
    rawimg=killme;
    
  // Pixel size must be in 1..32. Again, RGBA if not.
  } else if (rawimg->pixelsize>32) {
    if (!(killme=rawimg_new_copy(rawimg))||(rawimg_force_rgba(killme)<0)) {
      rawimg_del(killme);
      return -1;
    }
    rawimg=killme;
    
  // 24-bit pixels evidently need to be BGR, and masks are ignored.
  } else if ((rawimg->pixelsize==24)&&((rawimg->chorder[0]=='R')||(rawimg->chorder[0]=='r'))) {
    if (!(killme=rawimg_new_copy(rawimg))) return -1;
    uint8_t *row=killme->v;
    int yi=killme->h;
    for (;yi-->0;row+=killme->stride) {
      uint8_t *p=row;
      int xi=killme->w;
      for (;xi-->0;p+=3) {
        uint8_t tmp=p[0];
        p[0]=p[2];
        p[2]=tmp;
      }
    }
    rawimg=killme;
    
  // TODO Other conversions before BMP encode?
  }
  
  struct bmp_image bmp={
    .v=rawimg->v,
    .w=rawimg->w,
    .h=rawimg->h,
    .stride=rawimg->stride,
    .pixelsize=rawimg->pixelsize,
    .ctab=rawimg->ctab,
    .ctabc=rawimg->ctabc,
    .rmask=rawimg->rmask,
    .gmask=rawimg->gmask,
    .bmask=rawimg->bmask,
    .amask=rawimg->amask,
  };
  int err=bmp_encode(encoder,&bmp,0);
  rawimg_del(killme);
  return err;
}

static struct rawimg *rawimg_decode_bmp(const uint8_t *src,int srcc) {
  struct bmp_image *bmp=bmp_decode(src,srcc);
  if (!bmp) return 0;
  struct rawimg *rawimg=rawimg_new_handoff(bmp->v,bmp->w,bmp->h,bmp->stride,bmp->pixelsize);
  if (!rawimg) {
    bmp_image_del(bmp);
    return 0;
  }
  bmp->v=0; // handed off
  if (bmp->ctab) {
    rawimg->ctab=bmp->ctab;
    rawimg->ctabc=bmp->ctabc;
    bmp->ctab=0;
    bmp->ctabc=0;
  }
  
  rawimg->rmask=bmp->rmask;
  rawimg->gmask=bmp->gmask;
  rawimg->bmask=bmp->bmask;
  rawimg->amask=bmp->amask;
  rawimg->bitorder='>';
  if (rawimg->pixelsize==24) {
    memcpy(rawimg->chorder,"BGR\0",4);
  }
  
  bmp_image_del(bmp);
  return rawimg;
}

#endif

/* GIF
 ***************************************************************************/
 
#if USE_gif

#include "opt/gif/gif.h"

static int rawimg_encode_gif(struct sr_encoder *encoder,const struct rawimg *rawimg) {
  struct rawimg *killme=0;
  if (!rawimg_is_rgba(rawimg)) {
    if (!(killme=rawimg_new_copy(rawimg))) return -1;
    if (rawimg_force_rgba(killme)) {
      rawimg_del(killme);
      return -1;
    }
    rawimg=killme;
  }
  struct gif_image gif={
    .v=rawimg->v,
    .w=rawimg->w,
    .h=rawimg->h,
    .stride=rawimg->stride,
  };
  int err=gif_encode(encoder,&gif);
  rawimg_del(killme);
  return err;
}

static struct rawimg *rawimg_decode_gif(const uint8_t *src,int srcc) {
  struct gif_image *gif=gif_decode(src,srcc);
  if (!gif) return 0;
  struct rawimg *rawimg=rawimg_new_handoff(gif->v,gif->w,gif->h,gif->stride,32);
  if (!rawimg) {
    gif_image_del(gif);
    return 0;
  }
  gif->v=0; // handed off
  gif_image_del(gif);
  
  memcpy(rawimg->chorder,"RGBA",4);
  uint8_t __attribute__((aligned(4))) layout[4]={0xff,0x00,0x00,0x00};
  rawimg->rmask=*(uint32_t*)layout;
  rawimg->gmask=(rawimg->rmask>>8)|(rawimg->rmask<<8); // one of those will be zero, get it?
  rawimg->bmask=(rawimg->rmask>>16)|(rawimg->rmask<<16);
  rawimg->amask=(rawimg->rmask>>24)|(rawimg->rmask<<24);
  
  return rawimg;
}

#endif

/* Format detection and dispatch.
 ********************************************************************************/
 
int rawimg_encode(struct sr_encoder *encoder,const struct rawimg *rawimg) {
  if (!encoder||!rawimg) return -1;
  if (!rawimg->encfmt||!rawimg->encfmt[0]) return rawimg_encode_rawimg(encoder,rawimg);
  if (!strcmp(rawimg->encfmt,"rawimg")) return rawimg_encode_rawimg(encoder,rawimg);
  #if USE_png
    if (!strcmp(rawimg->encfmt,"png")) return rawimg_encode_png(encoder,rawimg);
  #endif
  #if USE_ico
    if (!strcmp(rawimg->encfmt,"ico")) return rawimg_encode_ico(encoder,rawimg);
  #endif
  #if USE_qoi
    if (!strcmp(rawimg->encfmt,"qoi")) return rawimg_encode_qoi(encoder,rawimg);
  #endif
  #if USE_rlead
    if (!strcmp(rawimg->encfmt,"rlead")) return rawimg_encode_rlead(encoder,rawimg);
  #endif
  #if USE_jpeg
    if (!strcmp(rawimg->encfmt,"jpeg")) return rawimg_encode_jpeg(encoder,rawimg);
  #endif
  #if USE_bmp
    if (!strcmp(rawimg->encfmt,"bmp")) return rawimg_encode_bmp(encoder,rawimg);
  #endif
  #if USE_gif
    if (!strcmp(rawimg->encfmt,"gif")) return rawimg_encode_gif(encoder,rawimg);
  #endif
  return -1; // Unknown encoding requested.
}
 
struct rawimg *rawimg_decode(const void *src,int srcc) {
  if (!src) return 0;
  const char *encfmt=rawimg_detect_format(src,srcc);
  if (!encfmt) return 0;
  if (!strcmp(encfmt,"rawimg")) return rawimg_decode_rawimg(src,srcc);
  #if USE_png
    if (!strcmp(encfmt,"png")) return rawimg_decode_png(src,srcc);
  #endif
  #if USE_ico
    if (!strcmp(encfmt,"ico")) return rawimg_decode_ico(src,srcc);
  #endif
  #if USE_qoi
    if (!strcmp(encfmt,"qoi")) return rawimg_decode_qoi(src,srcc);
  #endif
  #if USE_rlead
    if (!strcmp(encfmt,"rlead")) return rawimg_decode_rlead(src,srcc);
  #endif
  #if USE_jpeg
    if (!strcmp(encfmt,"jpeg")) return rawimg_decode_jpeg(src,srcc);
  #endif
  #if USE_bmp
    if (!strcmp(encfmt,"bmp")) return rawimg_decode_bmp(src,srcc);
  #endif
  #if USE_gif
    if (!strcmp(encfmt,"gif")) return rawimg_decode_gif(src,srcc);
  #endif
  return 0;
}

const char *rawimg_supported_format_by_index(int p) {
  if (p<0) return 0;
  if (!p--) return "rawimg";
  #if USE_png
    if (!p--) return "png";
  #endif
  #if USE_ico
    if (!p--) return "ico";
  #endif
  #if USE_qoi
    if (!p--) return "qoi";
  #endif
  #if USE_rlead
    if (!p--) return "rlead";
  #endif
  #if USE_jpeg
    if (!p--) return "jpeg";
  #endif
  #if USE_bmp
    if (!p--) return "bmp";
  #endif
  #if USE_gif
    if (!p--) return "gif";
  #endif
  return 0;
}

const char *rawimg_detect_format(const void *src,int srcc) {
  if (!src) return 0;
  if ((srcc>=4)&&!memcmp(src,"\x00raw",4)) return "rawimg";
  if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return "png";
  if ((srcc>=4)&&!memcmp(src,"\0\0\1\0",4)) return "ico";
  if ((srcc>=4)&&!memcmp(src,"qoif",4)) return "qoi";
  if ((srcc>=2)&&!memcmp(src,"\xbb\xad",2)) return "rlead";
  if ((srcc>=2)&&!memcmp(src,"BM",2)) return "bmp";
  if ((srcc>=3)&&!memcmp(src,"GIF",3)) return "gif";
  // JPEG has some very silly ideas about file signatures:
  if ((srcc>=3)&&!memcmp(src,"\xff\xd8\xff",3)) return "jpeg";
  if ((srcc>=3)&&!memcmp(src,"\xff\x4f\xff",3)) return "jpeg";
  if ((srcc>=12)&&!memcmp(src,"\x00\x00\x00\x0C\x6A\x50\x20\x20\x0D\x0A\x87\x0A",12)) return "jpeg";
  return 0;
}
