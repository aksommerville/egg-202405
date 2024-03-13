#include "drmfb_internal.h"

/* Delete.
 */
 
static void drmfb_fb_cleanup(struct drmfb *drmfb,struct drmfb_fb *fb) {
  if (fb->fbid) drmModeRmFB(drmfb->fd,fb->fbid);
  if (fb->v) munmap(fb->v,fb->size);
}
 
void drmfb_del(struct drmfb *drmfb) {
  if (!drmfb) return;
  if (drmfb->fd>=0) {
    drmfb_fb_cleanup(drmfb,drmfb->fbv+0);
    drmfb_fb_cleanup(drmfb,drmfb->fbv+1);
    if (drmfb->crtc_restore) {
      drmModeCrtcPtr crtc=drmfb->crtc_restore;
      drmModeSetCrtc(
        drmfb->fd,
        crtc->crtc_id,
        crtc->buffer_id,
        crtc->x,
        crtc->y,
        &drmfb->connid,
        1,
        &crtc->mode
      );
      drmModeFreeCrtc(drmfb->crtc_restore);
    }
    close(drmfb->fd);
  }
  free(drmfb);
}

/* New.
 */

struct drmfb *drmfb_new() {
  struct drmfb *drmfb=calloc(1,sizeof(struct drmfb));
  if (!drmfb) return 0;
  drmfb->fd=-1;
  return drmfb;
}

/* Trivial accessors.
 */

const struct drmfb_mode *drmfb_get_mode(const struct drmfb *drmfb) {
  if (drmfb->fd<0) return 0;
  return &drmfb->mode;
}

int drmfb_get_fd(const struct drmfb *drmfb) {
  return drmfb->fd;
}

uint32_t drmfb_get_pixel_format(const struct drmfb *drmfb) {
  return drmfb->pixel_format;
}

/* Begin rendering.
 */

void *drmfb_begin(struct drmfb *drmfb) {
  if (drmfb->fd<0) return 0;
  drmfb->fbp^=1;
  struct drmfb_fb *fb=drmfb->fbv+drmfb->fbp;
  void *buffer=fb->v;
  return buffer;
}

/* End rendering.
 */
 
int drmfb_end(struct drmfb *drmfb) {
  if (drmfb->fd<0) return -1;
  struct drmfb_fb *fb=drmfb->fbv+drmfb->fbp;
  while (1) {
    if (drmModePageFlip(drmfb->fd,drmfb->crtcid,fb->fbid,0,drmfb)<0) {
      if (errno==EBUSY) { // waiting for prior flip
        usleep(1000);
        continue;
      }
      fprintf(stderr,"drmModePageFlip: %m\n");
      return -1;
    } else {
      break;
    }
  }
  return 0;
}
