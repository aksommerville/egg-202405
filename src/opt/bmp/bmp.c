#include "bmp.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>

/* Cleanup.
 */
 
void bmp_image_cleanup(struct bmp_image *image) {
  if (!image) return;
  if (image->v) free(image->v);
  if (image->ctab) free(image->ctab);
}

void bmp_image_del(struct bmp_image *image) {
  if (!image) return;
  bmp_image_cleanup(image);
  free(image);
}

/* Encode.
 */

int bmp_encode(struct sr_encoder *dst,const struct bmp_image *image,int skip_file_header) {
  int dstc0=dst->c;
  if (!skip_file_header) {
    if (sr_encode_raw(dst,"BM\0\0\0\0\0\0\0\0\0\0\0\0",14)<0) return -1;
  }
  
  if (sr_encode_intle(dst,124,4)<0) return -1;
  if (sr_encode_intle(dst,image->w,4)<0) return -1;
  if (sr_encode_intle(dst,-image->h,4)<0) return -1;
  if (sr_encode_intle(dst,1,2)<0) return -1; // planec
  if (sr_encode_intle(dst,image->pixelsize,2)<0) return -1;
  if (sr_encode_intle(dst,3,4)<0) return -1; // compression
  if (sr_encode_intle(dst,image->stride*image->h,4)<0) return -1;
  if (sr_encode_raw(dst,"\0\0\0\0\0\0\0\0",8)<0) return -1; // resolution
  if (sr_encode_intle(dst,image->ctabc,4)<0) return -1;
  if (sr_encode_intle(dst,0,4)<0) return -1; // important color count. zero, none, these colors all suck.
  if (sr_encode_raw(dst,&image->rmask,4)<0) return -1;
  if (sr_encode_raw(dst,&image->gmask,4)<0) return -1;
  if (sr_encode_raw(dst,&image->bmask,4)<0) return -1;
  if (sr_encode_raw(dst,&image->amask,4)<0) return -1;
  if (sr_encode_zero(dst,68)<0) return -1;
  
  /* Color table for BMP should be (B,G,R,A). Unclear whether A gets used or is a dummy.
   */
  if (image->ctabc>0) {
    const uint8_t *src=image->ctab;
    int i=image->ctabc;
    for (;i-->0;src+=4) {
      if (sr_encode_u8(dst,src[2])<0) return -1;
      if (sr_encode_u8(dst,src[1])<0) return -1;
      if (sr_encode_u8(dst,src[0])<0) return -1;
      if (sr_encode_u8(dst,src[3])<0) return -1;
    }
  }
  
  int lensofar=dst->c-dstc0;
  if (lensofar&3) {
    if (sr_encode_raw(dst,"\0\0\0\0",4-(lensofar&3))<0) return -1;
  }
  int pstart=dst->c-dstc0;
  // Pad stride to 4 bytes if necessary.
  if (image->stride&3) {
    int dststride=(image->stride+3)&~3;
    const uint8_t *row=image->v;
    int yi=image->h;
    for (;yi-->0;row+=image->stride) {
      if (sr_encode_raw(dst,row,image->stride)<0) return -1;
      if (sr_encode_zero(dst,dststride-image->stride)<0) return -1;
    }
  } else {
    if (sr_encode_raw(dst,image->v,image->stride*image->h)<0) return -1;
  }
  int flen=dst->c-dstc0;
  
  if (!skip_file_header) {
    uint8_t *tmp=(uint8_t*)dst->v+dstc0;
    tmp[2]=flen;
    tmp[3]=flen>>8;
    tmp[4]=flen>>16;
    tmp[5]=flen>>24;
    tmp[10]=pstart;
    tmp[11]=pstart>>8;
    tmp[12]=pstart>>16;
    tmp[13]=pstart>>24;
  }
  
  return 0;
}

/* Decode header, several options.
 */
 
static int bmp_decode_BITMAPCOREHEADER(struct bmp_image *image,const uint8_t *src,int srcc) {
  if (srcc<12) return -1;
  image->w=src[4]|(src[5]<<8);
  image->h=src[6]|(src[7]<<8);
  int planec=src[8]|(src[9]<<8);
  image->pixelsize=src[10]|(src[11]<<8);
  if (planec!=1) return -1;
  if (image->w&0x8000) image->w|=~0xffff;
  if (image->h&0x8000) image->h|=~0xffff;
  return 0;
}

static int bmp_decode_BITMAPINFOHEADER(struct bmp_image *image,const uint8_t *src,int srcc) {

  // Bare minimum, we can't take less than 20 bytes.
  if (srcc<20) return -1;
  image->w=src[4]|(src[5]<<8)|(src[6]<<16)|(src[7]<<24);
  image->h=src[8]|(src[9]<<8)|(src[10]<<16)|(src[11]<<24);
  int planec=src[12]|(src[13]<<8);
  image->pixelsize=src[14]|(src[15]<<8);
  image->compression=src[16]|(src[17]<<8)|(src[18]<<16)|(src[19]<<24);
  
  // Best guess for channel masks, if the header is short.
  switch (image->pixelsize) {
    case 16: image->rmask=0xf800; image->gmask=0x07e0; image->bmask=0x001f; break;
    case 24: image->rmask=0xff0000; image->gmask=0x00ff00; image->bmask=0x0000ff; break;
    case 32: image->rmask=0xff000000; image->gmask=0x00ff0000; image->bmask=0x0000ff00; image->amask=0x000000ff; break;
  }
  
  if (srcc<36) return 0;
  image->ctabc=src[32]|(src[33]<<8)|(src[34]<<16)|(src[35]<<24);
  
  if (srcc<56) return 0;
  // Read masks in the native byte order, because we're not byte-swapping individual pixels either.
  image->rmask=*(uint32_t*)(src+40);
  image->gmask=*(uint32_t*)(src+44);
  image->bmask=*(uint32_t*)(src+48);
  image->amask=*(uint32_t*)(src+52);
  
  return 0;
}

#define bmp_decode_OS22XBITMAPHEADER bmp_decode_BITMAPINFOHEADER
#define bmp_decode_BITMAPV2INFOHEADER bmp_decode_BITMAPINFOHEADER
#define bmp_decode_BITMAPV3INFOHEADER bmp_decode_BITMAPINFOHEADER
#define bmp_decode_BITMAPV4HEADER bmp_decode_BITMAPINFOHEADER
#define bmp_decode_BITMAPV5HEADER bmp_decode_BITMAPINFOHEADER
   
/* Decode color table.
 */
 
static int bmp_decode_ctab(struct bmp_image *image,const uint8_t *src,int ctabc) {
  // Per Wikipedia: Sometimes it's 3 bytes, and sometimes the dummy byte is actually alpha.
  // Hard to know when though...
  if (image->ctab) return -1;
  if (ctabc<1) return 0;
  if (ctabc>256) ctabc=256;
  uint8_t *nv=malloc(ctabc<<2);
  if (!nv) return -1;
  uint8_t *dst=nv;
  int i=ctabc;
  for (;i-->0;dst+=4,src+=4) {
    dst[0]=src[2];
    dst[1]=src[1];
    dst[2]=src[0];
    dst[3]=0xff;
  }
  image->ctab=nv;
  image->ctabc=ctabc;
  return 0;
}

/* Copy pixels reversing Y axis.
 * BMP conventionally stores pixels LRBT. We will never keep them in memory this way.
 */
 
static void bmp_copy_flip(uint8_t *dst,const uint8_t *src,int stride,int h) {
  src+=stride*(h-1);
  for (;h-->0;dst+=stride,src-=stride) memcpy(dst,src,stride);
}

/* Decode.
 */

struct bmp_image *bmp_decode(const void *src,int srcc) {
  const uint8_t *SRC=src;
  int srcp=0;
  int pixelsp=0;
  
  /* BITMAPFILEHEADER.
   * Optional, because ICO files don't include it.
   * If present, this is the authority for (pixelsp).
   */
  if ((srcp<=srcc-14)&&!memcmp(src,"BM",2)) {
    pixelsp=SRC[10]|(SRC[11]<<8)|(SRC[12]<<16)|(SRC[13]<<24);
    if (pixelsp<14) return 0;
    srcp+=14;
  }
  
  /* DIB Header.
   * There are several versions of this. They all begin with 4-byte header length.
   * If we find a sensible length, create the image before proceeding.
   */
  if (srcp>srcc-4) return 0;
  int hdrlen=SRC[srcp]|(SRC[srcp+1]<<8)|(SRC[srcp+2]<<16)|(SRC[srcp+3]<<24);
  if (hdrlen<4) return 0;
  if (srcp>srcc-hdrlen) return 0;
  
  struct bmp_image *image=calloc(1,sizeof(struct bmp_image));
  if (!image) return 0;
  
  // https://en.wikipedia.org/wiki/BMP_file_format
  int err=-1;
  switch (hdrlen) {
    case 12: err=bmp_decode_BITMAPCOREHEADER(image,SRC+srcp,hdrlen); break;
    case 64: err=bmp_decode_OS22XBITMAPHEADER(image,SRC+srcp,hdrlen); break;
    case 16: err=bmp_decode_OS22XBITMAPHEADER(image,SRC+srcp,hdrlen); break;
    case 40: err=bmp_decode_BITMAPINFOHEADER(image,SRC+srcp,hdrlen); break;
    case 52: err=bmp_decode_BITMAPV2INFOHEADER(image,SRC+srcp,hdrlen); break;
    case 56: err=bmp_decode_BITMAPV3INFOHEADER(image,SRC+srcp,hdrlen); break;
    case 108: err=bmp_decode_BITMAPV4HEADER(image,SRC+srcp,hdrlen); break;
    case 124: err=bmp_decode_BITMAPV5HEADER(image,SRC+srcp,hdrlen); break;
  }
  if (err<0) {
    bmp_image_del(image);
    return 0;
  }
  srcp+=hdrlen;
  
  // Validate header.
  int flip=0;
  if (image->h<0) image->h=-image->h;
  else flip=1;
  if (
    (image->w<1)||(image->w>0x7fff)||
    (image->h<1)||(image->h>0x7fff)||
    (image->ctabc<0)||
    (image->pixelsize<1)||
    (image->pixelsize>32)
  ) {
    bmp_image_del(image);
    return 0;
  }
  
  switch (image->compression) {
    case 0: // none
    case 3: // bitfields
    case 6: // rgba bitfields
      break;
    // Should we support these?
    case 1: // RLE8
    case 2: // RLE4
    case 4: // JPEG
    case 5: // PNG
    case 11: // CMYK
    case 12: // CMYKRLE8
    case 13: // CMYKRLE4
      bmp_image_del(image);
      return 0;
  }
  
  /* Color table.
   * Per Wikipedia, it's more complicated than following the stated (ctabc). Because of course it bloody is.
   * I'm going to assume that (ctabc) is always set correctly.
   */
  if (image->ctabc) {
    int ctablen=image->ctabc<<2;
    if (srcp>srcc-ctablen) {
      bmp_image_del(image);
      return 0;
    }
    if (bmp_decode_ctab(image,SRC+srcp,image->ctabc)<0) {
      bmp_image_del(image);
      return 0;
    }
    srcp+=ctablen;
  }
  
  /* If a DIB header was present, there can be a gap here up to the offset it specified.
   * If not, we must force alignment to 4 bytes.
   */
  if (pixelsp) {
    if (pixelsp<srcp) {
      bmp_image_del(image);
      return 0;
    }
    srcp=pixelsp;
  } else if (srcp&3) {
    srcp=(srcp+3)&~3;
  }
  
  image->stride=((image->w*image->pixelsize+31)>>3)&~3;
  if ((image->stride<1)||(image->stride>INT_MAX/image->h)) {
    bmp_image_del(image);
    return 0;
  }
  int pixelslen=image->stride*image->h;
  if ((srcp>srcc-pixelslen)||!(image->v=malloc(pixelslen))) {
    bmp_image_del(image);
    return 0;
  }
  if (flip) bmp_copy_flip(image->v,SRC+srcp,image->stride,image->h);
  else memcpy(image->v,SRC+srcp,pixelslen);
  
  return image;
}

/* Reformat from packed 16-bit pixels.
 */
 
static int bmp_reformat_rgba_from_16(struct bmp_image *image) {
  int nstride=((image->w*32+31)&~31)>>3;
  uint8_t *nv=malloc(nstride*image->h);
  if (!nv) return -1;
  
  int rshift=0,rsize=0,gshift=0,gsize=0,bshift=0,bsize=0;
  uint32_t tmp;
  if (tmp=image->rmask) {
    while (!(tmp&1)) { tmp>>=1; rshift++; }
    while (tmp&1) { tmp>>=1; rsize++; }
  }
  if (tmp=image->gmask) {
    while (!(tmp&1)) { tmp>>=1; gshift++; }
    while (tmp&1) { tmp>>=1; gsize++; }
  }
  if (tmp=image->bmask) {
    while (!(tmp&1)) { tmp>>=1; bshift++; }
    while (tmp&1) { tmp>>=1; bsize++; }
  }
  
  const uint8_t *srcrow=image->v;
  uint8_t *dstrow=nv;
  int yi=image->h; for (;yi-->0;srcrow+=image->stride) {
    const uint16_t *srcp=(uint16_t*)srcrow;
    uint8_t *dstp=dstrow;
    int xi=image->w; for (;xi-->0;srcp++,dstp+=4) {
      dstp[0]=((*srcp)&image->rmask)>>rshift;
      dstp[0]|=dstp[0]>>rsize;
      dstp[1]=((*srcp)&image->gmask)>>gshift;
      dstp[1]|=dstp[1]>>gsize;
      dstp[2]=((*srcp)&image->bmask)>>bshift;
      dstp[2]|=dstp[2]>>gsize;
      dstp[3]=((*srcp)&image->amask)?0xff:0x00; // assume that amask is one bit
    }
  }
  
  free(image->v);
  image->v=nv;
  image->stride=nstride;
  image->pixelsize=32;
  image->rmask=0x000000ff;
  image->gmask=0x0000ff00;
  image->bmask=0x00ff0000;
  image->amask=0xff000000;
  return 0;
}
 
static int bmp_reformat_rgb_from_16(struct bmp_image *image) {
  int nstride=((image->w*24+31)&~31)>>3;
  uint8_t *nv=malloc(nstride*image->h);
  if (!nv) return -1;
  
  int rshift=0,rsize=0,gshift=0,gsize=0,bshift=0,bsize=0;
  uint32_t tmp;
  if (tmp=image->rmask) {
    while (!(tmp&1)) { tmp>>=1; rshift++; }
    while (tmp&1) { tmp>>=1; rsize++; }
  }
  if (tmp=image->gmask) {
    while (!(tmp&1)) { tmp>>=1; gshift++; }
    while (tmp&1) { tmp>>=1; gsize++; }
  }
  if (tmp=image->bmask) {
    while (!(tmp&1)) { tmp>>=1; bshift++; }
    while (tmp&1) { tmp>>=1; bsize++; }
  }
  
  const uint8_t *srcrow=image->v;
  uint8_t *dstrow=nv;
  int yi=image->h; for (;yi-->0;srcrow+=image->stride) {
    const uint16_t *srcp=(uint16_t*)srcrow;
    uint8_t *dstp=dstrow;
    int xi=image->w; for (;xi-->0;srcp++,dstp+=3) {
      dstp[0]=((*srcp)&image->rmask)>>rshift;
      dstp[0]|=dstp[0]>>rsize;
      dstp[1]=((*srcp)&image->gmask)>>gshift;
      dstp[1]|=dstp[1]>>gsize;
      dstp[2]=((*srcp)&image->bmask)>>bshift;
      dstp[2]|=dstp[2]>>gsize;
    }
  }
  
  free(image->v);
  image->v=nv;
  image->stride=nstride;
  image->pixelsize=24;
  image->rmask=0x000000ff;
  image->gmask=0x0000ff00;
  image->bmask=0x00ff0000;
  image->amask=0;
  return 0;
}

/* Swizzle bytes.
 */
 
static void bmp_swizzle3_02(struct bmp_image *image) {
  uint8_t *row=image->v;
  int yi=image->h;
  for (;yi-->0;row+=image->stride) {
    uint8_t *p=row;
    int xi=image->w;
    for (;xi-->0;p+=3) {
      uint8_t tmp=p[0];
      p[0]=p[2];
      p[2]=tmp;
    }
  }
  int tmp=image->rmask;
  image->rmask=image->bmask;
  image->bmask=tmp;
}
 
static void bmp_swizzle4(struct bmp_image *image,const uint8_t srcp_by_dstp[4]) {
  uint8_t *row=image->v;
  int yi=image->h;
  for (;yi-->0;row+=image->stride) {
    uint8_t *p=row;
    int xi=image->w;
    for (;xi-->0;p+=4) {
      uint8_t tmp[4];
      tmp[0]=p[srcp_by_dstp[0]];
      tmp[1]=p[srcp_by_dstp[1]];
      tmp[2]=p[srcp_by_dstp[2]];
      tmp[3]=p[srcp_by_dstp[3]];
      memcpy(p,tmp,4);
    }
  }
}

/* Sanitize pixel format.
 */
 
int bmp_force_png_format(struct bmp_image *image) {
  if (!image) return -1;
  
  /* Assume that sub-byte pixels are index, and already PNG-legal.
   * Baffingly, even though everything else in the format is little-endian, BMP packs bits big-endianly.
   * But I'm not complaining; it works to our advantage.
   */
  if (image->pixelsize<=8) return 0;
  
  /* 16-bit pixels, we must expand to 24 or 32.
   */
  if (image->pixelsize==16) {
    if (image->amask) return bmp_reformat_rgba_from_16(image);
    return bmp_reformat_rgb_from_16(image);
  }
  
  /* 24-bit pixels, swizzle channels as needed.
   * Realistically it will only be (0<~>2) or nothing.
   * I'm finding that GIMP at least, ignores the masks and assumes BGR always.
   * So if masks are unset, assume the same.
   */
  if (image->pixelsize==24) {
    if ((image->rmask==0x0000ff)&&(image->gmask==0x00ff00)&&(image->bmask==0xff0000)) return 0;
    if ((image->rmask==0xff0000)&&(image->gmask==0x00ff00)&&(image->bmask==0x0000ff)) {
      bmp_swizzle3_02(image);
      return 0;
    }
    bmp_swizzle3_02(image);
    return 0;
  }
  
  /* 32-bit pixels, swizzle as needed.
   */
  if (image->pixelsize==32) {
    if ((image->rmask==0x000000ff)&&(image->gmask==0x0000ff00)&&(image->bmask==0x00ff0000)&&(image->amask==0xff000000)) return 0;
    uint8_t srcp_by_dstp[4]={0xff,0xff,0xff,0xff};
    #define _(dstp,mask) switch (mask) { \
      case 0x000000ff: srcp_by_dstp[dstp]=0; break; \
      case 0x0000ff00: srcp_by_dstp[dstp]=1; break; \
      case 0x00ff0000: srcp_by_dstp[dstp]=2; break; \
      case 0xff000000: srcp_by_dstp[dstp]=3; break; \
      default: return -1; \
    }
    _(0,image->rmask)
    _(1,image->gmask)
    _(2,image->bmask)
    _(3,image->amask)
    #undef _
    bmp_swizzle4(image,srcp_by_dstp);
    image->rmask=0x000000ff;
    image->gmask=0x0000ff00;
    image->bmask=0x00ff0000;
    image->amask=0xff000000;
    return 0;
  }
  
  return -1;
}
