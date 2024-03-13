#include "x11fb_internal.h"

/* Key press, release, or repeat.
 */
 
static int x11fb_evt_key(struct x11fb *x11fb,XKeyEvent *evt,int value) {

  /* Pass the raw keystroke. */
  if (x11fb->delegate.cb_key) {
    KeySym keysym=XkbKeycodeToKeysym(x11fb->dpy,evt->keycode,0,0);
    if (keysym) {
      int keycode=x11fb_usb_usage_from_keysym((int)keysym);
      if (keycode) {
        int err=x11fb->delegate.cb_key(x11fb->delegate.userdata,keycode,value);
        if (err) return err; // Stop here if acknowledged.
      }
    }
  }
  
  /* Pass text if press or repeat, and text can be acquired. */
  if (x11fb->delegate.cb_text) {
    int shift=(evt->state&ShiftMask)?1:0;
    KeySym tkeysym=XkbKeycodeToKeysym(x11fb->dpy,evt->keycode,0,shift);
    if (shift&&!tkeysym) { // If pressing shift makes this key "not a key anymore", fuck that and pretend shift is off
      tkeysym=XkbKeycodeToKeysym(x11fb->dpy,evt->keycode,0,0);
    }
    if (tkeysym) {
      int codepoint=x11fb_codepoint_from_keysym(tkeysym);
      if (codepoint && (evt->type == KeyPress || evt->type == KeyRepeat)) {
        x11fb->delegate.cb_text(x11fb->delegate.userdata,codepoint);
      }
    }
  }
  
  return 0;
}

/* Mouse events.
 */
 
static int x11fb_evt_mbtn(struct x11fb *x11fb,XButtonEvent *evt,int value) {
  
  // I swear X11 used to automatically report the wheel as (6,7) while shift held, and (4,5) otherwise.
  // After switching to GNOME 3, seems it is only ever (4,5).
  #define SHIFTABLE(v) (evt->state&ShiftMask)?v:0,(evt->state&ShiftMask)?0:v
  
  switch (evt->button) {
    case 1: if (x11fb->delegate.cb_mbutton) x11fb->delegate.cb_mbutton(x11fb->delegate.userdata,1,value); break;
    case 2: if (x11fb->delegate.cb_mbutton) x11fb->delegate.cb_mbutton(x11fb->delegate.userdata,3,value); break;
    case 3: if (x11fb->delegate.cb_mbutton) x11fb->delegate.cb_mbutton(x11fb->delegate.userdata,2,value); break;
    case 4: if (value&&x11fb->delegate.cb_mwheel) x11fb->delegate.cb_mwheel(x11fb->delegate.userdata,SHIFTABLE(-1)); break;
    case 5: if (value&&x11fb->delegate.cb_mwheel) x11fb->delegate.cb_mwheel(x11fb->delegate.userdata,SHIFTABLE(1)); break;
    case 6: if (value&&x11fb->delegate.cb_mwheel) x11fb->delegate.cb_mwheel(x11fb->delegate.userdata,-1,0); break;
    case 7: if (value&&x11fb->delegate.cb_mwheel) x11fb->delegate.cb_mwheel(x11fb->delegate.userdata,1,0); break;
  }
  #undef SHIFTABLE
  return 0;
}

static int x11fb_evt_mmotion(struct x11fb *x11fb,XMotionEvent *evt) {
  if (x11fb->delegate.cb_mmotion) {
    x11fb->delegate.cb_mmotion(x11fb->delegate.userdata,evt->x,evt->y);
  }
  return 0;
}

/* Client message.
 */
 
static int x11fb_evt_client(struct x11fb *x11fb,XClientMessageEvent *evt) {
  if (evt->message_type==x11fb->atom_WM_PROTOCOLS) {
    if (evt->format==32) {
      if (evt->data.l[0]==x11fb->atom_WM_DELETE_WINDOW) {
        if (x11fb->delegate.cb_close) {
          x11fb->delegate.cb_close(x11fb->delegate.userdata);
        }
      }
    }
  }
  return 0;
}

/* Configuration event (eg resize).
 */
 
static int x11fb_evt_configure(struct x11fb *x11fb,XConfigureEvent *evt) {
  int nw=evt->width,nh=evt->height;
  if ((nw!=x11fb->w)||(nh!=x11fb->h)) {
    x11fb->dstdirty=1;
    x11fb->w=nw;
    x11fb->h=nh;
    if (x11fb->delegate.cb_resize) {
      x11fb->delegate.cb_resize(x11fb->delegate.userdata,nw,nh);
    }
  }
  return 0;
}

/* Focus.
 */
 
static int x11fb_evt_focus(struct x11fb *x11fb,XFocusInEvent *evt,int value) {
  if (value==x11fb->focus) return 0;
  x11fb->focus=value;
  if (x11fb->delegate.cb_focus) {
    x11fb->delegate.cb_focus(x11fb->delegate.userdata,value);
  }
  return 0;
}

/* Process one event.
 */
 
static int x11fb_receive_event(struct x11fb *x11fb,XEvent *evt) {
  if (!evt) return -1;
  switch (evt->type) {
  
    case KeyPress: return x11fb_evt_key(x11fb,&evt->xkey,1);
    case KeyRelease: return x11fb_evt_key(x11fb,&evt->xkey,0);
    case KeyRepeat: return x11fb_evt_key(x11fb,&evt->xkey,2);
    
    case ButtonPress: return x11fb_evt_mbtn(x11fb,&evt->xbutton,1);
    case ButtonRelease: return x11fb_evt_mbtn(x11fb,&evt->xbutton,0);
    case MotionNotify: return x11fb_evt_mmotion(x11fb,&evt->xmotion);
    
    case ClientMessage: return x11fb_evt_client(x11fb,&evt->xclient);
    
    case ConfigureNotify: return x11fb_evt_configure(x11fb,&evt->xconfigure);
    
    case FocusIn: return x11fb_evt_focus(x11fb,&evt->xfocus,1);
    case FocusOut: return x11fb_evt_focus(x11fb,&evt->xfocus,0);
    
  }
  return 0;
}

/* Update.
 */
 
int x11fb_update(struct x11fb *x11fb) {
  int evtc=XEventsQueued(x11fb->dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(x11fb->dpy,&evt);
    if ((evtc>0)&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(x11fb->dpy,&next);
      evtc--;
      if ((next.type==KeyPress)&&(evt.xkey.keycode==next.xkey.keycode)&&(evt.xkey.time>=next.xkey.time-X11FB_KEY_REPEAT_INTERVAL)) {
        evt.type=KeyRepeat;
        if (x11fb_receive_event(x11fb,&evt)<0) return -1;
      } else {
        if (x11fb_receive_event(x11fb,&evt)<0) return -1;
        if (x11fb_receive_event(x11fb,&next)<0) return -1;
      }
    } else {
      if (x11fb_receive_event(x11fb,&evt)<0) return -1;
    }
  }
  return 0;
}
