/* akglx.h
 * Required:
 * Optional: hostio(drop glue file if absent) xinerama(not a real unit)
 * Link: -lX11 (-lXinerama) -lGL
 *
 * Interface to X11 with OpenGL.
 * Note that the unit is properly called "glx".
 * This file is "akglx.h" to avoid conflict with standard headers.
 * "glx" and "x11fb" could have been implemented as one unit.
 * I chose to separate them so that x11fb consumers don't need to link pointlessly against OpenGL.
 */
 
#ifndef AKGLX_H
#define AKGLX_H

struct glx;

// Identical to hostio_video_delegate, except we call with (userdata) instead of (driver).
// When hostio is in use, callbacks pass through directly.
struct glx_delegate {
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
struct glx_setup {
  const char *title;
  const void *iconrgba;
  int iconw,iconh;
  int w,h; // Actual window. Typically (0,0) to let us decide.
  int fullscreen;
  int fbw,fbh; // Only for guiding initial window size decision.
  const char *device; // DISPLAY
};

void glx_del(struct glx *glx);

struct glx *glx_new(
  const struct glx_delegate *delegate,
  const struct glx_setup *setup
);

int glx_update(struct glx *glx);

/* The OpenGL context is guaranteed in scope only between begin and end.
 */
int glx_begin(struct glx *glx);
int glx_end(struct glx *glx);

/* Odds and ends.
 * The cursor is initially hidden.
 * Initial fullscreen state is specified in setup.
 */
void glx_show_cursor(struct glx *glx,int show);
void glx_set_fullscreen(struct glx *glx,int fullscreen);
void glx_suppress_screensaver(struct glx *glx);

void glx_get_size(int *w,int *h,const struct glx *glx);
int glx_get_fullscreen(const struct glx *glx);

#endif
