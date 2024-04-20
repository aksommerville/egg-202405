#include "egg_native_internal.h"

/* Cleanup.
 */
 
void egg_native_net_cleanup() {
  #if USE_curlwrap
    curlwrap_del(egg.curlwrap);
    egg.curlwrap=0;
  #endif
}

/* curlwrap callbacks.
 * These callbacks fit our events neatly. Not a coincidence; I wrote curlwrap specifically for Egg.
 */
#if USE_curlwrap

static void egg_net_cb_http_rsp(struct curlwrap *cw,int reqid,int status,int length) {
  struct egg_event *event=egg_native_push_event();
  event->type=EGG_EVENT_HTTP_RSP;
  event->v[0]=reqid;
  event->v[1]=status;
  event->v[2]=length;
}

static void egg_net_cb_ws_connect(struct curlwrap *cw,int wsid) {
  struct egg_event *event=egg_native_push_event();
  event->type=EGG_EVENT_WS_CONNECT;
  event->v[0]=wsid;
}

static void egg_net_cb_ws_disconnect(struct curlwrap *cw,int wsid) {
  struct egg_event *event=egg_native_push_event();
  event->type=EGG_EVENT_WS_DISCONNECT;
  event->v[0]=wsid;
}

static void egg_net_cb_ws_message(struct curlwrap *cw,int wsid,int msgid,int length) {
  struct egg_event *event=egg_native_push_event();
  event->type=EGG_EVENT_WS_MESSAGE;
  event->v[0]=wsid;
  event->v[1]=msgid;
  event->v[2]=length;
}

#endif

/* Init.
 * Beware, initialization will proceed even if we fail.
 */
 
int egg_native_net_init() {
  #if USE_curlwrap
    struct curlwrap_delegate delegate={
      .cb_http_rsp=egg_net_cb_http_rsp,
      .cb_ws_connect=egg_net_cb_ws_connect,
      .cb_ws_disconnect=egg_net_cb_ws_disconnect,
      .cb_ws_message=egg_net_cb_ws_message,
    };
    if (!(egg.curlwrap=curlwrap_new(&delegate))) return -1;
  #endif
  return 0;
}

/* Update.
 */
 
int egg_native_net_update() {
  #if USE_curlwrap
    if (egg.curlwrap) {
      if (curlwrap_update(egg.curlwrap)<0) return -1;
    }
  #endif
  return 0;
}

/* Public API: HTTP requests.
 */
 
int egg_http_request(const char *method,const char *url,const void *body,int bodyc) {
  if (!method||!method[0]) method="GET";
  if (!url||!url[0]) return -1;
  if ((bodyc<0)||(bodyc&&!body)) return -1;
  //TODO filter
  #if USE_curlwrap
    if (egg.curlwrap) {
      return curlwrap_http_request(egg.curlwrap,method,url,body,bodyc);
    }
  #endif
  return -1;
}

int egg_http_get_status(int reqid) {
  #if USE_curlwrap
    if (egg.curlwrap) return curlwrap_http_get_status(egg.curlwrap,reqid);
  #endif
  return -1;
}

int egg_http_get_header(char *dst,int dsta,int reqid,const char *k,int kc) {
  #if USE_curlwrap
    if (egg.curlwrap) {
      const char *v=0;
      int vc=curlwrap_http_get_header(&v,egg.curlwrap,reqid,k,kc);
      if (vc>=0) {
        if (vc<=dsta) {
          memcpy(dst,v,vc);
          if (vc<dsta) dst[vc]=0;
        }
        return vc;
      }
    }
  #endif
  return 0;
}

int egg_http_get_body(void *dst,int dsta,int reqid) {
  #if USE_curlwrap
    if (egg.curlwrap) {
      const void *v=0;
      int vc=curlwrap_http_get_body(&v,egg.curlwrap,reqid);
      if (vc>=0) {
        if (vc<=dsta) {
          memcpy(dst,v,vc);
          if (vc<dsta) ((char*)dst)[vc]=0;
        }
        return vc;
      }
    }
  #endif
  return 0;
}

/* Public API: WebSocket.
 */
 
int egg_ws_connect(const char *url) {
  #if USE_curlwrap
    if (egg.curlwrap) {
      return curlwrap_ws_connect(egg.curlwrap,url);
    }
  #endif
  return -1;
}

void egg_ws_disconnect(int wsid) {
  #if USE_curlwrap
    if (egg.curlwrap) {
      curlwrap_ws_disconnect(egg.curlwrap,wsid);
      return;
    }
  #endif
}

int egg_ws_get_message(void *dst,int dsta,int wsid,int msgid) {
  #if USE_curlwrap
    if (egg.curlwrap) {
      const void *v=0;
      int vc=curlwrap_ws_get_message(&v,egg.curlwrap,wsid,msgid);
      if (vc>=0) {
        if (vc<=dsta) {
          memcpy(dst,v,vc);
          if (vc<dsta) ((char*)dst)[vc]=0;
        }
        return vc;
      }
    }
  #endif
  return 0;
}

void egg_ws_send(int wsid,int opcode,const void *v,int c) {
  #if USE_curlwrap
    if (egg.curlwrap) {
      curlwrap_ws_send(egg.curlwrap,wsid,opcode,v,c);
      return;
    }
  #endif
}
