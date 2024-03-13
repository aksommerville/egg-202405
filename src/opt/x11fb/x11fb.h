/* x11fb.h
 * Required:
 * Optional: hostio(drop glue file if absent) xinerama(not a real unit)
 * Link: -lX11 (-lXinerama)
 *
 * Interface to X11 with no acceleration.
 * This is probably not what you want in the 21st century: If the machine supports X11, it probably does some GX too.
 */
 
#ifndef X11FB_H
#define X11FB_H

struct x11fb;

// Identical to hostio_video_delegate, except we call with (userdata) instead of (driver).
// When hostio is in use, callbacks pass through directly.
struct x11fb_delegate {
  void *userdata;
  void (*cb_close)(void *userdata);
  void (*cb_focus)(void *userdata,int focus);
  void (*cb_resize)(void *userdata,int w,int h);
  int (*cb_key)(void *userdata,int keycode,int value);
  void (*cb_text)(void *userdata,int codepoint);
  void (*cb_mmotion)(void *userdata,int x,int y);
  void (*cb_mbutton)(void *userdata,int btnid,int value);
  void (*cb_mwheel)(void *userdata,int dx,int dy);
};

// Identical to hostio_video_setup.
struct x11fb_setup {
  const char *title;
  const void *iconrgba;
  int iconw,iconh;
  int w,h; // Actual window. Typically (0,0) to let us decide.
  int fullscreen;
  int fbw,fbh; // Exposed framebuffer.
  const char *device; // DISPLAY
};

void x11fb_del(struct x11fb *x11fb);

struct x11fb *x11fb_new(
  const struct x11fb_delegate *delegate,
  const struct x11fb_setup *setup
);

int x11fb_update(struct x11fb *x11fb);

/* Begin to get a framebuffer ready to draw to, then end to commit it.
 */
void *x11fb_begin(struct x11fb *x11fb);
int x11fb_end(struct x11fb *x11fb);

/* Framebuffer properties won't change after construction.
 * "masks" for channel positions when read as native words.
 * Irrelevant for pixelsize other than 8, 16, 32.
 * "chorder" for bytewise channels (eg 24-bit), returns "rgbayx\0" in storage order.
 */
void x11fb_get_framebuffer_geometry(
  int *w,int *h,int *stride,int *pixelsize,
  const struct x11fb *x11fb
);
void x11fb_get_framebuffer_masks(
  int *r,int *g,int *b,int *a,
  const struct x11fb *x11fb
);
void x11fb_get_framebuffer_chorder(
  char *chorder, // 4 bytes
  const struct x11fb *x11fb
);

/* Odds and ends.
 * The cursor is initially hidden.
 * Initial fullscreen state is specified in setup.
 */
void x11fb_show_cursor(struct x11fb *x11fb,int show);
void x11fb_set_fullscreen(struct x11fb *x11fb,int fullscreen);
void x11fb_suppress_screensaver(struct x11fb *x11fb);

void x11fb_get_size(int *w,int *h,const struct x11fb *x11fb);
int x11fb_get_fullscreen(const struct x11fb *x11fb);

#endif
