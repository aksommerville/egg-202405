#include "ico.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>

#if USE_bmp
  #include "opt/bmp/bmp.h"
#endif
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

/* Populate ico_image from bmp_image.
 * May handoff and zero pixels directly.
 */
 
#if USE_bmp
 
static int ico_image_yoink_bmp(struct ico_image *ico,struct bmp_image *bmp) {
  ico->v=bmp->v;
  bmp->v=0;
  ico->w=bmp->w;
  ico->h=bmp->h;
  ico->stride=bmp->stride;
  ico->planec=1;
  ico->pixelsize=bmp->pixelsize;
  //TODO We need to capture the color table if present.
  return 0;
}

#endif

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
  
  // Otherwise BMP, and beware that the outer header is missing.
  #if USE_bmp
    struct bmp_image *bmp=bmp_decode(image->serial,image->serialc);
    if (bmp) {
      int err=ico_image_yoink_bmp(image,bmp);
      bmp_image_del(bmp);
      return err;
    }
  #endif
  
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
