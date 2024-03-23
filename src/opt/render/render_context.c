#include "render_internal.h"

/* Delete.
 */
 
static void render_texture_cleanup(struct render_texture *texture) {
  if (texture->texid) glDeleteTextures(1,&texture->texid);
  if (texture->fbid) glDeleteFramebuffers(1,&texture->fbid);
}
 
void render_del(struct render *render) {
  if (!render) return;
  if (render->texturev) {
    while (render->texturec-->0) render_texture_cleanup(render->texturev+render->texturec);
    free(render->texturev);
  }
  free(render);
}

/* New.
 */
 
struct render *render_new() {
  struct render *render=calloc(1,sizeof(struct render));
  if (!render) return 0;
  
  render->alpha=0xff;
  
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  
  if (render_init_programs(render)<0) {
    render_del(render);
    return 0;
  }
  
  return render;
}

/* Delete texture.
 */

void render_texture_del(struct render *render,int texid) {
  if ((texid<2)||(texid>render->texturec)) return; // sic "<2", no deleting the main
  texid--;
  struct render_texture *texture=render->texturev+texid;
  render_texture_cleanup(texture);
  memset(texture,0,sizeof(struct render_texture));
}

/* New texture.
 */
 
int render_texture_new(struct render *render) {
  struct render_texture *texture=0;
  if (render->texturec<render->texturea) {
    texture=render->texturev+render->texturec++;
  } else {
    int i=render->texturec;
    struct render_texture *q=render->texturev;
    for (;i-->0;q++) {
      if (q->texid) continue;
      texture=q;
      break;
    }
    if (!texture) {
      int na=render->texturea+16;
      if (na>INT_MAX/sizeof(struct render_texture)) return 0;
      void *nv=realloc(render->texturev,sizeof(struct render_texture)*na);
      if (!nv) return 0;
      render->texturev=nv;
      render->texturea=na;
      texture=render->texturev+render->texturec++;
    }
  }
  memset(texture,0,sizeof(struct render_texture));
  
  glGenTextures(1,&texture->texid);
  if (!texture->texid) {
    glGenTextures(1,&texture->texid);
    if (!texture->texid) {
      return 0;
    }
  }
  
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  
  return (texture-render->texturev)+1;
}

/* Return one EGG_TEX_FMT_* if this image is uploadable, zero if invalid.
 */
 
static int render_texture_fmt_from_rawimg(struct rawimg *rawimg) {
  if (rawimg_is_rgba(rawimg)) return EGG_TEX_FMT_RGBA;
  if (rawimg_is_y8(rawimg)) return EGG_TEX_FMT_Y8;
  if (rawimg_is_y1(rawimg)) return EGG_TEX_FMT_Y1;
  if (rawimg->pixelsize==8) return EGG_TEX_FMT_A8;
  if ((rawimg->pixelsize==1)&&(rawimg->bitorder=='>')) return EGG_TEX_FMT_A1;
  return 0;
}

/* Expected length of incoming image data.
 */
 
static int render_texture_measure(int w,int h,int stride,int fmt) {
  if ((w<1)||(h<1)||(stride<1)) return -1;
  int pixelsize=0;
  switch (fmt) {
    case EGG_TEX_FMT_RGBA: pixelsize=32; break;
    case EGG_TEX_FMT_A8: pixelsize=8; break;
    case EGG_TEX_FMT_A1: pixelsize=1; break;
    case EGG_TEX_FMT_Y8: pixelsize=8; break;
    case EGG_TEX_FMT_Y1: pixelsize=1; break;
  }
  if (pixelsize<1) return -1;
  if (w>(INT_MAX-7)/pixelsize) return -1;
  int bitsperrow=w*pixelsize;
  int minstride=(bitsperrow+7)>>3;
  if (stride!=minstride) return -1; // incoming strides must be exact (TODO should we work around it?)
  if (stride>INT_MAX/h) return -1;
  return stride*h;
}

/* Upload pixels to a texture. Null is legal.
 */
 
static int render_texture_upload(struct render_texture *texture,int w,int h,int stride,int fmt,const void *v) {
  int ifmt,glfmt,type,chanc;
  switch (fmt) {
    case EGG_TEX_FMT_RGBA: chanc=4; ifmt=GL_RGBA; glfmt=GL_RGBA; type=GL_UNSIGNED_BYTE; break;
    case EGG_TEX_FMT_A8: chanc=1; ifmt=GL_ALPHA; glfmt=GL_LUMINANCE; type=GL_UNSIGNED_BYTE; break;
    case EGG_TEX_FMT_A1: return -1;
    case EGG_TEX_FMT_Y8: chanc=1; ifmt=GL_LUMINANCE; glfmt=GL_LUMINANCE; type=GL_UNSIGNED_BYTE; break;
    case EGG_TEX_FMT_Y1: return -1;
    default: return -1;
  }
  if (stride!=w*chanc) return -1;
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glTexImage2D(GL_TEXTURE_2D,0,ifmt,w,h,0,glfmt,type,v);
  texture->w=w;
  texture->h=h;
  texture->fmt=fmt;
  return 0;
}

/* Load texture.
 */

int render_texture_load(struct render *render,int texid,int w,int h,int stride,int fmt,const void *src,int srcc) {
  if ((texid<1)||(texid>render->texturec)) return -1;
  struct render_texture *texture=render->texturev+texid-1;
  
  /* If format is completely unspecified, (src) may be an encoded image.
   * Not permitted for texid 1.
   */
  if (!w&&!h&&!stride&&!fmt) {
    if (texid==1) return -1;
    struct rawimg *rawimg=rawimg_decode(src,srcc);
    if (!rawimg) return -1;
    if (!(fmt=render_texture_fmt_from_rawimg(rawimg))) {
      rawimg_del(rawimg);
      return -1;
    }
    if (render_texture_upload(texture,rawimg->w,rawimg->h,rawimg->stride,fmt,rawimg->v)<0) {
      rawimg_del(rawimg);
      return -1;
    }
    rawimg_del(rawimg);
    return 0;
  }
  
  /* Texid 1, dimensions must not change, except the first call.
   */
  if (texid==1) {
    if (texture->w&&texture->h) {
      if ((w!=texture->w)||(h!=texture->h)) return -1;
    }
  }
  
  /* Validate length, then upload.
   */
  int expectsrcc=render_texture_measure(w,h,stride,fmt);
  if ((expectsrcc<1)||(src&&(srcc<expectsrcc))) return -1;
  if (render_texture_upload(texture,w,h,stride,fmt,src)<0) return -1;
  return 0;
}
  
/* Get texture properties.
 */
 
void render_texture_get_header(int *w,int *h,int *fmt,const struct render *render,int texid) {
  if ((texid<1)||(texid>render->texturec)) return;
  struct render_texture *texture=render->texturev+texid-1;
  if (w) *w=texture->w;
  if (h) *h=texture->h;
  if (fmt) *fmt=texture->fmt;
}

/* Allocate framebuffer if needed.
 */
 
int render_texture_require_fb(struct render_texture *texture) {
  if (texture->fbid) return 0;
  glGenFramebuffers(1,&texture->fbid);
  if (!texture->fbid) {
    glGenFramebuffers(1,&texture->fbid);
    if (!texture->fbid) return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER,texture->fbid);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture->texid,0);
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  return 0;
}
