#ifndef X11FB_INTERNAL_H
#define X11FB_INTERNAL_H

#include "x11fb.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#if USE_xinerama
  #include <X11/extensions/Xinerama.h>
#endif

#define KeyRepeat (LASTEvent+2)
#define X11FB_KEY_REPEAT_INTERVAL 10
#define X11FB_ICON_SIZE_LIMIT 64

struct x11fb {
  struct x11fb_delegate delegate;
  
  Display *dpy;
  int screen;
  Window win;
  GC gc;
  int w,h;
  int fullscreen;
  
  Atom atom_WM_PROTOCOLS;
  Atom atom_WM_DELETE_WINDOW;
  Atom atom__NET_WM_STATE;
  Atom atom__NET_WM_STATE_FULLSCREEN;
  Atom atom__NET_WM_STATE_ADD;
  Atom atom__NET_WM_STATE_REMOVE;
  Atom atom__NET_WM_ICON;
  Atom atom__NET_WM_ICON_NAME;
  Atom atom__NET_WM_NAME;
  Atom atom_WM_CLASS;
  Atom atom_STRING;
  Atom atom_UTF8_STRING;
  
  int screensaver_suppressed;
  int focus;
  int cursor_visible;
  
  XImage *fb; // Created at init, never changes.
  XImage *fblarge; // Optional and volatile.
  int dstx,dsty,dstw,dsth;
  int dstdirty; // Nonzero to reconsider (dst*) and (fblarge) at next render.
};

XImage *x11fb_new_image(struct x11fb *x11fb,int w,int h);
void x11fb_scale(XImage *dst,struct x11fb *x11fb,XImage *src);

int x11fb_codepoint_from_keysym(int keysym);
int x11fb_usb_usage_from_keysym(int keysym);

#endif
