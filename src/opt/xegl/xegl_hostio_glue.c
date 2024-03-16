#include "xegl_internal.h"
#include "opt/hostio/hostio_video.h"

/* Instance definition.
 */
 
struct hostio_video_xegl {
  struct hostio_video hdr;
  struct xegl *xegl;
};

#define DRIVER ((struct hostio_video_xegl*)driver)

/* Delete.
 */
 
static void _xegl_del(struct hostio_video *driver) {
  xegl_del(DRIVER->xegl);
}

/* Resize.
 */
 
static void _xegl_cb_resize(void *userdata,int w,int h) {
  struct hostio_video *driver=userdata;
  driver->w=w;
  driver->h=h;
  if (driver->delegate.cb_resize) driver->delegate.cb_resize(driver,w,h);
}

/* Init.
 */
 
static int _xegl_init(struct hostio_video *driver,const struct hostio_video_setup *setup) {
  // xegl's delegate and setup match hostio's exactly (not a coincidence).
  struct xegl_delegate delegate;
  memcpy(&delegate,&driver->delegate,sizeof(struct xegl_delegate));
  delegate.userdata=driver; // Only we need xegl to trigger its callbacks with the hostio driver as first param.
  delegate.cb_resize=_xegl_cb_resize; // Must intercept this one, though, to update the outer driver.
  if (!(DRIVER->xegl=xegl_new(&delegate,(struct xegl_setup*)setup))) return -1;
  driver->w=DRIVER->xegl->w;
  driver->h=DRIVER->xegl->h;
  driver->fullscreen=DRIVER->xegl->fullscreen;
  driver->cursor_visible=DRIVER->xegl->cursor_visible;
  return 0;
}

/* Trivial pass-thru hooks.
 */
 
static int _xegl_update(struct hostio_video *driver) {
  return xegl_update(DRIVER->xegl);
}

static void _xegl_show_cursor(struct hostio_video *driver,int show) {
  xegl_show_cursor(DRIVER->xegl,show);
  driver->cursor_visible=DRIVER->xegl->cursor_visible;
}

static void _xegl_set_fullscreen(struct hostio_video *driver,int fullscreen) {
  xegl_set_fullscreen(DRIVER->xegl,fullscreen);
  driver->fullscreen=DRIVER->xegl->fullscreen;
}

static void _xegl_suppress_screensaver(struct hostio_video *driver) {
  xegl_suppress_screensaver(DRIVER->xegl);
}

static int _xegl_begin(struct hostio_video *driver) {
  return xegl_begin(DRIVER->xegl);
}

static int _xegl_end(struct hostio_video *driver) {
  return xegl_end(DRIVER->xegl);
}

/* Type definition.
 */
 
const struct hostio_video_type hostio_video_type_xegl={
  .name="xegl",
  .desc="X11 with OpenGL. Preferred for most Linux systems.",
  .objlen=sizeof(struct hostio_video_xegl),
  .appointment_only=0,
  .provides_input=1,
  .del=_xegl_del,
  .init=_xegl_init,
  .update=_xegl_update,
  .show_cursor=_xegl_show_cursor,
  .set_fullscreen=_xegl_set_fullscreen,
  .suppress_screensaver=_xegl_suppress_screensaver,
  .gx_begin=_xegl_begin,
  .gx_end=_xegl_end,
};
