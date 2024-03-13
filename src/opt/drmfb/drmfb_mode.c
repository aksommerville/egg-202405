#include "drmfb_internal.h"
#include <dirent.h>

/* Iteration context.
 */
 
struct drmfb_mode_iterator {
  struct drmfb *drmfb;
  DIR *dir;
  int (*cb)(const struct drmfb_mode *mode,void *userdata);
  void *userdata;
  int fd; // init to <0
  drmModeResPtr res;
  struct drmfb_mode mode;
};

static void drmfb_mode_iterator_close_file(struct drmfb_mode_iterator *ctx) {
  if (ctx->fd>=0) {
    if (ctx->res) drmModeFreeResources(ctx->res);
    close(ctx->fd);
  }
  ctx->fd=-1;
  ctx->res=0;
}

static void drmfb_mode_iterator_cleanup(struct drmfb_mode_iterator *ctx) {
  if (ctx->dir) closedir(ctx->dir);
  drmfb_mode_iterator_close_file(ctx);
}

/* Try one file.
 */
 
static int drmfb_try_file(struct drmfb_mode_iterator *ctx,const char *base) {
  if (memcmp(base,"card",4)||!base[4]) return 0;
  int kid=0,basep=4;
  for (;base[basep];basep++) {
    int digit=base[basep]-'0';
    if ((digit<0)||(digit>9)) return 0;
    kid*=10;
    kid+=digit;
    if (kid>999999) return 0;
  }
  char path[256];
  int pathc=snprintf(path,sizeof(path),"/dev/dri/%s",base);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  drmfb_mode_iterator_close_file(ctx);
  if ((ctx->fd=open(path,O_RDONLY))<0) return 0;
  ctx->mode.kid=kid;
  
  if (!(ctx->res=drmModeGetResources(ctx->fd))) {
    fprintf(stderr,"drmModeGetResources: %m\n");
    return 0;
  }
  
  int conni=0;
  for (;conni<ctx->res->count_connectors;conni++) {
    drmModeConnectorPtr connq=drmModeGetConnector(ctx->fd,ctx->res->connectors[conni]);
    if (!connq) continue;
    ctx->mode.connector_id=connq->connector_id;
    /**
    fprintf(stderr,
      "  CONNECTOR %d %s %dx%dmm type=%08x typeid=%08x\n",
      connq->connector_id,
      (connq->connection==DRM_MODE_CONNECTED)?"connected":"disconnected",
      connq->mmWidth,connq->mmHeight,
      connq->connector_type,
      connq->connector_type_id
    );
    /**/
    if (connq->connection!=DRM_MODE_CONNECTED) {
      drmModeFreeConnector(connq);
      continue;
    }
    int modei=0;
    for (;modei<connq->count_modes;modei++) { 
      drmModeModeInfoPtr modeq=connq->modes+modei;
      /**
      fprintf(stderr,
        "    MODE %dx%d @ %d Hz, flags=%08x, type=%08x\n",
        modeq->hdisplay,modeq->vdisplay,
        modeq->vrefresh,
        modeq->flags,modeq->type
      );
      /**/
      ctx->mode.modep=modei;
      ctx->mode.w=modeq->hdisplay;
      ctx->mode.h=modeq->vdisplay;
      ctx->mode.rate=modeq->vrefresh;
      ctx->mode.already=(modeq->type&DRM_MODE_TYPE_PREFERRED)?1:0;
      int err=ctx->cb(&ctx->mode,ctx->userdata);
      if (err) {
        drmModeFreeConnector(connq);
        return err;
      }
    }
    drmModeFreeConnector(connq);
  }
  
  return 0;
}

/* Iterate modes, in context.
 */
 
static int drmfb_for_each_mode_inner(struct drmfb_mode_iterator *ctx) {
  if (!(ctx->dir=opendir("/dev/dri"))) return -1;
  struct dirent *de;
  while (de=readdir(ctx->dir)) {
    if (de->d_type!=DT_CHR) continue;
    int err=drmfb_try_file(ctx,de->d_name);
    if (err) return err;
  }
  return 0;
}

/* Discover and report available modes.
 */
 
int drmfb_for_each_mode(
  struct drmfb *drmfb,
  int (*cb)(const struct drmfb_mode *mode,void *userdata),
  void *userdata
) {
  struct drmfb_mode_iterator ctx={.drmfb=drmfb,.cb=cb,.userdata=userdata,.fd=-1};
  int err=drmfb_for_each_mode_inner(&ctx);
  drmfb_mode_iterator_cleanup(&ctx);
  return err;
}

/* Initialize framebuffer.
 */
 
static int drmfb_fb_init(
  struct drmfb *drmfb,
  struct drmfb_fb *fb
) {

  struct drm_mode_create_dumb creq={
    .width=drmfb->mode.w,
    .height=drmfb->mode.h,
    .bpp=32,
    .flags=0,
  };
  if (ioctl(drmfb->fd,DRM_IOCTL_MODE_CREATE_DUMB,&creq)<0) {
    fprintf(stderr,"DRM_IOCTL_MODE_CREATE_DUMB: %m\n");
    return -1;
  }
  fb->handle=creq.handle;
  fb->size=creq.size;
  fb->stridewords=creq.pitch;
  fb->stridewords>>=2;
  
  if (drmModeAddFB(
    drmfb->fd,
    creq.width,creq.height,
    32,
    creq.bpp,creq.pitch,
    fb->handle,
    &fb->fbid
  )<0) {
    fprintf(stderr,"drmModeAddFB: %m\n");
    return -1;
  }
  
  struct drm_mode_map_dumb mreq={
    .handle=fb->handle,
  };
  if (ioctl(drmfb->fd,DRM_IOCTL_MODE_MAP_DUMB,&mreq)<0) {
    fprintf(stderr,"DRM_IOCTL_MODE_MAP_DUMB: %m\n");
    return -1;
  }
  
  fb->v=mmap(0,fb->size,PROT_READ|PROT_WRITE,MAP_SHARED,drmfb->fd,mreq.offset);
  if (fb->v==MAP_FAILED) {
    fprintf(stderr,"mmap: %m\n");
    return -1;
  }
  
  return 0;
}

/* Set mode, found DRM object.
 */
 
static int drmfb_commit_mode(
  struct drmfb *drmfb,
  drmModeResPtr res,
  drmModeConnectorPtr connq,
  drmModeModeInfoPtr modeq,
  const struct drmfb_mode *mode
) {
  drmfb->drmmode=*modeq;
  drmfb->mode=*mode;
  drmfb->connid=connq->connector_id;
  drmfb->encid=connq->encoder_id;
  
  drmModeEncoderPtr encoder=drmModeGetEncoder(drmfb->fd,drmfb->encid);
  if (!encoder) return -1;
  drmfb->crtcid=encoder->crtc_id;
  drmModeFreeEncoder(encoder);
  
  // Store the current CRTC so we can restore it at quit.
  if (!(drmfb->crtc_restore=drmModeGetCrtc(drmfb->fd,drmfb->crtcid))) return -1;
  
  if (drmfb_fb_init(drmfb,drmfb->fbv+0)<0) return -1;
  if (drmfb_fb_init(drmfb,drmfb->fbv+1)<0) return -1;
  
  /* Take format from the first framebuffer (assume they are identical; we created them the same way).
   */
  struct drm_mode_fb_cmd2 cmd={
    .fb_id=drmfb->fbv[0].fbid,
  };
  if (ioctl(drmfb->fd,DRM_IOCTL_MODE_GETFB2,&cmd)<0) {
    fprintf(stderr,"DRM_IOCTL_MODE_GETFB2: %m\n");
    return -1;
  }
  drmfb->pixel_format=cmd.pixel_format;
  
  /* This is where I normally fail, if try to launch with X running.
   * (not that one expects that to work).
   */
  if (drmModeSetCrtc(
    drmfb->fd,
    drmfb->crtcid,
    drmfb->fbv[0].fbid,
    0,0,
    &drmfb->connid,
    1,
    &drmfb->drmmode
  )<0) {
    fprintf(stderr,"drmModeSetCrtc: %m\n");
    return -1;
  }

  return 0;
}

/* Set mode.
 */
 
int drmfb_set_mode(struct drmfb *drmfb,const struct drmfb_mode *mode) {
  if (!drmfb||!mode) return -1;
  if (mode->modep<0) return -1;
  if (drmfb->fd>=0) return -1; // Forbid changing once set. TODO Can we allow it?
  
  char path[256];
  int pathc=snprintf(path,sizeof(path),"/dev/dri/card%d",mode->kid);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  if ((drmfb->fd=open(path,O_RDWR))<0) return -1;
  
  drmModeResPtr res=drmModeGetResources(drmfb->fd);
  if (!res) {
    fprintf(stderr,"drmModeGetResources: %m\n");
    close(drmfb->fd);
    drmfb->fd=-1;
    return 0;
  }
  
  int conni=0;
  for (;conni<res->count_connectors;conni++) {
    drmModeConnectorPtr connq=drmModeGetConnector(drmfb->fd,res->connectors[conni]);
    if (!connq) continue;
    if (connq->connector_id!=mode->connector_id) {
      drmModeFreeConnector(connq);
      continue;
    }
    if (mode->modep>=connq->count_modes) {
      drmModeFreeConnector(connq);
      drmModeFreeResources(res);
      close(drmfb->fd);
      drmfb->fd=-1;
      return -1;
    }
    drmModeModeInfoPtr modeq=connq->modes+mode->modep;
    if (
      (modeq->hdisplay!=mode->w)||
      (modeq->vdisplay!=mode->h)||
      (modeq->vrefresh!=mode->rate)
    ) {
      drmModeFreeConnector(connq);
      drmModeFreeResources(res);
      close(drmfb->fd);
      drmfb->fd=-1;
      return -1;
    }
    
    int err=drmfb_commit_mode(drmfb,res,connq,modeq,mode);
    drmModeFreeConnector(connq);
    drmModeFreeResources(res);
    if (err<0) {
      close(drmfb->fd);
      drmfb->fd=-1;
      return -1;
    }
    return 0;
  }
  drmModeFreeResources(res);
  close(drmfb->fd);
  drmfb->fd=-1;
  return -1;
}
