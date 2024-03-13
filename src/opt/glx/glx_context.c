#include "glx_internal.h"

/* Delete.
 */

void glx_del(struct glx *glx) {
  if (!glx) return;
  XCloseDisplay(glx->dpy);
  free(glx);
}

/* Estimate monitor size.
 * With Xinerama in play, we look for the smallest monitor.
 * Otherwise we end up with the logical desktop size, which is really never what you want.
 * Never fails to return something positive, even if it's a wild guess.
 */
 
static void glx_estimate_monitor_size(int *w,int *h,const struct glx *glx) {
  *w=*h=0;
  #if USE_xinerama
    int infoc=0;
    XineramaScreenInfo *infov=XineramaQueryScreens(glx->dpy,&infoc);
    if (infov) {
      if (infoc>0) {
        *w=infov[0].width;
        *h=infov[0].height;
        int i=infoc; while (i-->1) {
          if ((infov[i].width<*w)||(infov[i].height<*h)) {
            *w=infov[i].width;
            *h=infov[i].height;
          }
        }
      }
      XFree(infov);
    }
  #endif
  if ((*w<1)||(*h<1)) {
    *w=DisplayWidth(glx->dpy,0);
    *h=DisplayHeight(glx->dpy,0);
    if ((*w<1)||(*h<1)) {
      *w=640;
      *h=480;
    }
  }
}

/* Set window title.
 */
 
static void glx_set_title(struct glx *glx,const char *src) {
  
  // I've seen these properties in GNOME 2, unclear whether they might still be in play:
  XTextProperty prop={.value=(void*)src,.encoding=glx->atom_STRING,.format=8,.nitems=0};
  while (prop.value[prop.nitems]) prop.nitems++;
  XSetWMName(glx->dpy,glx->win,&prop);
  XSetWMIconName(glx->dpy,glx->win,&prop);
  XSetTextProperty(glx->dpy,glx->win,&prop,glx->atom__NET_WM_ICON_NAME);
    
  // This one becomes the window title and bottom-bar label, in GNOME 3:
  prop.encoding=glx->atom_UTF8_STRING;
  XSetTextProperty(glx->dpy,glx->win,&prop,glx->atom__NET_WM_NAME);
    
  // This daffy bullshit becomes the Alt-Tab text in GNOME 3:
  {
    char tmp[256];
    int len=prop.nitems+1+prop.nitems;
    if (len<sizeof(tmp)) {
      memcpy(tmp,prop.value,prop.nitems);
      tmp[prop.nitems]=0;
      memcpy(tmp+prop.nitems+1,prop.value,prop.nitems);
      tmp[prop.nitems+1+prop.nitems]=0;
      prop.value=tmp;
      prop.nitems=prop.nitems+1+prop.nitems;
      prop.encoding=glx->atom_STRING;
      XSetTextProperty(glx->dpy,glx->win,&prop,glx->atom_WM_CLASS);
    }
  }
}

/* Set window icon.
 */
 
static void glx_copy_icon_pixels(long *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=4) {
    *dst=(src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
  }
}
 
static void glx_set_icon(struct glx *glx,const void *rgba,int w,int h) {
  if (!rgba||(w<1)||(h<1)||(w>GLX_ICON_SIZE_LIMIT)||(h>GLX_ICON_SIZE_LIMIT)) return;
  int length=2+w*h;
  long *pixels=malloc(sizeof(long)*length);
  if (!pixels) return;
  pixels[0]=w;
  pixels[1]=h;
  glx_copy_icon_pixels(pixels+2,rgba,w*h);
  XChangeProperty(glx->dpy,glx->win,glx->atom__NET_WM_ICON,XA_CARDINAL,32,PropModeReplace,(unsigned char*)pixels,length);
  free(pixels);
}

/* GLXFBConfig.
 */
 
static GLXFBConfig glx_get_fbconfig(struct glx *glx) {

  int attrv[]={
    GLX_X_RENDERABLE,1,
    GLX_DRAWABLE_TYPE,GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE,GLX_TRUE_COLOR,
    GLX_RED_SIZE,8,
    GLX_GREEN_SIZE,8,
    GLX_BLUE_SIZE,8,
    GLX_ALPHA_SIZE,0,
    GLX_DEPTH_SIZE,0,
    GLX_STENCIL_SIZE,0,
    GLX_DOUBLEBUFFER,1,
  0};
  
  if (!glXQueryVersion(glx->dpy,&glx->glx_version_major,&glx->glx_version_minor)) {
    return (GLXFBConfig){0};
  }
  
  int fbc=0;
  GLXFBConfig *configv=glXChooseFBConfig(glx->dpy,glx->screen,attrv,&fbc);
  if (!configv||(fbc<1)) return (GLXFBConfig){0};
  GLXFBConfig config=configv[0];
  XFree(configv);
  
  return config;
}

/* Initialize. Display is opened, and nothing else.
 */
 
static int glx_init(struct glx *glx,const struct glx_setup *setup) {
  glx->screen=DefaultScreen(glx->dpy);
  
  #define GETATOM(tag) glx->atom_##tag=XInternAtom(glx->dpy,#tag,0);
  GETATOM(WM_PROTOCOLS)
  GETATOM(WM_DELETE_WINDOW)
  GETATOM(_NET_WM_STATE)
  GETATOM(_NET_WM_STATE_FULLSCREEN)
  GETATOM(_NET_WM_STATE_ADD)
  GETATOM(_NET_WM_STATE_REMOVE)
  GETATOM(_NET_WM_ICON)
  GETATOM(_NET_WM_ICON_NAME)
  GETATOM(_NET_WM_NAME)
  GETATOM(STRING)
  GETATOM(UTF8_STRING)
  GETATOM(WM_CLASS)
  #undef GETATOM
  
  // Choose window size.
  if ((setup->w>0)&&(setup->h>0)) {
    glx->w=setup->w;
    glx->h=setup->h;
  } else {
    int monw,monh;
    glx_estimate_monitor_size(&monw,&monh,glx);
    // There's usually some fixed chrome on both the main screen and around the window.
    // If we got a reasonable size, cut it to 3/4 to accomodate that chrome.
    if ((monw>=256)&&(monh>=256)) {
      monw=(monw*3)>>2;
      monh=(monh*3)>>2;
    }
    if ((setup->fbw>0)&&(setup->fbh>0)) {
      int xscale=monw/setup->fbw;
      int yscale=monh/setup->fbh;
      int scale=(xscale<yscale)?xscale:yscale;
      if (scale<1) scale=1;
      glx->w=setup->fbw*scale;
      glx->h=setup->fbh*scale;
    } else {
      glx->w=monw;
      glx->h=monh;
    }
  }
  
  // Query GLX context.
  GLXFBConfig fbconfig=glx_get_fbconfig(glx);
  XVisualInfo *vi=glXGetVisualFromFBConfig(glx->dpy,fbconfig);
  if (!vi) return -1;
  
  // Create window.
  XSetWindowAttributes wattr={
    .event_mask=
      StructureNotifyMask|
      KeyPressMask|KeyReleaseMask|
      FocusChangeMask|
    0,
  };
  if (glx->delegate.cb_mmotion) {
    wattr.event_mask|=PointerMotionMask;
  }
  if (glx->delegate.cb_mbutton||glx->delegate.cb_mwheel) {
    wattr.event_mask|=ButtonPressMask|ButtonReleaseMask;
  }
  wattr.colormap=XCreateColormap(glx->dpy,RootWindow(glx->dpy,vi->screen),vi->visual,AllocNone);
  glx->win=XCreateWindow(
    glx->dpy,RootWindow(glx->dpy,glx->screen),
    0,0,glx->w,glx->h,0,
    vi->depth,InputOutput,vi->visual,
    CWBorderPixel|CWColormap|CWEventMask,&wattr
  );
  XFree(vi);
  if (!glx->win) return -1;
  
  // Create OpenGL context.
  if (!(glx->ctx=glXCreateNewContext(glx->dpy,fbconfig,GLX_RGBA_TYPE,0,1))) return -1;
  glXMakeCurrent(glx->dpy,glx->win,glx->ctx);
  
  // Title and icon, if provided.
  if (setup->title&&setup->title[0]) {
    glx_set_title(glx,setup->title);
  }
  if (setup->iconrgba&&(setup->iconw>0)&&(setup->iconh>0)) {
    glx_set_icon(glx,setup->iconrgba,setup->iconw,setup->iconh);
  }
  
  // Fullscreen if requested.
  if (setup->fullscreen) {
    XChangeProperty(
      glx->dpy,glx->win,
      glx->atom__NET_WM_STATE,
      XA_ATOM,32,PropModeReplace,
      (unsigned char*)&glx->atom__NET_WM_STATE_FULLSCREEN,1
    );
    glx->fullscreen=1;
  }
  
  // Enable window.
  XMapWindow(glx->dpy,glx->win);
  XSync(glx->dpy,0);
  XSetWMProtocols(glx->dpy,glx->win,&glx->atom_WM_DELETE_WINDOW,1);
  
  // Hide cursor.
  glx->cursor_visible=1; // x11's default...
  glx_show_cursor(glx,0); // our default
  
  return 0;
}

/* New.
 */

struct glx *glx_new(
  const struct glx_delegate *delegate,
  const struct glx_setup *setup
) {
  struct glx *glx=calloc(1,sizeof(struct glx));
  if (!glx) return 0;
  
  if (!(glx->dpy=XOpenDisplay(setup->device))) {
    free(glx);
    return 0;
  }
  glx->delegate=*delegate;
  
  if (glx_init(glx,setup)<0) {
    glx_del(glx);
    return 0;
  }
  return glx;
}

/* Begin frame.
 */

int glx_begin(struct glx *glx) {
  glXMakeCurrent(glx->dpy,glx->win,glx->ctx);
  return 0;
}

/* End frame.
 */
 
int glx_end(struct glx *glx) {
  glx->screensaver_suppressed=0;
  glXSwapBuffers(glx->dpy,glx->win);
  return 0;
}

/* Show/hide cursor.
 */

void glx_show_cursor(struct glx *glx,int show) {
  if (show) {
    if (glx->cursor_visible) return;
    XDefineCursor(glx->dpy,glx->win,0);
    glx->cursor_visible=1;
  } else {
    if (!glx->cursor_visible) return;
    XColor color;
    Pixmap pixmap=XCreateBitmapFromData(glx->dpy,glx->win,"\0\0\0\0\0\0\0\0",1,1);
    Cursor cursor=XCreatePixmapCursor(glx->dpy,pixmap,pixmap,&color,&color,0,0);
    XDefineCursor(glx->dpy,glx->win,cursor);
    XFreeCursor(glx->dpy,cursor);
    XFreePixmap(glx->dpy,pixmap);
    glx->cursor_visible=0;
  }
}

/* Fullscreen.
 */
 
static void glx_change_fullscreen(struct glx *glx,int fullscreen) {
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=glx->atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=glx->win,
      .data={.l={
        fullscreen,
        glx->atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(glx->dpy,RootWindow(glx->dpy,glx->screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(glx->dpy);
}
 
void glx_set_fullscreen(struct glx *glx,int fullscreen) {
  if (fullscreen) {
    if (glx->fullscreen) return;
    glx_change_fullscreen(glx,1);
    glx->fullscreen=1;
  } else {
    if (!glx->fullscreen) return;
    glx_change_fullscreen(glx,0);
    glx->fullscreen=0;
  }
}

/* Suppress screensaver.
 */
 
void glx_suppress_screensaver(struct glx *glx) {
  if (glx->screensaver_suppressed) return;
  glx->screensaver_suppressed=1;
  XForceScreenSaver(glx->dpy,ScreenSaverReset);
}

/* Trivial accessors.
 */
 
void glx_get_size(int *w,int *h,const struct glx *glx) {
  *w=glx->w;
  *h=glx->h;
}

int glx_get_fullscreen(const struct glx *glx) {
  return glx->fullscreen;
}
