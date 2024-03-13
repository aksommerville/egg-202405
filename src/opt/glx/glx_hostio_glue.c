#include "glx_internal.h"
#include "opt/hostio/hostio_video.h"

/* Instance definition.
 */
 
struct hostio_video_glx {
  struct hostio_video hdr;
  struct glx *glx;
};

#define DRIVER ((struct hostio_video_glx*)driver)

/* Delete.
 */
 
static void _glx_del(struct hostio_video *driver) {
  glx_del(DRIVER->glx);
}

/* Resize.
 */
 
static void _glx_cb_resize(void *userdata,int w,int h) {
  struct hostio_video *driver=userdata;
  driver->w=w;
  driver->h=h;
  if (driver->delegate.cb_resize) driver->delegate.cb_resize(driver,w,h);
}

/* Init.
 */
 
static int _glx_init(struct hostio_video *driver,const struct hostio_video_setup *setup) {
  // glx's delegate and setup match hostio's exactly (not a coincidence).
  struct glx_delegate delegate;
  memcpy(&delegate,&driver->delegate,sizeof(struct glx_delegate));
  delegate.userdata=driver; // Only we need glx to trigger its callbacks with the hostio driver as first param.
  delegate.cb_resize=_glx_cb_resize; // Must intercept this one, though, to update the outer driver.
  if (!(DRIVER->glx=glx_new(&delegate,(struct glx_setup*)setup))) return -1;
  driver->w=DRIVER->glx->w;
  driver->h=DRIVER->glx->h;
  driver->fullscreen=DRIVER->glx->fullscreen;
  driver->cursor_visible=DRIVER->glx->cursor_visible;
  return 0;
}

/* Trivial pass-thru hooks.
 */
 
static int _glx_update(struct hostio_video *driver) {
  return glx_update(DRIVER->glx);
}

static void _glx_show_cursor(struct hostio_video *driver,int show) {
  glx_show_cursor(DRIVER->glx,show);
  driver->cursor_visible=DRIVER->glx->cursor_visible;
}

static void _glx_set_fullscreen(struct hostio_video *driver,int fullscreen) {
  glx_set_fullscreen(DRIVER->glx,fullscreen);
  driver->fullscreen=DRIVER->glx->fullscreen;
}

static void _glx_suppress_screensaver(struct hostio_video *driver) {
  glx_suppress_screensaver(DRIVER->glx);
}

static int _glx_begin(struct hostio_video *driver) {
  return glx_begin(DRIVER->glx);
}

static int _glx_end(struct hostio_video *driver) {
  return glx_end(DRIVER->glx);
}

/* Type definition.
 */
 
const struct hostio_video_type hostio_video_type_glx={
  .name="glx",
  .desc="X11 with OpenGL. Preferred for most Linux systems.",
  .objlen=sizeof(struct hostio_video_glx),
  .appointment_only=0,
  .provides_input=1,
  .del=_glx_del,
  .init=_glx_init,
  .update=_glx_update,
  .show_cursor=_glx_show_cursor,
  .set_fullscreen=_glx_set_fullscreen,
  .suppress_screensaver=_glx_suppress_screensaver,
  .gx_begin=_glx_begin,
  .gx_end=_glx_end,
};
