#include "egg_native_internal.h"

/* Event queue.
 */
 
struct egg_event *egg_native_push_event() {
  if (egg.eventc>=EGG_EVENT_QUEUE_LENGTH) {
    fprintf(stderr,"*** WARNING *** Event queue overflow, dropping oldest event.\n");
    egg.eventp++;
    egg.eventc=EGG_EVENT_QUEUE_LENGTH-1;
  }
  int np=(egg.eventp+egg.eventc)%EGG_EVENT_QUEUE_LENGTH;
  egg.eventc++;
  struct egg_event *event=egg.eventq+np;
  memset(event,0,sizeof(struct egg_event));
  return event;
}

/* Public API: Pull from event queue.
 */
 
int egg_event_next(struct egg_event *eventv,int eventa) {
  if (eventa>egg.eventc) eventa=egg.eventc;
  if (eventa<1) return 0;
  int cpc=EGG_EVENT_QUEUE_LENGTH-egg.eventp;
  if (cpc>eventa) cpc=eventa;
  memcpy(eventv,egg.eventq+egg.eventp,sizeof(struct egg_event)*cpc);
  if (!(egg.eventc-=cpc)) {
    egg.eventp=0;
    return cpc;
  }
  if ((egg.eventp+=cpc)>=EGG_EVENT_QUEUE_LENGTH) egg.eventp=0;
  eventv+=cpc;
  cpc=eventa-cpc;
  memcpy(eventv,egg.eventq+egg.eventp,sizeof(struct egg_event)*cpc);
  if (!(egg.eventc-=cpc)) {
    egg.eventp=0;
    return cpc;
  }
  if ((egg.eventp+=cpc)>=EGG_EVENT_QUEUE_LENGTH) egg.eventp=0;
  return eventa;
}

/* Return 0, EGG_EVTSTATE_IMPOSSIBLE, or EGG_EVTSTATE_REQUIRED,
 * to identify events that can't be changed by the client.
 */
 
static int egg_event_get_hard_state(int evttype) {
  switch (evttype) {

    /* Network message-received events are required; failure to react to them is a serious warning.
     * Resize too, but that's debatable.
     */
    case EGG_EVENT_HTTP_RSP: return EGG_EVTSTATE_REQUIRED;
    case EGG_EVENT_WS_MESSAGE: return EGG_EVTSTATE_REQUIRED;
    case EGG_EVENT_RESIZE: return EGG_EVTSTATE_REQUIRED;
    
    /* Mouse and keyboard events are available if the video driver provides input.
     * Otherwise IMPOSSIBLE.
     * Providing a fake emulated mouse or keyboard is a great idea, but should be done on the client side.
     */
    case EGG_EVENT_MMOTION:
    case EGG_EVENT_MBUTTON:
    case EGG_EVENT_MWHEEL:
    case EGG_EVENT_KEY:
    case EGG_EVENT_TEXT: {
        if (!egg.hostio->video||!egg.hostio->video->type->provides_input) {
          return EGG_EVTSTATE_IMPOSSIBLE;
        }
        return 0;
      }
  }
  return 0;
}

/* Public API: Event mask.
 */
 
int egg_event_enable(int evttype,int evtstate) {
  
  /* Not changeable, or only querying state?
   */
  if ((evttype<1)||(evttype>31)) return EGG_EVTSTATE_IMPOSSIBLE;
  int hardstate=egg_event_get_hard_state(evttype);
  if (hardstate) return hardstate;
  if (evtstate==EGG_EVTSTATE_QUERY) return (egg.eventmask&(1<<evttype))?EGG_EVTSTATE_ENABLED:EGG_EVTSTATE_DISABLED;
  
  int mask=1<<evttype;
  int nstate=((evtstate==EGG_EVTSTATE_ENABLED)||(evtstate==EGG_EVTSTATE_REQUIRED))?mask:0;
  #define ACCEPT if (nstate) egg.eventmask|=mask; else egg.eventmask&=~mask;
  #define RETURN return (egg.eventmask&mask)?EGG_EVTSTATE_ENABLED:EGG_EVTSTATE_DISABLED;
  switch (evttype) {

    // TODO Should we call Input Bus events IMPOSSIBLE when hostio has no input driver?
    case EGG_EVENT_INPUT: ACCEPT RETURN
    case EGG_EVENT_CONNECT: ACCEPT RETURN
    case EGG_EVENT_DISCONNECT: ACCEPT RETURN
    
    // Network. Some of these are not maskable, but whatever, that would be handled already.
    case EGG_EVENT_HTTP_RSP: ACCEPT RETURN
    case EGG_EVENT_WS_CONNECT: ACCEPT RETURN
    case EGG_EVENT_WS_DISCONNECT: ACCEPT RETURN
    case EGG_EVENT_WS_MESSAGE: ACCEPT RETURN
    
    // Mouse events control the system cursor's visibility.
    // We've already confirmed that there is a video driver and it provides input.
    case EGG_EVENT_MMOTION:
    case EGG_EVENT_MBUTTON:
    case EGG_EVENT_MWHEEL: {
        ACCEPT
        if (egg.hostio->video->type->show_cursor) {
          egg.hostio->video->type->show_cursor(egg.hostio->video,(egg.eventmask&(
            (1<<EGG_EVENT_MMOTION)|
            (1<<EGG_EVENT_MBUTTON)|
            (1<<EGG_EVENT_MWHEEL)
          ))?1:0);
        }
        RETURN
      }
    
    // Keyboard: The IMPOSSIBLE case has already been handled.
    case EGG_EVENT_KEY: ACCEPT RETURN
    case EGG_EVENT_TEXT: ACCEPT RETURN
    
    case EGG_EVENT_RESIZE: return EGG_EVTSTATE_REQUIRED;
  }
  #undef ACCEPT
  #undef RETURN
  return EGG_EVTSTATE_IMPOSSIBLE;
}

/* Init.
 */
 
int egg_native_event_init() {
  egg.eventmask=
    (1<<EGG_EVENT_INPUT)|
    (1<<EGG_EVENT_CONNECT)|
    (1<<EGG_EVENT_DISCONNECT)|
    (1<<EGG_EVENT_HTTP_RSP)|
    (1<<EGG_EVENT_WS_CONNECT)|
    (1<<EGG_EVENT_WS_DISCONNECT)|
    (1<<EGG_EVENT_WS_MESSAGE)|
    // MMOTION, MBUTTON, MWHEEL, TEXT: off by default
    (1<<EGG_EVENT_KEY)|
    (1<<EGG_EVENT_RESIZE)|
  0;
  return 0;
}

/* Window.
 */
 
void egg_native_cb_close(struct hostio_video *driver) {
  egg.terminate=1;
}

void egg_native_cb_focus(struct hostio_video *driver,int focus) {
  //TODO Should we pause the game when blurred? Steam yelled at me once for not doing that, but I think it would be kind of rude of us.
}

void egg_native_cb_resize(struct hostio_video *driver,int w,int h) {
  struct egg_event *event=egg_native_push_event();
  event->type=EGG_EVENT_RESIZE;
  event->v[0]=w;
  event->v[1]=h;
}

/* Keyboard.
 */
 
int egg_native_cb_key(struct hostio_video *driver,int keycode,int value) {
  
  //TODO Handle certain keys here, and don't send to the game. Should we? I'm thinking eg Esc to quit.
  
  if (egg.eventmask&(1<<EGG_EVENT_KEY)) {
    struct egg_event *event=egg_native_push_event();
    event->type=EGG_EVENT_KEY;
    event->v[0]=keycode;
    event->v[1]=value;
  }
  if (egg.eventmask&(1<<EGG_EVENT_TEXT)) {
    return 0; // 0=Please translate if possible.
  } else {
    return 1; // 1=Don't bother looking up codepoint.
  }
}

void egg_native_cb_text(struct hostio_video *driver,int codepoint) {
  if (egg.eventmask&(1<<EGG_EVENT_TEXT)) {
    struct egg_event *event=egg_native_push_event();
    event->type=EGG_EVENT_TEXT;
    event->v[0]=codepoint;
  }
}

/* Mouse.
 */
 
void egg_native_cb_mmotion(struct hostio_video *driver,int x,int y) {
  egg.mousex=x;
  egg.mousey=y;
  if (egg.eventmask&(1<<EGG_EVENT_MMOTION)) {
    struct egg_event *event=egg_native_push_event();
    event->type=EGG_EVENT_MMOTION;
    event->v[0]=x;
    event->v[1]=y;
  }
}

void egg_native_cb_mbutton(struct hostio_video *driver,int btnid,int value) {
  if (egg.eventmask&(1<<EGG_EVENT_MBUTTON)) {
    struct egg_event *event=egg_native_push_event();
    event->type=EGG_EVENT_MBUTTON;
    event->v[0]=btnid;
    event->v[1]=value;
    event->v[2]=egg.mousex;
    event->v[3]=egg.mousey;
  }
}

void egg_native_cb_mwheel(struct hostio_video *driver,int dx,int dy) {
  if (egg.eventmask&(1<<EGG_EVENT_MWHEEL)) {
    struct egg_event *event=egg_native_push_event();
    event->type=EGG_EVENT_MWHEEL;
    event->v[0]=dx;
    event->v[1]=dy;
    event->v[2]=egg.mousex;
    event->v[3]=egg.mousey;
  }
}

/* Audio.
 */
 
void egg_native_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver) {
  memset(v,0,c<<1);//TODO synthesizer
}

/* Joystick.
 */
 
void egg_native_cb_connect(struct hostio_input *driver,int devid) {
  egg_native_input_add_device(driver,devid);
  if (egg.eventmask&(1<<EGG_EVENT_CONNECT)) {
    struct egg_event *event=egg_native_push_event();
    event->type=EGG_EVENT_CONNECT;
    event->v[0]=devid;
  }
}

void egg_native_cb_disconnect(struct hostio_input *driver,int devid) {
  egg_native_input_remove_device(devid);
  if (egg.eventmask&(1<<EGG_EVENT_DISCONNECT)) {
    struct egg_event *event=egg_native_push_event();
    event->type=EGG_EVENT_DISCONNECT;
    event->v[0]=devid;
  }
}

void egg_native_cb_button(struct hostio_input *driver,int devid,int btnid,int value) {
  if (egg.eventmask&(1<<EGG_EVENT_INPUT)) {
    struct egg_event *event=egg_native_push_event();
    event->type=EGG_EVENT_INPUT;
    event->v[0]=devid;
    event->v[1]=btnid;
    event->v[2]=value;
  }
}
