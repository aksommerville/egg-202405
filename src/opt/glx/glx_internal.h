#ifndef GLX_INTERNAL_H
#define GLX_INTERNAL_H

#include "akglx.h"
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
#include <GL/glx.h>

#if USE_xinerama
  #include <X11/extensions/Xinerama.h>
#endif

#define KeyRepeat (LASTEvent+2)
#define GLX_KEY_REPEAT_INTERVAL 10
#define GLX_ICON_SIZE_LIMIT 64

struct glx {
  struct glx_delegate delegate;
  
  Display *dpy;
  int screen;
  Window win;
  int w,h;
  int fullscreen;
  
  GLXContext ctx;
  int glx_version_major,glx_version_minor;
  
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
};

int glx_codepoint_from_keysym(int keysym);
int glx_usb_usage_from_keysym(int keysym);

#endif
