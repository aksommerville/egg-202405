#include "egg_native_internal.h"

// Drop HTTP responses if we update so many times and they're still not captured.
// Note that this is not a request timeout: It only starts counting after the response or error arrives.
#define EGG_HTTP_RESPONSE_TTL 30
#define EGG_WS_MESSAGE_TTL 30

/* Cleanup.
 */
 
static void egg_net_msg_cleanup(struct egg_net_msg *msg) {
  if (msg->v) free(msg->v);
}
 
static void egg_net_op_cleanup(struct egg_net_op *op) {
  if (op->v) free(op->v);
  if (op->msgv) {
    while (op->msgc-->0) egg_net_msg_cleanup(op->msgv+op->msgc);
    free(op->msgv);
  }
}
 
void egg_native_net_cleanup() {
  if (egg.net_opv) {
    while (egg.net_opc-->0) egg_net_op_cleanup(egg.net_opv+egg.net_opc);
    free(egg.net_opv);
  }
}

/* Op list.
 */
 
static int egg_net_op_require() {
  if (egg.net_opc<egg.net_opa) return 0;
  int na=egg.net_opa+8;
  if (na>INT_MAX/sizeof(struct egg_net_op)) return -1;
  void *nv=realloc(egg.net_opv,sizeof(struct egg_net_op)*na);
  if (!nv) return -1;
  egg.net_opv=nv;
  egg.net_opa=na;
  return 0;
}

static void egg_net_op_remove(struct egg_net_op *op) {
  if (!op) return;
  int p=op-egg.net_opv;
  if ((p<0)||(p>=egg.net_opc)) return;
  egg_net_op_cleanup(op);
  egg.net_opc--;
  memmove(op,op+1,sizeof(struct egg_net_op)*(egg.net_opc-p));
}

static struct egg_net_op *egg_net_op_by_ws(const struct http_websocket *ws) {
  if (!ws) return 0;
  struct egg_net_op *op=egg.net_opv;
  int i=egg.net_opc;
  for (;i-->0;op++) {
    if (op->ws==ws) return op;
  }
  return 0;
}

static struct egg_net_op *egg_net_op_by_wsid(int wsid) {
  struct egg_net_op *op=egg.net_opv;
  int i=egg.net_opc;
  for (;i-->0;op++) {
    if (!op->ws) continue;
    if (op->id!=wsid) continue;
    return op;
  }
  return 0;
}

static struct egg_net_op *egg_net_op_by_req(const struct http_xfer *req) {
  if (!req) return 0;
  struct egg_net_op *op=egg.net_opv;
  int i=egg.net_opc;
  for (;i-->0;op++) {
    if (op->req==req) return op;
  }
  return 0;
}

static struct egg_net_op *egg_net_op_by_reqid(int reqid) {
  struct egg_net_op *op=egg.net_opv;
  int i=egg.net_opc;
  for (;i-->0;op++) {
    if (!op->req) continue;
    if (op->id!=reqid) continue;
    return op;
  }
  return 0;
}

static int egg_net_op_add_msg(struct egg_net_op *op,int opcode,const void *v,int c) {
  if (op->msgc>=op->msga) {
    int na=op->msga+8;
    if (na>INT_MAX/sizeof(struct egg_net_msg)) return -1;
    void *nv=realloc(op->msgv,sizeof(struct egg_net_msg)*na);
    if (!nv) return -1;
    op->msgv=nv;
    op->msga=na;
  }
  void *nv=malloc(c?c:1);
  if (!nv) return -1;
  memcpy(nv,v,c);
  struct egg_net_msg *msg=op->msgv+op->msgc++;
  msg->msgid=op->msgid_next++;
  msg->opcode=opcode;
  msg->v=nv;
  msg->c=c;
  msg->ttl=EGG_WS_MESSAGE_TTL;
  return msg->msgid;
}

/* HTTP callbacks.
 */
 
static int egg_native_net_cb_connect(struct http_websocket *ws,void *userdata) {
  struct egg_net_op *op=egg_net_op_by_ws(ws);
  if (!op) return 0;
  struct egg_event *event=egg_native_push_event();
  event->type=EGG_EVENT_WS_CONNECT;
  event->v[0]=op->id;
  event->v[1]=0;
  event->v[2]=0;
  event->v[3]=0;
  op->status=1;
  return 0;
}

static int egg_native_net_cb_disconnect(struct http_websocket *ws,void *userdata) {
  struct egg_net_op *op=egg_net_op_by_ws(ws);
  if (!op) return 0;
  struct egg_event *event=egg_native_push_event();
  event->type=EGG_EVENT_WS_DISCONNECT;
  event->v[0]=op->id;
  event->v[1]=0;
  event->v[2]=0;
  event->v[3]=0;
  egg_net_op_remove(op);
  return 0;
}

static int egg_native_net_cb_message(struct http_websocket *ws,int opcode,const void *v,int c) {
  struct egg_net_op *op=egg_net_op_by_ws(ws);
  if (!op) return 0;
  int msgid=egg_net_op_add_msg(op,opcode,v,c);
  if (msgid<0) return 0;
  struct egg_event *event=egg_native_push_event();
  event->type=EGG_EVENT_WS_MESSAGE;
  event->v[0]=op->id;
  event->v[1]=msgid;
  event->v[2]=opcode;
  event->v[3]=c;
  return 0;
}

static int egg_native_net_cb_rsp(struct http_xfer *req,struct http_xfer *rsp) {
  struct egg_net_op *op=egg_net_op_by_req(req);
  if (!op) return 0;
  
  if (!(op->status=http_xfer_get_status(rsp))) op->status=-1;
  const struct sr_encoder *body=http_xfer_get_body(rsp);
  if (body->c) {
    if (op->v) free(op->v);
    if (!(op->v=malloc(body->c))) return -1;
    memcpy(op->v,body->v,body->c);
    op->c=body->c;
  }
  
  op->ttl=EGG_HTTP_RESPONSE_TTL;
  struct egg_event *event=egg_native_push_event();
  event->type=EGG_EVENT_HTTP_RSP;
  event->v[0]=op->id;
  event->v[1]=op->status;
  event->v[2]=http_xfer_get_body(rsp)->c;
  event->v[3]=0;
  return 0;
}

/* Init.
 * Beware, initialization will proceed even if we fail.
 */
 
int egg_native_net_init() {
  egg.net_id_next=1;
  struct http_context_delegate delegate={
    .cb_connect=egg_native_net_cb_connect,
    .cb_disconnect=egg_native_net_cb_disconnect,
  };
  if (!(egg.http=http_context_new(&delegate))) return -1;
  return 0;
}

/* Update.
 */
 
int egg_native_net_update() {
  if (egg.http&&(http_update(egg.http,0)<0)) return -1;
  
  // Enforce timeouts, so responses don't linger here when the client declines to retrieve them.
  // Client! Why did you ask for it, then?
  int opi=egg.net_opc;
  struct egg_net_op *op=egg.net_opv+opi-1;
  for (;opi-->0;op--) {
    if (op->ttl==1) {
      fprintf(stderr,"%s:WARNING: Dropping HTTP response, client did not retrieve it.\n",egg.exename);
      egg_net_op_cleanup(op);
      egg.net_opc--;
      memmove(op,op+1,sizeof(struct egg_net_op)*(egg.net_opc-opi));
    } else {
      if (op->ttl>0) op->ttl--;
      int msgi=op->msgc;
      struct egg_net_msg *msg=op->msgv+msgi-1;
      for (;msgi-->0;msg--) {
        if (msg->ttl==1) {
          fprintf(stderr,"%s:WARNING: Dropping %d-byte WebSocket packet, client did not retrieve it.\n",egg.exename,msg->c);
          egg_net_msg_cleanup(msg);
          op->msgc--;
          memmove(msg,msg+1,sizeof(struct egg_net_msg)*(op->msgc-msgi));
        } else {
          if (msg->ttl>0) msg->ttl--;
        }
      }
    }
  }
  return 0;
}

/* Public API: HTTP requests.
 */
 
int egg_http_request(const char *method,const char *url,const void *body,int bodyc) {
  if (!method||!method[0]) method="GET";
  if (!url||!url[0]) return -1;
  if ((bodyc<0)||(bodyc&&!body)) return -1;
  if (!egg.http) return -1;
  if (egg.net_id_next<1) return -1;
  //TODO Filter connections per user.
  if (egg_net_op_require()<0) return -1;
  struct egg_net_op *op=egg.net_opv+egg.net_opc++;
  memset(op,0,sizeof(struct egg_net_op));
  if (!(op->req=http_request(egg.http,method,url,-1,egg_native_net_cb_rsp,0))) {
    egg.net_opc--;
    return -1;
  }
  if (bodyc>0) {
    sr_encode_raw(http_xfer_get_body(op->req),body,bodyc);
  }
  op->id=egg.net_id_next++;
  return op->id;
}

int egg_http_get_status(int reqid) {
  struct egg_net_op *op=egg_net_op_by_reqid(reqid);
  if (!op) return -1;
  return op->status;
}

int egg_http_get_header(char *dst,int dsta,int reqid,const char *k,int kc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  fprintf(stderr,"%s reqid=%d k='%.*s'\n",__func__,reqid,kc,k);//TODO
  struct egg_net_op *op=egg_net_op_by_reqid(reqid);
  if (!op) return 0;
  //TODO -- http_xfer are not retainable. we'd have to copy the headers out, or change http unit to make xfer retainable. not crazy about either.
  if (dsta>0) dst[0]=0;
  return 0;
}

int egg_http_get_body(void *dst,int dsta,int reqid) {
  struct egg_net_op *op=egg_net_op_by_reqid(reqid);
  if (!op) return -1;
  if (op->c<=dsta) memcpy(dst,op->v,op->c);
  int c=op->c;
  egg_net_op_remove(op);
  return c;
}

/* Public API: WebSocket.
 */
 
int egg_ws_connect(const char *url) {
  if (!url||!url[0]) return -1;
  if (!egg.http) return -1;
  if (egg.net_id_next<1) return -1;
  //TODO Filter connections per user.
  if (egg_net_op_require()<0) return -1;
  struct egg_net_op *op=egg.net_opv+egg.net_opc++;
  memset(op,0,sizeof(struct egg_net_op));
  if (!(op->ws=http_websocket_connect(egg.http,url,-1,egg_native_net_cb_message,0))) {
    egg.net_opc--;
    return -1;
  }
  op->id=egg.net_id_next++;
  op->msgid_next=1;
  return op->id;
}

void egg_ws_disconnect(int wsid) {
  struct egg_net_op *op=egg_net_op_by_wsid(wsid);
  if (!op) return;
  http_websocket_disconnect(op->ws);
  egg_net_op_remove(op);
}

int egg_ws_get_message(void *dst,int dsta,int wsid,int msgid) {
  struct egg_net_op *op=egg_net_op_by_wsid(wsid);
  if (!op) return 0;
  struct egg_net_msg *msg=op->msgv;
  int i=0;
  for (;i<op->msgc;i++,msg++) {
    if (msg->msgid!=msgid) continue;
    if (msg->c>dsta) return msg->c; // do not remove message
    memcpy(dst,msg->v,msg->c);
    int c=msg->c;
    egg_net_msg_cleanup(msg);
    op->msgc--;
    memmove(msg,msg+1,sizeof(struct egg_net_msg)*(op->msgc-i));
    return c;
  }
  return 0;
}

void egg_ws_send(int wsid,int opcode,const void *v,int c) {
  struct egg_net_op *op=egg_net_op_by_wsid(wsid);
  if (!op) return;
  http_websocket_send(op->ws,opcode,v,c);
}
