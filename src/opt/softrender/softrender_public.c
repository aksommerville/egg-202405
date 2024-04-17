#include "softrender_internal.h"

const char *softrender_hint_opaque="opaque";
const char *softrender_hint_zeroalpha="zeroalpha";

/* Delete context.
 */

void softrender_del(struct softrender *softrender) {
  if (!softrender) return;
  if (softrender->texturev) {
    while (softrender->texturec-->0) {
      rawimg_del(softrender->texturev[softrender->texturec]);
    }
    free(softrender->texturev);
  }
  free(softrender);
}

/* New context.
 */
 
struct softrender *softrender_new() {
  struct softrender *softrender=calloc(1,sizeof(struct softrender));
  if (!softrender) return 0;
  return softrender;
}

/* Delete texture.
 */

void softrender_texture_del(struct softrender *softrender,int texid) {
  if ((texid<2)||(texid>softrender->texturec)) return; // sic <2: Not allowed to delete texture 1.
  texid--;
  rawimg_del(softrender->texturev[texid]);
  softrender->texturev[texid]=0;
  while ((softrender->texturec>0)&&!softrender->texturev[softrender->texturec-1]) softrender->texturec--;
}

/* New texture.
 */
 
int softrender_texture_new(struct softrender *softrender) {
  
  // Find an available slot.
  int p=-1;
  if (softrender->texturec<softrender->texturea) {
    p=softrender->texturec++;
    softrender->texturev[p]=0;
  } else {
    int i=softrender->texturec;
    while (i-->0) {
      if (!softrender->texturev[i]) {
        p=i;
        break;
      }
    }
    if (p<0) {
      if (softrender->texturea>=SOFTRENDER_TEXTURE_LIMIT) return -1;
      int na=softrender->texturea+16;
      if (na>INT_MAX/sizeof(void*)) return -1;
      void *nv=realloc(softrender->texturev,sizeof(void*)*na);
      if (!nv) return -1;
      softrender->texturev=nv;
      softrender->texturea=na;
      p=softrender->texturec++;
      softrender->texturev[p]=0;
    }
  }
  
  // Create a dummy rawimg in this slot.
  // We carefully rewrite the rawimg objects in place on loads. It's OK to have them zeroed.
  if (!(softrender->texturev[p]=calloc(1,sizeof(struct rawimg)))) return -1;
  softrender->texturev[p]->ownv=1;
  
  // And finally, texid is one greater than the index.
  return p+1;
}

/* Reallocate image in place if geometry doesn't match request.
 * Image content is undefined after this.
 */
 
static int softrender_texture_resize(struct rawimg *rawimg,int w,int h,int fmt,int pixelsize,int stride,int minstride) {

  if ((rawimg->w==w)&&(rawimg->h==h)&&(rawimg->pixelsize==pixelsize)) {
    //TODO Check (fmt)? We know the size agrees but maybe the pixel layout changed.
    return 0;
  }

  // Reallocate only if the total size increases.
  int havelen=rawimg->stride*rawimg->h;
  int needlen=minstride*h;
  if (needlen>havelen) {
    void *nv=realloc(rawimg->v,needlen);
    if (!nv) return -1;
    rawimg->v=nv;
    rawimg->ownv=1;
  }
  
  // Update rawimg's split-out format things.
  switch (fmt) {
    case EGG_TEX_FMT_RGBA: {
        uint32_t bodetect=0x01020304;
        if (*(uint8_t*)&bodetect==0x01) {
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
        memcpy(rawimg->chorder,"RGBA",4);
        rawimg->bitorder='>';
      } break;
    case EGG_TEX_FMT_A8: {
        rawimg->rmask=rawimg->gmask=rawimg->bmask=0;
        rawimg->amask=0xff;
        memcpy(rawimg->chorder,"A\0\0\0",4);
        rawimg->bitorder='>';
      } break;
    case EGG_TEX_FMT_A1: {
        rawimg->rmask=rawimg->gmask=rawimg->bmask=0;
        rawimg->amask=1;
        memcpy(rawimg->chorder,"A\0\0\0",4);
        rawimg->bitorder='>';
      } break;
    case EGG_TEX_FMT_Y8: {
        rawimg->rmask=rawimg->gmask=rawimg->bmask=0xff;
        rawimg->amask=0;
        memcpy(rawimg->chorder,"Y\0\0\0",4);
        rawimg->bitorder='>';
      } break;
    case EGG_TEX_FMT_Y1: {
        rawimg->rmask=rawimg->gmask=rawimg->bmask=1;
        rawimg->amask=0;
        memcpy(rawimg->chorder,"Y\0\0\0",4);
        rawimg->bitorder='>';
      } break;
    default: return -1;
  }
  
  // Record the updated geometry.
  rawimg->w=w;
  rawimg->h=h;
  rawimg->stride=minstride;
  rawimg->pixelsize=pixelsize;
  return 0;
}

/* Zero pixels.
 */
 
static void softrender_texture_zero(struct rawimg *rawimg) {
  // Textures are not allowed to be subviews, ie anything between the image edge and the stride is presumed garbage.
  // So we can do this with a single memset.
  memset(rawimg->v,0,rawimg->stride*rawimg->h);
}

/* Replace pixels.
 * Caller must resize first, so the image and incoming pixels have the same geometry except maybe stride.
 */
 
static void softrender_texture_replace(struct rawimg *rawimg,int srcstride,const uint8_t *src) {
  if (rawimg->stride==srcstride) {
    memcpy(rawimg->v,src,srcstride*rawimg->h);
  } else {
    uint8_t *dst=rawimg->v;
    int yi=rawimg->h;
    for (;yi-->0;dst+=rawimg->stride,src+=srcstride) {
      memcpy(dst,src,rawimg->stride);
    }
  }
}

/* Rewrite image in place with an egg-legal format, if it's not already.
 */
 
static int softrender_force_valid_format(struct rawimg *rawimg) {
  switch (rawimg->pixelsize) {
    case 1: { // A1,Y1
        if (rawimg->amask==1) return 0;
        return rawimg_force_y1(rawimg);
      } break;
    case 8: { // A8,Y8
        if (rawimg->amask==0xff) return 0;
        return rawimg_force_y8(rawimg);
      } break;
    case 32: { // RGBA
        return rawimg_force_rgba(rawimg);
      } break;
    default: { // Anything else, promote to RGBA.
        return rawimg_force_rgba(rawimg);
      }
  }
  return -1;
}

/* Load pixels or encoded image to texture.
 */

int softrender_texture_load(struct softrender *softrender,int texid,int w,int h,int stride,int fmt,const void *src,int srcc) {
  if ((texid<1)||(texid>softrender->texturec)) return -1;
  struct rawimg *rawimg=softrender->texturev[texid-1];
  if (!rawimg) return -1; // Must be allocated first.
  
  /* (w,h,stride,fmt) zero means (src) is an encoded image.
   * This is not allowed against texture 1.
   * rawimg_decode() produces a new object, so we'll trash the old one on success.
   */
  if (!w&&!h&&!stride&&!fmt) {
    if (texid==1) return -1;
    struct rawimg *newimg=rawimg_decode(src,srcc);
    if (!newimg) return -1;
    if (softrender_force_valid_format(newimg)<0) {
      rawimg_del(newimg);
      return -1;
    }
    rawimg_del(rawimg);
    softrender->texturev[texid-1]=newimg;
    return 0;
  }
  
  /* Loads to texture 1 must not change its dimensions, unless it is currently 0,0.
   */
  if (texid==1) {
    if (rawimg->w||rawimg->h) {
      if ((rawimg->w!=w)||(rawimg->h!=h)) return -1;
    }
  }
  
  /* Take some measurements.
   */
  if ((w<1)||(w>SOFTRENDER_SIZE_LIMIT)) return -1;
  if ((h<1)||(h>SOFTRENDER_SIZE_LIMIT)) return -1;
  int pixelsize=0;
  switch (fmt) {
    case EGG_TEX_FMT_RGBA: pixelsize=32; break;
    case EGG_TEX_FMT_A8: pixelsize=8; break;
    case EGG_TEX_FMT_A1: pixelsize=1; break;
    case EGG_TEX_FMT_Y8: pixelsize=8; break;
    case EGG_TEX_FMT_Y1: pixelsize=1; break;
  }
  if ((texid==1)&&(pixelsize!=rawimg->pixelsize)) return -1; // texture 1 must not change pixelsize
  if ((pixelsize<1)||(pixelsize>32)) return -1;
  if (w>(INT_MAX-7)/pixelsize) return -1;
  int minstride=(w*pixelsize+7)>>3;
  if (stride<minstride) return -1;
  if (stride>INT_MAX/h) return -1;
  if (src) {
    if (srcc<stride*h) return -1;
    if (softrender_texture_resize(rawimg,w,h,fmt,pixelsize,stride,minstride)<0) return -1;
    softrender_texture_replace(rawimg,stride,src);
    
    // Determine hint.
    rawimg->encfmt=softrender_hint_opaque;
    const uint32_t *row=rawimg->v;
    int wstride=rawimg->stride>>2;
    int yi=rawimg->h;
    for (;yi-->0;row+=rawimg->stride) {
      const uint32_t *p=row;
      int xi=rawimg->w;
      for (;xi-->0;p++) {
        if (!*p) rawimg->encfmt=softrender_hint_zeroalpha;
        else {
          #if BYTE_ORDER==BIG_ENDIAN
            uint8_t a=(*p);
          #else
            uint32_t a=(*p)>>24;
          #endif
          if (a!=0xff) {
            rawimg->encfmt=0;
            return 0;
          }
        }
      }
    }
    
  } else {
    if (srcc) return -1;
    if (softrender_texture_resize(rawimg,w,h,fmt,pixelsize,stride,minstride)<0) return -1;
    softrender_texture_zero(rawimg);
    rawimg->encfmt=softrender_hint_zeroalpha;
  }
  return 0;
}

/* Get texture header.
 */

void softrender_texture_get_header(int *w,int *h,int *fmt,const struct softrender *softrender,int texid) {
  *w=*h=*fmt=0;
  if ((texid<1)||(texid>softrender->texturec)) return;
  struct rawimg *rawimg=softrender->texturev[texid-1];
  if (!rawimg) return;
  *w=rawimg->w;
  *h=rawimg->h;
  switch (rawimg->pixelsize) {
    case 1: {
        if (rawimg->amask==1) *fmt=EGG_TEX_FMT_A1;
        else *fmt=EGG_TEX_FMT_Y1;
      } break;
    case 8: {
        if (rawimg->amask==0xff) *fmt=EGG_TEX_FMT_A8;
        else *fmt=EGG_TEX_FMT_Y8;
      } break;
    case 32: {
        *fmt=EGG_TEX_FMT_RGBA;
      }
  }
}

/* Clear texture.
 */

void softrender_texture_clear(struct softrender *softrender,int texid) {
  if ((texid<1)||(texid>softrender->texturec)) return;
  struct rawimg *rawimg=softrender->texturev[texid-1];
  if (!rawimg) return;
  if (rawimg->encfmt!=softrender_hint_opaque) rawimg->encfmt=softrender_hint_zeroalpha;
  softrender_texture_zero(rawimg);
}

/* Set globals.
 */

void softrender_draw_mode(struct softrender *softrender,int xfermode,uint32_t tint,uint8_t alpha) {
  softrender->xfermode=xfermode;
  softrender->tint=tint;
  softrender->alpha=alpha;
}

/* Texture 1 setup.
 */
 
int softrender_init_texture_1(struct softrender *softrender,const struct hostio_video_fb_description *desc) {

  // Validate description.
  if (!desc) return -1;
  if ((desc->w<1)||(desc->w>SOFTRENDER_SIZE_LIMIT)) return -1;
  if ((desc->h<1)||(desc->h>SOFTRENDER_SIZE_LIMIT)) return -1;
  switch (desc->pixelsize) {
    case 1: case 8: break;
    case 32: {
        if (desc->stride&3) return -1;
      } break;
    default: return -1;
  }
  int minstride=(desc->w*desc->pixelsize+7)>>3;
  if (desc->stride<minstride) return -1;

  // Ensure there's no textures yet, and allocate the slot for texture 1.
  if (softrender->texturec) return -1;
  if (softrender->texturea<1) {
    int na=8;
    void *nv=realloc(softrender->texturev,sizeof(void*)*na);
    if (!nv) return -1;
    softrender->texturev=nv;
    softrender->texturea=na;
  }
  softrender->texturec=1;
  softrender->texturev[0]=0;
  
  /* Create the image on our own. It must match the video driver's declared stride, which rawimg_new_alloc() won't guarantee.
   * Our declared format is always Y1, Y8, or RGBA.
   * We'll create a converter if needed, to swizzle into the driver's format at the end of each frame.
   */
  softrender->fbcvt=0;
  softrender->pxcvt=0;
  struct rawimg *rawimg=calloc(1,sizeof(struct rawimg));
  if (!rawimg) return -1;
  softrender->texturev[0]=rawimg;
  rawimg->w=desc->w;
  rawimg->h=desc->h;
  rawimg->stride=desc->stride;
  rawimg->bitorder='>';
  rawimg->encfmt=softrender_hint_opaque;
  switch (rawimg->pixelsize=desc->pixelsize) {
    case 1: {
        memcpy(rawimg->chorder,"Y\0\0\0",4);
        rawimg->rmask=rawimg->gmask=rawimg->bmask=1;
        rawimg->amask=0;
      } break;
    case 8: {
        memcpy(rawimg->chorder,"Y\0\0\0",4);
        rawimg->rmask=rawimg->gmask=rawimg->bmask=0xff;
        rawimg->amask=0;
      } break;
    case 32: {
        memcpy(rawimg->chorder,"RGBX",4);
        uint32_t bodetect=0x01020304;
        int be=(*(uint8_t*)&bodetect==0x01);
        if (be) {
          rawimg->rmask=0xff000000;
          rawimg->gmask=0x00ff0000;
          rawimg->bmask=0x0000ff00;
          rawimg->amask=0x00000000;
        } else {
          softrender->pxcvt=softrender_pxcvt_swap;
          rawimg->rmask=0x000000ff;
          rawimg->gmask=0x0000ff00;
          rawimg->bmask=0x00ff0000;
          rawimg->amask=0x00000000;
        }
        char normorder[4];
        memcpy(normorder,desc->chorder,4);
        int i=4; while (i-->0) {
          if (!normorder[i]) normorder[i]='x';
          else if ((normorder[i]=='A')||(normorder[i]=='a')) normorder[i]='x';
          else if ((normorder[i]>='A')&&(normorder[i]<='Z')) normorder[i]+=0x20;
        }
             if (!memcmp(normorder,"rgbx",4)) { }
        else if (!memcmp(normorder,"xrgb",4)) { softrender->fbcvt=be?softrender_fbcvt_shr8:softrender_fbcvt_shl8; }
        else if (!memcmp(normorder,"bgrx",4)) { softrender->fbcvt=be?softrender_fbcvt_swap02:softrender_fbcvt_swap13; }
        else if (!memcmp(normorder,"xbgr",4)) { softrender->fbcvt=softrender_fbcvt_swap; }
        else {
          fprintf(stderr,"Unsupported 32-bit pixel format '%.4s'\n",normorder);
          return -1;
        }
      } break;
    default: return -1;
  }
  
  // Don't allocate (rawimg->v). It will be reassigned each frame with the buffer provided by the video driver.
  
  return 0;
}

void softrender_set_main(struct softrender *softrender,void *fb) {
  if (!fb) return;
  if (softrender->texturec<1) return;
  struct rawimg *rawimg=softrender->texturev[0];
  if (rawimg->v&&rawimg->ownv) free(rawimg->v);
  rawimg->v=fb;
  rawimg->ownv=0;
}

void softrender_finalize_frame(struct softrender *softrender) {
  if (softrender->fbcvt) {
    struct rawimg *img=softrender->texturev[0];
    softrender->fbcvt(img->v,img->w,img->h,img->stride>>2);
  }
}
