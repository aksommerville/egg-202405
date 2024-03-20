#include "egg/egg.h"

#if 1

void egg_client_quit() {
  egg_log("egg_client_quit");
}

int egg_client_init() {
  egg_log("egg_client_init");
  egg_log("This message has %d variadic arguments (%s:%d:%s).",4,__FILE__,__LINE__,__func__);
  
  egg_log("%s:%d",__FILE__,__LINE__);
  return 0;
}

/**/
static void on_key(int keycode,int value) {
  if (value) switch (keycode) {
    case 0x00070029: egg_request_termination(); break; // ESC
    case 0x0007003a: { // F1
        if (egg_event_enable(EGG_EVENT_MMOTION,EGG_EVTSTATE_QUERY)==EGG_EVTSTATE_ENABLED) {
          egg_event_enable(EGG_EVENT_MMOTION,EGG_EVTSTATE_DISABLED);
          egg_event_enable(EGG_EVENT_MBUTTON,EGG_EVTSTATE_DISABLED);
          egg_event_enable(EGG_EVENT_MWHEEL,EGG_EVTSTATE_DISABLED);
        } else {
          egg_event_enable(EGG_EVENT_MMOTION,EGG_EVTSTATE_ENABLED);
          egg_event_enable(EGG_EVENT_MBUTTON,EGG_EVTSTATE_ENABLED);
          egg_event_enable(EGG_EVENT_MWHEEL,EGG_EVTSTATE_ENABLED);
        }
      } break;
    case 0x0007003b: { // F2
        if (egg_event_enable(EGG_EVENT_TEXT,EGG_EVTSTATE_QUERY)==EGG_EVTSTATE_ENABLED) {
          egg_event_enable(EGG_EVENT_TEXT,EGG_EVTSTATE_DISABLED);
        } else {
          egg_event_enable(EGG_EVENT_TEXT,EGG_EVTSTATE_ENABLED);
        }
      } break;
  }
}

static void on_input_connect(int devid) {
  egg_log("CONNECT %d",devid);
  char name[256];
  int namec=egg_input_device_get_name(name,sizeof(name),devid);
  if ((namec<0)||(namec>sizeof(name))) namec=0;
  int vid=0,pid=0,version=0;
  egg_input_device_get_ids(&vid,&pid,&version,devid);
  egg_log("name='%.*s' vid=%04x pid=%04x version=%04x",namec,name,vid,pid,version);
  int btnix=0; for (;;btnix++) {
    int btnid=0,hidusage=0,lo=0,hi=0,value=0;
    egg_input_device_get_button(&btnid,&hidusage,&lo,&hi,&value,devid,btnix);
    if (!btnid) break;
    egg_log("  %d 0x%08x %d..%d =%d",btnid,hidusage,lo,hi,value);
  }
}

void egg_client_update(double elapsed) {
  //egg_log("egg_client_update %f",elapsed);
  //egg_request_termination();
  struct egg_event eventv[32];
  int eventc=egg_event_next(eventv,sizeof(eventv)/sizeof(eventv[0]));
  const struct egg_event *event=eventv;
  for (;eventc-->0;event++) switch (event->type) {
    case EGG_EVENT_INPUT: egg_log("INPUT %d.%d=%d",event->v[0],event->v[1],event->v[2]); break;
    case EGG_EVENT_CONNECT: on_input_connect(event->v[0]); break;
    case EGG_EVENT_DISCONNECT: egg_log("DISCONNECT %d",event->v[0]); break;
    case EGG_EVENT_HTTP_RSP: {
        egg_log("HTTP_RSP reqid=%d status=%d length=%d zero=%d",event->v[0],event->v[1],event->v[2],event->v[3]);
        char body[1024];
        if (event->v[2]<=sizeof(body)) {
          if (egg_http_get_body(body,sizeof(body),event->v[0])==event->v[2]) {
            egg_log(">> %.*s",event->v[2],body);
          }
        }
      } break;
    case EGG_EVENT_WS_CONNECT: egg_log("WS_CONNECT %d",event->v[0]); break;
    case EGG_EVENT_WS_DISCONNECT: egg_log("WS_DISCONNECT %d",event->v[0]); break;
    case EGG_EVENT_WS_MESSAGE: {
        char msg[1024];
        if (event->v[3]<=sizeof(msg)) {
          if (egg_ws_get_message(msg,sizeof(msg),event->v[0],event->v[1])==event->v[3]) {
            egg_log(">> %.*s",event->v[3],msg);
          }
        }
      } break;
    case EGG_EVENT_MMOTION: egg_log("MMOTION %d,%d",event->v[0],event->v[1]); break;
    case EGG_EVENT_MBUTTON: egg_log("MBUTTON %d=%d @%d,%d",event->v[0],event->v[1],event->v[2],event->v[3]); break;
    case EGG_EVENT_MWHEEL: egg_log("MWHEEL +%d,%d @%d,%d",event->v[0],event->v[1],event->v[2],event->v[3]); break;
    case EGG_EVENT_KEY: on_key(event->v[0],event->v[1]); break;
    case EGG_EVENT_TEXT: egg_log("TEXT U+%x",event->v[0]); break;
    case EGG_EVENT_RESIZE: egg_log("RESIZE %d,%d",event->v[0],event->v[1]); break;
    default: egg_log("UNKNOWN EVENT: %d[%d,%d,%d,%d]",event->type,event->v[0],event->v[1],event->v[2],event->v[3]);
  }
}
/**/

void egg_client_render() {
  int screenw=0,screenh=0;
  egg_video_get_size(&screenw,&screenh);
}
#endif
