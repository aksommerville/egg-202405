#include "ico.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>

#if USE_png
  #include "opt/png/png.h"
#endif

/* Cleanup.
 */
 
void ico_image_cleanup(struct ico_image *image) {
  if (image->v) free(image->v);
  if (image->serial) free(image->serial);
}

void ico_file_cleanup(struct ico_file *file) {
  if (!file) return;
  if (file->imagev) {
    while (file->imagec-->0) ico_image_cleanup(file->imagev+file->imagec);
    free(file->imagev);
  }
}

void ico_file_del(struct ico_file *file) {
  if (!file) return;
  ico_file_cleanup(file);
  free(file);
}

/* Encode one image's serial if absent.
 */
 
static int ico_image_require_serial(struct ico_file *file,struct ico_image *image) {
  if (image->serial) return 0;

  // We're missing something here, and the format confuses me.
  // PNG output does work, at least the GIMP opens them ok. So go with just that.
  // I don't anticipate ICO output being a big thing for us.
  // 2024-02-26
  #if USE_bmp && 0
  {
    struct bmp_image bmp={
      .v=image->v,
      .w=image->w,
      .h=image->h,
      .stride=image->stride,
      .pixelsize=image->pixelsize,
      // ctab,ctabc?
      .compression=3,
      .rmask=0x000000ff,
      .gmask=0x0000ff00,
      .bmask=0x00ff0000,
      .amask=0xff000000,
    };
    struct sr_encoder encoder={0};
    if (bmp_encode(&encoder,&bmp,1)<0) {
      sr_encoder_cleanup(&encoder);
      return -1;
    }
    image->serial=encoder.v; // HANDOFF
    image->serialc=encoder.c;
    return 0;
  }
  #endif

  #if USE_png
  {
    struct png_image png={
      .v=image->v,
      .w=image->w,
      .h=image->h,
      .stride=image->stride,
      .pixelsize=image->pixelsize,
    };
    switch (image->pixelsize) {
      case 1: png.depth=1; png.colortype=0; break;
      case 2: png.depth=2; png.colortype=0; break;
      case 4: png.depth=4; png.colortype=0; break;
      case 8: png.depth=8; png.colortype=0; break;
      case 16: png.depth=8; png.colortype=4; break;
      case 24: png.depth=8; png.colortype=2; break;
      case 32: png.depth=8; png.colortype=6; break;
      default: return -1;
    }
    struct sr_encoder encoder={0};
    if (png_encode(&encoder,&png)<0) {
      sr_encoder_cleanup(&encoder);
      return -1;
    }
    image->serial=encoder.v; // HANDOFF
    image->serialc=encoder.c;
    return 0;
  }
  #endif

  return -1;
}

/* Encode.
 */

int ico_encode(struct sr_encoder *dst,struct ico_file *file) {
  
  // 6-byte header.
  if (sr_encode_raw(dst,"\0\0\1\0",4)<0) return -1;
  if (sr_encode_intle(dst,file->imagec,2)<0) return -1;
  
  // TOC: 16 bytes per image.
  int payp=6+16*file->imagec;
  struct ico_image *image=file->imagev;
  int i=file->imagec;
  for (;i-->0;image++) {
    if ((image->w<1)||(image->w>256)) return -1;
    if ((image->h<1)||(image->h>256)) return -1;
    if (ico_image_require_serial(file,image)<0) return -1;
    if (sr_encode_u8(dst,image->w)<0) return -1; // 0 for 256, let it overflow
    if (sr_encode_u8(dst,image->h)<0) return -1; // ''
    if (sr_encode_u8(dst,image->ctc)<0) return -1;
    if (sr_encode_u8(dst,0)<0) return -1; // rsv
    if (sr_encode_intle(dst,image->planec,2)<0) return -1;
    if (sr_encode_intle(dst,image->pixelsize,2)<0) return -1; // why are dimensions 1 byte but pixel size 2?
    if (sr_encode_intle(dst,image->serialc,4)<0) return -1;
    if (sr_encode_intle(dst,payp,4)<0) return -1;
    payp+=image->serialc;
  }
  
  // Payload for each image.
  for (image=file->imagev,i=file->imagec;i-->0;image++) {
    if (sr_encode_raw(dst,image->serial,image->serialc)<0) return -1;
  }
  
  return 0;
}

/* Decode in file.
 */
 
static int ico_file_decode(struct ico_file *ico,const uint8_t *src,int srcc) {
  if (srcc<6) return -1;
  if (memcmp(src,"\0\0\1\0",4)) return -1; // Signature and file type: icon
  int imagec=src[4]|(src[5]<<8);
  int srcp=6;
  while (imagec-->0) {
    if (srcp>srcc-16) return -1;
    int w=src[srcp++]; if (!w) w=256;
    int h=src[srcp++]; if (!h) h=256;
    int ctc=src[srcp++];
    int rsv=src[srcp++];
    if (rsv) return -1;
    int planec=src[srcp]|(src[srcp+1]<<8); srcp+=2;
    int pixelsize=src[srcp]|(src[srcp+1]<<8); srcp+=2;
    int paylen=src[srcp]|(src[srcp+1]<<8)|(src[srcp+2]<<16)|(src[srcp+3]<<24); srcp+=4;
    int payp=src[srcp]|(src[srcp+1]<<8)|(src[srcp+2]<<16)|(src[srcp+3]<<24); srcp+=4;
    if ((paylen<0)||(payp<0)||(payp>srcc-paylen)) return -1;
    struct ico_image *image=ico_file_add_uninitialized_image(ico);
    if (!image) return -1;
    if (!(image->serial=malloc(paylen))) return -1;
    memcpy(image->serial,src+payp,paylen);
    image->serialc=paylen;
    image->w=w;
    image->h=h;
    image->ctc=ctc;
    image->planec=planec;
    image->pixelsize=pixelsize;
    // (stride,v) do not get populated yet
  }
  return 0;
}

/* Decode.
 */

struct ico_file *ico_decode(const void *src,int srcc) {
  struct ico_file *file=calloc(1,sizeof(struct ico_file));
  if (!file) return 0;
  if (ico_file_decode(file,src,srcc)<0) {
    ico_file_del(file);
    return 0;
  }
  return file;
}

/* Get image.
 */

struct ico_image *ico_file_get_image(struct ico_file *file,int w,int h,int obo) {
  if (!file||(file->imagec<1)) return 0;
  int pxc=w*h;
  struct ico_image *bestimage=file->imagev;
  int bestscore=INT_MAX;
  struct ico_image *image=file->imagev;
  int i=file->imagec;
  for (;i-->0;image++) {
    if ((image->w==w)&&(image->h==h)) {
      bestimage=image;
      break;
    }
    int qpxc=image->w*image->h;
    int qscore=qpxc-pxc;
    if (qscore<0) qscore=-qscore;
    if (qscore<bestscore) {
      bestimage=image;
      bestscore=qscore;
    }
  }
  if (!obo) {
    if ((bestimage->w!=w)||(bestimage->h!=h)) return 0;
  }
  if (ico_image_decode(bestimage)<0) return 0;
  return bestimage;
}

/* Populate ico_image from png_image.
 * May handoff and zero pixels directly.
 */
 
#if USE_png
 
static int ico_image_yoink_png(struct ico_image *ico,struct png_image *png) {
  if (png_image_reformat(png,8,6)<0) return -1;
  ico->v=png->v;
  png->v=0;
  ico->w=png->w;
  ico->h=png->h;
  ico->stride=png->stride;
  ico->planec=1;
  ico->pixelsize=png->pixelsize;
  return 0;
}

#endif

/* Finish decoding from BMP.
 * Caller takes all the measurements.
 *
 * Pixels and mask are stored big-endianly (despite everything else being little-endian, grr).
 * Color table entries are 32 bit BGRX, the last 8 are evidently unused.
 * Mask is backward: Ones are transparent, zeroes are opaque.
 */
 
static int ico_image_from_bmp(
  struct ico_image *image,
  const uint8_t *ctab,
  const uint8_t *pixels,int stride,
  const uint8_t *mask,int maskstride
) {
  int dststride=image->w<<2;
  uint8_t *dst=calloc(dststride,image->h); // only need to visit the opaque ones
  if (!dst) return -1;
  
  const uint8_t *srcrow=pixels;
  const uint8_t *maskrow=mask;
  uint8_t *dstrow=dst+dststride*(image->h-1); // write bottom to top
  int yi=image->h;
  for (;yi-->0;srcrow+=stride,maskrow+=maskstride,dstrow-=dststride) {
    const uint8_t *srcp=srcrow;
    const uint8_t *maskp=maskrow;
    uint8_t maskmask=0x80;
    uint8_t *dstp=dstrow;
    int xi=image->w;
    switch (image->pixelsize) {
    
      case 1: {
          for (;xi-->0;dstp+=4) {
            if (!((*maskp)&maskmask)) {
              int p=((*srcp)&maskmask)?1:0;
              if (p>=image->ctc) {
                if (p) dstp[0]=dstp[1]=dstp[2]=0xff;
                else dstp[0]=dstp[1]=dstp[2]=0x00;
              } else {
                const uint8_t *ct=ctab+p*4;
                dstp[0]=ct[2];
                dstp[1]=ct[1];
                dstp[2]=ct[0];
              }
              dstp[3]=0xff;
            }
            if (maskmask==1) {
              maskmask=0x80;
              srcp++;
              maskp++;
            } else {
              maskmask>>=1;
            }
          }
        } break;
        
      case 2: {
          int srcshift=6;
          for (;xi-->0;dstp+=4) {
            if (!((*maskp)&maskmask)) {
              int p=((*srcp)>>srcshift)&3;
              if (p>=image->ctc) {
                p|=p<<2;
                p|=p<<4;
                dstp[0]=dstp[1]=dstp[2]=p;
              } else {
                const uint8_t *ct=ctab+p*4;
                dstp[0]=ct[2];
                dstp[1]=ct[1];
                dstp[2]=ct[0];
              }
              dstp[3]=0xff;
            }
            if (maskmask==1) {
              maskmask=0x80;
              maskp++;
            } else {
              maskmask>>=1;
            }
            if (srcshift) {
              srcshift-=2;
            } else {
              srcshift=6;
              srcp++;
            }
          }
        } break;
        
      case 4: {
          int srcshift=4;
          for (;xi-->0;dstp+=4) {
            if (!((*maskp)&maskmask)) {
              int p=((*srcp)>>srcshift)&15;
              if (p>=image->ctc) {
                p|=p<<2;
                p|=p<<4;
                dstp[0]=dstp[1]=dstp[2]=p;
              } else {
                const uint8_t *ct=ctab+p*4;
                dstp[0]=ct[2];
                dstp[1]=ct[1];
                dstp[2]=ct[0];
              }
              dstp[3]=0xff;
            }
            if (maskmask==1) {
              maskmask=0x80;
              maskp++;
            } else {
              maskmask>>=1;
            }
            if (srcshift) {
              srcshift=0;
            } else {
              srcshift=4;
              srcp++;
            }
          }
        } break;
        
      case 8: {
          for (;xi-->0;dstp+=4,srcp++) {
            if (!((*maskp)&maskmask)) {
              int p=*srcp;
              if (p>=image->ctc) {
                dstp[0]=dstp[1]=dstp[2]=p;
              } else {
                const uint8_t *ct=ctab+p*4;
                dstp[0]=ct[2];
                dstp[1]=ct[1];
                dstp[2]=ct[0];
              }
            }
            if (maskmask==1) {
              maskmask=0x80;
              maskp++;
            } else {
              maskmask>>=1;
            }
          }
        } break;
        
      default: return -1; // Is it ever 16, 24, or 32? Or something else?
    }
  }
  
  image->v=dst;
  image->stride=dststride;
  image->pixelsize=32;
  image->planec=1;
  return 0;
}

/* Decode image.
 */
 
int ico_image_decode(struct ico_image *image) {
  if (image->v) return 0;
  
  // Try PNG first because its header is unambiguous.
  #if USE_png
    struct png_image *png=png_decode(image->serial,image->serialc);
    if (png) {
      int err=ico_image_yoink_png(image,png);
      png_image_del(png);
      return err;
    }
  #endif
  
  // When it's BMP, we don't actually need to use the "bmp" unit.
  // And in fact we can't. The header is all mixed up between the ICO TOC and the BMP header.
  // I am assuming that all BMP ICO have a color table and mask, according to the image TOC.
  if (image->serialc>=4) {
    const uint8_t *src=image->serial;
    int hdrlen=src[0]|(src[1]<<8)|(src[2]<<16)|(src[3]<<24);
    if ((hdrlen>=4)&&(hdrlen<image->serialc)) {
      const uint8_t *ctab=src+hdrlen;
      int ctablen=image->ctc*4;
      if (hdrlen<=image->serialc-ctablen) {
        int pixp=hdrlen+ctablen;
        pixp=(pixp+3)&~3; // Pixels always begin on a 4-byte boundary.
        const uint8_t *pixels=src+pixp;
        int stride=((image->pixelsize*image->w+31)&~31)>>3; // Stride always a multiple of 4.
        if (stride<INT_MAX/image->h) {
          int pixelslen=stride*image->h;
          if (pixp<=image->serialc-pixelslen) {
            const uint8_t *mask=src+pixp+pixelslen;
            int maskstride=((image->w+31)&~31)>>3; // Mask stride always a multiple of 4.
            int masklen=maskstride*image->h;
            if (pixp+pixelslen<=image->serialc-masklen) {
              return ico_image_from_bmp(image,ctab,pixels,stride,mask,maskstride);
            } else {
              // Not sure whether BMP ICO always has a mask. We could proceed here if that happens.
            }
          }
        }
      }
    }
  }
  
  return -1;
}

/* Drop serial data.
 */
 
void ico_image_dirty(struct ico_image *image) {
  if (image->serial) {
    free(image->serial);
    image->serial=0;
    image->serialc=0;
  }
}

/* Add image to file.
 */

struct ico_image *ico_file_add_image(struct ico_file *file,int w,int h) {
  if (!file) return 0;
  if ((w<1)||(w>256)) return 0;
  if ((h<1)||(h>256)) return 0;
  struct ico_image *image=ico_file_add_uninitialized_image(file);
  if (!image) return 0;
  image->w=w;
  image->h=h;
  image->stride=w<<2;
  image->planec=1;
  image->pixelsize=32;
  if (!(image->v=calloc(image->stride,image->h))) {
    file->imagec--;
    return 0;
  }
  return image;
}

/* Add uninitialized image.
 */
 
struct ico_image *ico_file_add_uninitialized_image(struct ico_file *file) {
  if (!file) return 0;
  if (file->imagec>=file->imagea) {
    int na=file->imagea+8;
    if (na>INT_MAX/sizeof(struct ico_image)) return 0;
    void *nv=realloc(file->imagev,sizeof(struct ico_image)*na);
    if (!nv) return 0;
    file->imagev=nv;
    file->imagea=na;
  }
  struct ico_image *image=file->imagev+file->imagec++;
  memset(image,0,sizeof(struct ico_image));
  return image;
}
