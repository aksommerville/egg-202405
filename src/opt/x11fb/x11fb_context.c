#include "x11fb_internal.h"

/* Delete.
 */

void x11fb_del(struct x11fb *x11fb) {
  if (!x11fb) return;
  if (x11fb->fb) XDestroyImage(x11fb->fb);
  if (x11fb->fblarge) XDestroyImage(x11fb->fblarge);
  XCloseDisplay(x11fb->dpy);
  free(x11fb);
}

/* Estimate monitor size.
 * With Xinerama in play, we look for the smallest monitor.
 * Otherwise we end up with the logical desktop size, which is really never what you want.
 * Never fails to return something positive, even if it's a wild guess.
 */
 
static void x11fb_estimate_monitor_size(int *w,int *h,const struct x11fb *x11fb) {
  *w=*h=0;
  #if USE_xinerama
    int infoc=0;
    XineramaScreenInfo *infov=XineramaQueryScreens(x11fb->dpy,&infoc);
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
    *w=DisplayWidth(x11fb->dpy,0);
    *h=DisplayHeight(x11fb->dpy,0);
    if ((*w<1)||(*h<1)) {
      *w=640;
      *h=480;
    }
  }
}

/* Set window title.
 */
 
static void x11fb_set_title(struct x11fb *x11fb,const char *src) {
  
  // I've seen these properties in GNOME 2, unclear whether they might still be in play:
  XTextProperty prop={.value=(void*)src,.encoding=x11fb->atom_STRING,.format=8,.nitems=0};
  while (prop.value[prop.nitems]) prop.nitems++;
  XSetWMName(x11fb->dpy,x11fb->win,&prop);
  XSetWMIconName(x11fb->dpy,x11fb->win,&prop);
  XSetTextProperty(x11fb->dpy,x11fb->win,&prop,x11fb->atom__NET_WM_ICON_NAME);
    
  // This one becomes the window title and bottom-bar label, in GNOME 3:
  prop.encoding=x11fb->atom_UTF8_STRING;
  XSetTextProperty(x11fb->dpy,x11fb->win,&prop,x11fb->atom__NET_WM_NAME);
    
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
      prop.encoding=x11fb->atom_STRING;
      XSetTextProperty(x11fb->dpy,x11fb->win,&prop,x11fb->atom_WM_CLASS);
    }
  }
}

/* Set window icon.
 */
 
static void x11fb_copy_icon_pixels(long *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=4) {
    *dst=(src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
  }
}
 
static void x11fb_set_icon(struct x11fb *x11fb,const void *rgba,int w,int h) {
  if (!rgba||(w<1)||(h<1)||(w>X11FB_ICON_SIZE_LIMIT)||(h>X11FB_ICON_SIZE_LIMIT)) return;
  int length=2+w*h;
  long *pixels=malloc(sizeof(long)*length);
  if (!pixels) return;
  pixels[0]=w;
  pixels[1]=h;
  x11fb_copy_icon_pixels(pixels+2,rgba,w*h);
  XChangeProperty(x11fb->dpy,x11fb->win,x11fb->atom__NET_WM_ICON,XA_CARDINAL,32,PropModeReplace,(unsigned char*)pixels,length);
  free(pixels);
}

/* Initialize. Display is opened, and nothing else.
 */
 
static int x11fb_init(struct x11fb *x11fb,const struct x11fb_setup *setup) {
  x11fb->screen=DefaultScreen(x11fb->dpy);
  
  #define GETATOM(tag) x11fb->atom_##tag=XInternAtom(x11fb->dpy,#tag,0);
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
  
  // Create framebuffer.
  if (!(x11fb->fb=x11fb_new_image(x11fb,setup->fbw,setup->fbh))) return -1;
  x11fb->dstdirty=1;
  
  // Choose window size.
  if ((setup->w>0)&&(setup->h>0)) {
    x11fb->w=setup->w;
    x11fb->h=setup->h;
  } else {
    int monw,monh;
    x11fb_estimate_monitor_size(&monw,&monh,x11fb);
    // There's usually some fixed chrome on both the main screen and around the window.
    // If we got a reasonable size, cut it to 7/8 to accomodate that chrome.
    if ((monw>=256)&&(monh>=256)) {
      monw=(monw*7)>>3;
      monh=(monh*7)>>3;
    }
    int xscale=monw/setup->fbw;
    int yscale=monh/setup->fbh;
    int scale=(xscale<yscale)?xscale:yscale;
    if (scale<1) scale=1;
    x11fb->w=setup->fbw*scale;
    x11fb->h=setup->fbh*scale;
  }
  
  // Create window and graphics context.
  XSetWindowAttributes wattr={
    .event_mask=
      StructureNotifyMask|
      KeyPressMask|KeyReleaseMask|
      FocusChangeMask|
    0,
  };
  if (x11fb->delegate.cb_mmotion) {
    wattr.event_mask|=PointerMotionMask;
  }
  if (x11fb->delegate.cb_mbutton||x11fb->delegate.cb_mwheel) {
    wattr.event_mask|=ButtonPressMask|ButtonReleaseMask;
  }
  if (!(x11fb->win=XCreateWindow(
    x11fb->dpy,RootWindow(x11fb->dpy,x11fb->screen),
    0,0,x11fb->w,x11fb->h,0,
    DefaultDepth(x11fb->dpy,x11fb->screen),InputOutput,CopyFromParent,
    CWBorderPixel|CWEventMask,&wattr
  ))) return -1;
  if (!(x11fb->gc=XCreateGC(x11fb->dpy,x11fb->win,0,0))) return -1;
  
  // Title and icon, if provided.
  if (setup->title&&setup->title[0]) {
    x11fb_set_title(x11fb,setup->title);
  }
  if (setup->iconrgba&&(setup->iconw>0)&&(setup->iconh>0)) {
    x11fb_set_icon(x11fb,setup->iconrgba,setup->iconw,setup->iconh);
  }
  
  // Fullscreen if requested.
  if (setup->fullscreen) {
    XChangeProperty(
      x11fb->dpy,x11fb->win,
      x11fb->atom__NET_WM_STATE,
      XA_ATOM,32,PropModeReplace,
      (unsigned char*)&x11fb->atom__NET_WM_STATE_FULLSCREEN,1
    );
    x11fb->fullscreen=1;
  }
  
  // Enable window.
  XMapWindow(x11fb->dpy,x11fb->win);
  XSync(x11fb->dpy,0);
  XSetWMProtocols(x11fb->dpy,x11fb->win,&x11fb->atom_WM_DELETE_WINDOW,1);
  
  // Hide cursor.
  x11fb->cursor_visible=1; // x11's default...
  x11fb_show_cursor(x11fb,0); // our default
  
  return 0;
}

/* New.
 */

struct x11fb *x11fb_new(
  const struct x11fb_delegate *delegate,
  const struct x11fb_setup *setup
) {
  if ((setup->fbw<1)||(setup->fbh<1)) return 0;
  
  struct x11fb *x11fb=calloc(1,sizeof(struct x11fb));
  if (!x11fb) return 0;
  
  if (!(x11fb->dpy=XOpenDisplay(setup->device))) {
    free(x11fb);
    return 0;
  }
  x11fb->delegate=*delegate;
  
  if (x11fb_init(x11fb,setup)<0) {
    x11fb_del(x11fb);
    return 0;
  }
  return x11fb;
}

/* Recalculate output bounds, and reallocate (fblarge) as needed.
 */
 
static int x11fb_require_output(struct x11fb *x11fb) {
  
  int xscale=x11fb->w/x11fb->fb->width;
  int yscale=x11fb->h/x11fb->fb->height;
  int scale=(xscale<yscale)?xscale:yscale;
  if (scale<1) scale=1;
  
  x11fb->dstw=x11fb->fb->width*scale;
  x11fb->dsth=x11fb->fb->height*scale;
  x11fb->dstx=(x11fb->w>>1)-(x11fb->dstw>>1);
  x11fb->dsty=(x11fb->h>>1)-(x11fb->dsth>>1);
  
  if (scale==1) { // use (fb) directly
    if (x11fb->fblarge) {
      XDestroyImage(x11fb->fblarge);
      x11fb->fblarge=0;
    }
  } else {
    if (x11fb->fblarge&&(x11fb->fblarge->width==x11fb->dstw)) {
      // already good
    } else {
      if (x11fb->fblarge) {
        XDestroyImage(x11fb->fblarge);
        x11fb->fblarge=0;
      }
      if (!(x11fb->fblarge=x11fb_new_image(x11fb,x11fb->dstw,x11fb->dsth))) return -1;
    }
  }
  
  return 0;
}

/* Begin frame.
 */

void *x11fb_begin(struct x11fb *x11fb) {
  return x11fb->fb->data;
}

/* End frame.
 */
 
int x11fb_end(struct x11fb *x11fb) {
  x11fb->screensaver_suppressed=0;
  
  if (x11fb->dstdirty) {
    if (x11fb_require_output(x11fb)<0) return -1;
    x11fb->dstdirty=0;
  }
  
  if (x11fb->fblarge) {
    x11fb_scale(x11fb->fblarge,x11fb,x11fb->fb);
  }
  
  int c;
  /**/
  if (x11fb->dstx>0) XFillRectangle(x11fb->dpy,x11fb->win,x11fb->gc,0,0,x11fb->dstx,x11fb->h);
  if (x11fb->dsty>0) XFillRectangle(x11fb->dpy,x11fb->win,x11fb->gc,0,0,x11fb->w,x11fb->dsty);
  if ((c=x11fb->w-x11fb->dstw-x11fb->dstx)>0) XFillRectangle(x11fb->dpy,x11fb->win,x11fb->gc,x11fb->w-c,0,c,x11fb->h);
  if ((c=x11fb->h-x11fb->dsth-x11fb->dsty)>0) XFillRectangle(x11fb->dpy,x11fb->win,x11fb->gc,0,x11fb->h-c,x11fb->w,c);
  /**/
  
  XImage *src=x11fb->fblarge?x11fb->fblarge:x11fb->fb;
  XPutImage(x11fb->dpy,x11fb->win,x11fb->gc,src,0,0,x11fb->dstx,x11fb->dsty,src->width,src->height);
  
  return 0;
}

/* Get framebuffer properties.
 */

void x11fb_get_framebuffer_geometry(
  int *w,int *h,int *stride,int *pixelsize,
  const struct x11fb *x11fb
) {
  if (w) *w=x11fb->fb->width;
  if (h) *h=x11fb->fb->height;
  if (stride) *stride=x11fb->fb->width<<2;
  if (pixelsize) *pixelsize=32;
}

void x11fb_get_framebuffer_masks(
  int *r,int *g,int *b,int *a,
  const struct x11fb *x11fb
) {
  if (r) *r=x11fb->fb->red_mask;
  if (g) *g=x11fb->fb->green_mask;
  if (b) *b=x11fb->fb->blue_mask;
  if (a) *a=0;
}

void x11fb_get_framebuffer_chorder(
  char *chorder, // 4 bytes
  const struct x11fb *x11fb
) {
  memset(chorder,0,4);
  uint32_t bodetect=0x04030201;
  if (*(uint8_t*)&bodetect==0x04) { // big-endian
    #define SETCHAN(mask,ch) switch (mask) { \
      case 0xff000000: chorder[0]=ch; break; \
      case 0x00ff0000: chorder[1]=ch; break; \
      case 0x0000ff00: chorder[2]=ch; break; \
      case 0x000000ff: chorder[3]=ch; break; \
    }
    SETCHAN(x11fb->fb->red_mask,'r')
    SETCHAN(x11fb->fb->green_mask,'g')
    SETCHAN(x11fb->fb->blue_mask,'b')
    #undef SETCHAN
  } else {
    #define SETCHAN(mask,ch) switch (mask) { \
      case 0x000000ff: chorder[0]=ch; break; \
      case 0x0000ff00: chorder[1]=ch; break; \
      case 0x00ff0000: chorder[2]=ch; break; \
      case 0xff000000: chorder[3]=ch; break; \
    }
    SETCHAN(x11fb->fb->red_mask,'r')
    SETCHAN(x11fb->fb->green_mask,'g')
    SETCHAN(x11fb->fb->blue_mask,'b')
    #undef SETCHAN
  }
}

/* Show/hide cursor.
 */

void x11fb_show_cursor(struct x11fb *x11fb,int show) {
  if (show) {
    if (x11fb->cursor_visible) return;
    XDefineCursor(x11fb->dpy,x11fb->win,0);
    x11fb->cursor_visible=1;
  } else {
    if (!x11fb->cursor_visible) return;
    XColor color;
    Pixmap pixmap=XCreateBitmapFromData(x11fb->dpy,x11fb->win,"\0\0\0\0\0\0\0\0",1,1);
    Cursor cursor=XCreatePixmapCursor(x11fb->dpy,pixmap,pixmap,&color,&color,0,0);
    XDefineCursor(x11fb->dpy,x11fb->win,cursor);
    XFreeCursor(x11fb->dpy,cursor);
    XFreePixmap(x11fb->dpy,pixmap);
    x11fb->cursor_visible=0;
  }
}

/* Fullscreen.
 */
 
static void x11fb_change_fullscreen(struct x11fb *x11fb,int fullscreen) {
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=x11fb->atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=x11fb->win,
      .data={.l={
        fullscreen,
        x11fb->atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(x11fb->dpy,RootWindow(x11fb->dpy,x11fb->screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(x11fb->dpy);
}
 
void x11fb_set_fullscreen(struct x11fb *x11fb,int fullscreen) {
  if (fullscreen) {
    if (x11fb->fullscreen) return;
    x11fb_change_fullscreen(x11fb,1);
    x11fb->fullscreen=1;
  } else {
    if (!x11fb->fullscreen) return;
    x11fb_change_fullscreen(x11fb,0);
    x11fb->fullscreen=0;
  }
}

/* Suppress screensaver.
 */
 
void x11fb_suppress_screensaver(struct x11fb *x11fb) {
  if (x11fb->screensaver_suppressed) return;
  x11fb->screensaver_suppressed=1;
  XForceScreenSaver(x11fb->dpy,ScreenSaverReset);
}

/* Trivial accessors.
 */
 
void x11fb_get_size(int *w,int *h,const struct x11fb *x11fb) {
  *w=x11fb->w;
  *h=x11fb->h;
}

int x11fb_get_fullscreen(const struct x11fb *x11fb) {
  return x11fb->fullscreen;
}
