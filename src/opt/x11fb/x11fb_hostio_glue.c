#include "x11fb_internal.h"
#include "opt/hostio/hostio_video.h"

/* Instance definition.
 */
 
struct hostio_video_x11fb {
  struct hostio_video hdr;
  struct x11fb *x11fb;
};

#define DRIVER ((struct hostio_video_x11fb*)driver)

/* Delete.
 */
 
static void _x11fb_del(struct hostio_video *driver) {
  x11fb_del(DRIVER->x11fb);
}

/* Resize.
 */
 
static void _x11fb_cb_resize(void *userdata,int w,int h) {
  struct hostio_video *driver=userdata;
  driver->w=w;
  driver->h=h;
  if (driver->delegate.cb_resize) driver->delegate.cb_resize(driver,w,h);
}

/* Init.
 */
 
static int _x11fb_init(struct hostio_video *driver,const struct hostio_video_setup *setup) {
  // x11fb's delegate and setup match hostio's exactly (not a coincidence).
  struct x11fb_delegate delegate;
  memcpy(&delegate,&driver->delegate,sizeof(struct x11fb_delegate));
  delegate.userdata=driver; // Only we need x11fb to trigger its callbacks with the hostio driver as first param.
  delegate.cb_resize=_x11fb_cb_resize; // Must intercept this one, though, to update the outer driver.
  if (!(DRIVER->x11fb=x11fb_new(&delegate,(struct x11fb_setup*)setup))) return -1;
  driver->w=DRIVER->x11fb->w;
  driver->h=DRIVER->x11fb->h;
  driver->fullscreen=DRIVER->x11fb->fullscreen;
  driver->cursor_visible=DRIVER->x11fb->cursor_visible;
  return 0;
}

/* Describe framebuffer.
 */
 
static int _x11fb_fb_describe(struct hostio_video_fb_description *desc,struct hostio_video *driver) {
  x11fb_get_framebuffer_geometry(&desc->w,&desc->h,&desc->stride,&desc->pixelsize,DRIVER->x11fb);
  x11fb_get_framebuffer_masks(&desc->rmask,&desc->gmask,&desc->bmask,&desc->amask,DRIVER->x11fb);
  x11fb_get_framebuffer_chorder(desc->chorder,DRIVER->x11fb);
  return 0;
}

/* Trivial pass-thru hooks.
 */
 
static int _x11fb_update(struct hostio_video *driver) {
  return x11fb_update(DRIVER->x11fb);
}

static void _x11fb_show_cursor(struct hostio_video *driver,int show) {
  x11fb_show_cursor(DRIVER->x11fb,show);
  driver->cursor_visible=DRIVER->x11fb->cursor_visible;
}

static void _x11fb_set_fullscreen(struct hostio_video *driver,int fullscreen) {
  x11fb_set_fullscreen(DRIVER->x11fb,fullscreen);
  driver->fullscreen=DRIVER->x11fb->fullscreen;
}

static void _x11fb_suppress_screensaver(struct hostio_video *driver) {
  x11fb_suppress_screensaver(DRIVER->x11fb);
}

static void *_x11fb_begin(struct hostio_video *driver) {
  return x11fb_begin(DRIVER->x11fb);
}

static int _x11fb_end(struct hostio_video *driver) {
  return x11fb_end(DRIVER->x11fb);
}

/* Type definition.
 */
 
const struct hostio_video_type hostio_video_type_x11fb={
  .name="x11fb",
  .desc="X11 with no graphics acceleration.",
  .objlen=sizeof(struct hostio_video_x11fb),
  .appointment_only=0,
  .provides_input=1,
  .del=_x11fb_del,
  .init=_x11fb_init,
  .update=_x11fb_update,
  .show_cursor=_x11fb_show_cursor,
  .set_fullscreen=_x11fb_set_fullscreen,
  .suppress_screensaver=_x11fb_suppress_screensaver,
  .fb_describe=_x11fb_fb_describe,
  .fb_begin=_x11fb_begin,
  .fb_end=_x11fb_end,
};
