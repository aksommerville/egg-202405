#include "glx_internal.h"

/* Key press, release, or repeat.
 */
 
static int glx_evt_key(struct glx *glx,XKeyEvent *evt,int value) {

  /* Pass the raw keystroke. */
  if (glx->delegate.cb_key) {
    KeySym keysym=XkbKeycodeToKeysym(glx->dpy,evt->keycode,0,0);
    if (keysym) {
      int keycode=glx_usb_usage_from_keysym((int)keysym);
      if (keycode) {
        int err=glx->delegate.cb_key(glx->delegate.userdata,keycode,value);
        if (err) return err; // Stop here if acknowledged.
      }
    }
  }
  
  /* Pass text if press or repeat, and text can be acquired. */
  if (glx->delegate.cb_text) {
    int shift=(evt->state&ShiftMask)?1:0;
    KeySym tkeysym=XkbKeycodeToKeysym(glx->dpy,evt->keycode,0,shift);
    if (shift&&!tkeysym) { // If pressing shift makes this key "not a key anymore", fuck that and pretend shift is off
      tkeysym=XkbKeycodeToKeysym(glx->dpy,evt->keycode,0,0);
    }
    if (tkeysym) {
      int codepoint=glx_codepoint_from_keysym(tkeysym);
      if (codepoint && (evt->type == KeyPress || evt->type == KeyRepeat)) {
        glx->delegate.cb_text(glx->delegate.userdata,codepoint);
      }
    }
  }
  
  return 0;
}

/* Mouse events.
 */
 
static int glx_evt_mbtn(struct glx *glx,XButtonEvent *evt,int value) {
  
  // I swear X11 used to automatically report the wheel as (6,7) while shift held, and (4,5) otherwise.
  // After switching to GNOME 3, seems it is only ever (4,5).
  #define SHIFTABLE(v) (evt->state&ShiftMask)?v:0,(evt->state&ShiftMask)?0:v
  
  switch (evt->button) {
    case 1: if (glx->delegate.cb_mbutton) glx->delegate.cb_mbutton(glx->delegate.userdata,1,value); break;
    case 2: if (glx->delegate.cb_mbutton) glx->delegate.cb_mbutton(glx->delegate.userdata,3,value); break;
    case 3: if (glx->delegate.cb_mbutton) glx->delegate.cb_mbutton(glx->delegate.userdata,2,value); break;
    case 4: if (value&&glx->delegate.cb_mwheel) glx->delegate.cb_mwheel(glx->delegate.userdata,SHIFTABLE(-1)); break;
    case 5: if (value&&glx->delegate.cb_mwheel) glx->delegate.cb_mwheel(glx->delegate.userdata,SHIFTABLE(1)); break;
    case 6: if (value&&glx->delegate.cb_mwheel) glx->delegate.cb_mwheel(glx->delegate.userdata,-1,0); break;
    case 7: if (value&&glx->delegate.cb_mwheel) glx->delegate.cb_mwheel(glx->delegate.userdata,1,0); break;
  }
  #undef SHIFTABLE
  return 0;
}

static int glx_evt_mmotion(struct glx *glx,XMotionEvent *evt) {
  if (glx->delegate.cb_mmotion) {
    glx->delegate.cb_mmotion(glx->delegate.userdata,evt->x,evt->y);
  }
  return 0;
}

/* Client message.
 */
 
static int glx_evt_client(struct glx *glx,XClientMessageEvent *evt) {
  if (evt->message_type==glx->atom_WM_PROTOCOLS) {
    if (evt->format==32) {
      if (evt->data.l[0]==glx->atom_WM_DELETE_WINDOW) {
        if (glx->delegate.cb_close) {
          glx->delegate.cb_close(glx->delegate.userdata);
        }
      }
    }
  }
  return 0;
}

/* Configuration event (eg resize).
 */
 
static int glx_evt_configure(struct glx *glx,XConfigureEvent *evt) {
  int nw=evt->width,nh=evt->height;
  if ((nw!=glx->w)||(nh!=glx->h)) {
    glx->w=nw;
    glx->h=nh;
    if (glx->delegate.cb_resize) {
      glx->delegate.cb_resize(glx->delegate.userdata,nw,nh);
    }
  }
  return 0;
}

/* Focus.
 */
 
static int glx_evt_focus(struct glx *glx,XFocusInEvent *evt,int value) {
  if (value==glx->focus) return 0;
  glx->focus=value;
  if (glx->delegate.cb_focus) {
    glx->delegate.cb_focus(glx->delegate.userdata,value);
  }
  return 0;
}

/* Process one event.
 */
 
static int glx_receive_event(struct glx *glx,XEvent *evt) {
  if (!evt) return -1;
  switch (evt->type) {
  
    case KeyPress: return glx_evt_key(glx,&evt->xkey,1);
    case KeyRelease: return glx_evt_key(glx,&evt->xkey,0);
    case KeyRepeat: return glx_evt_key(glx,&evt->xkey,2);
    
    case ButtonPress: return glx_evt_mbtn(glx,&evt->xbutton,1);
    case ButtonRelease: return glx_evt_mbtn(glx,&evt->xbutton,0);
    case MotionNotify: return glx_evt_mmotion(glx,&evt->xmotion);
    
    case ClientMessage: return glx_evt_client(glx,&evt->xclient);
    
    case ConfigureNotify: return glx_evt_configure(glx,&evt->xconfigure);
    
    case FocusIn: return glx_evt_focus(glx,&evt->xfocus,1);
    case FocusOut: return glx_evt_focus(glx,&evt->xfocus,0);
    
  }
  return 0;
}

/* Update.
 */
 
int glx_update(struct glx *glx) {
  int evtc=XEventsQueued(glx->dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(glx->dpy,&evt);
    if ((evtc>0)&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(glx->dpy,&next);
      evtc--;
      if ((next.type==KeyPress)&&(evt.xkey.keycode==next.xkey.keycode)&&(evt.xkey.time>=next.xkey.time-GLX_KEY_REPEAT_INTERVAL)) {
        evt.type=KeyRepeat;
        if (glx_receive_event(glx,&evt)<0) return -1;
      } else {
        if (glx_receive_event(glx,&evt)<0) return -1;
        if (glx_receive_event(glx,&next)<0) return -1;
      }
    } else {
      if (glx_receive_event(glx,&evt)<0) return -1;
    }
  }
  return 0;
}
