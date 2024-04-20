#include "curlwrap_internal.h"

/* Delete.
 */
 
static void curlwrap_http_del(struct curlwrap_http *http) {
  if (!http) return;
  if (http->headerv) {
    while (http->headerc-->0) free(http->headerv[http->headerc]);
    free(http->headerv);
  }
  if (http->rspbody) free(http->rspbody);
  if (http->reqbody) free(http->reqbody);
  free(http);
}

static void curlwrap_ws_del(struct curlwrap_ws *ws) {
  if (!ws) return;
  if (ws->msgv) {
    while (ws->msgc-->0) {
      struct curlwrap_ws_msg *msg=ws->msgv+ws->msgc;
      if (msg->v) free(msg->v);
    }
    free(ws->msgv);
  }
  free(ws);
}

void curlwrap_del(struct curlwrap *cw) {
  if (!cw) return;
  if (cw->httpv) {
    while (cw->httpc-->0) curlwrap_http_del(cw->httpv[cw->httpc]);
    free(cw->httpv);
  }
  if (cw->wsv) {
    while (cw->wsc-->0) curlwrap_ws_del(cw->wsv[cw->wsc]);
    free(cw->wsv);
  }
  curl_multi_cleanup(cw->multi);
  curl_global_cleanup();
  free(cw);
}

/* New.
 */

struct curlwrap *curlwrap_new(const struct curlwrap_delegate *delegate) {
  struct curlwrap *cw=calloc(1,sizeof(struct curlwrap));
  if (!cw) return 0;
  if (delegate) cw->delegate=*delegate;
  cw->id_next=1;
  if (curl_global_init(0)) {
    free(cw);
    return 0;
  }
  if (!(cw->multi=curl_multi_init())) {
    curlwrap_del(cw);
    return 0;
  }
  return cw;
}

/* HTTP registry.
 */
 
static struct curlwrap_http *curlwrap_http_by_reqid(const struct curlwrap *cw,int reqid) {
  int lo=0,hi=cw->httpc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    struct curlwrap_http *http=cw->httpv[ck];
         if (reqid<http->reqid) hi=ck;
    else if (reqid>http->reqid) lo=ck+1;
    else return http;
  }
  return 0;
}

static struct curlwrap_http *curlwrap_http_new(struct curlwrap *cw) {
  if (cw->id_next<1) return 0;
  if (cw->httpc>=cw->httpa) {
    int na=cw->httpa+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(cw->httpv,sizeof(void*)*na);
    if (!nv) return 0;
    cw->httpv=nv;
    cw->httpa=na;
  }
  struct curlwrap_http *http=calloc(1,sizeof(struct curlwrap_http));
  if (!http) return 0;
  cw->httpv[cw->httpc++]=http;
  http->reqid=cw->id_next++;
  http->magic=CURLWRAP_MAGIC_HTTP;
  return http;
}

static void curlwrap_http_remove(struct curlwrap *cw,struct curlwrap_http *http) {
  int i=cw->httpc;
  while (i-->0) {
    if (cw->httpv[i]!=http) continue;
    cw->httpc--;
    memmove(cw->httpv+i,cw->httpv+i+1,sizeof(void*)*(cw->httpc-i));
    curlwrap_http_del(http);
    return;
  }
}

/* WebSocket registry.
 */
 
static struct curlwrap_ws *curlwrap_ws_by_wsid(const struct curlwrap *cw,int wsid) {
  int lo=0,hi=cw->wsc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    struct curlwrap_ws *ws=cw->wsv[ck];
         if (wsid<ws->wsid) hi=ck;
    else if (wsid>ws->wsid) lo=ck+1;
    else return ws;
  }
  return 0;
}

static struct curlwrap_ws *curlwrap_ws_new(struct curlwrap *cw) {
  if (cw->id_next<1) return 0;
  if (cw->wsc>=cw->wsa) {
    int na=cw->wsa+8;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(cw->wsv,sizeof(void*)*na);
    if (!nv) return 0;
    cw->wsv=nv;
    cw->wsa=na;
  }
  struct curlwrap_ws *ws=calloc(1,sizeof(struct curlwrap_ws));
  if (!ws) return 0;
  cw->wsv[cw->wsc++]=ws;
  ws->wsid=cw->id_next++;
  ws->magic=CURLWRAP_MAGIC_WS;
  ws->parent=cw;
  return ws;
}

static void curlwrap_ws_remove(struct curlwrap *cw,struct curlwrap_ws *ws) {
  int i=cw->wsc;
  while (i-->0) {
    if (cw->wsv[i]!=ws) continue;
    cw->wsc--;
    memmove(cw->wsv+i,cw->wsv+i+1,sizeof(void*)*(cw->wsc-i));
    curlwrap_ws_del(ws);
    return;
  }
}

/* Trivial accessors.
 */

void *curlwrap_get_userdata(const struct curlwrap *cw) {
  if (!cw) return 0;
  return cw->delegate.userdata;
}

int curlwrap_http_get_status(const struct curlwrap *cw,int reqid) {
  struct curlwrap_http *http=curlwrap_http_by_reqid(cw,reqid);
  if (!http) return -1;
  return http->status;
}

static int curlwrap_hdrmatch(const char *hdr,const char *k,int kc) {
  for (;kc-->0;k++,hdr++) {
    char a=*hdr; if ((a>='A')&&(a<='Z')) a+=0x20;
    char b=*k; if ((b>='A')&&(b<='Z')) b+=0x20;
    if (a!=b) return 0;
  }
  if (*hdr!=':') return 0;
  return 1;
}

int curlwrap_http_get_header(void *dstpp,const struct curlwrap *cw,int reqid,const char *k,int kc) {
  struct curlwrap_http *http=curlwrap_http_by_reqid(cw,reqid);
  if (!http) return 0;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  char **hdr=http->headerv;
  int i=http->headerc;
  for (;i-->0;hdr++) {
    if (!curlwrap_hdrmatch(*hdr,k,kc)) continue;
    const char *v=(*hdr)+kc+1;
    while (v&&((unsigned char)(*v)<=0x20)) v++;
    int vc=0; while (v[vc]) vc++;
    if (dstpp) *(const void**)dstpp=v;
    return vc;
  }
  return 0;
}

int curlwrap_http_get_body(void *dstpp,const struct curlwrap *cw,int reqid) {
  struct curlwrap_http *http=curlwrap_http_by_reqid(cw,reqid);
  if (!http) return 0;
  if (dstpp) *(const void**)dstpp=http->rspbody;
  return http->rspbodyc;
}

int curlwrap_http_for_each(
  const struct curlwrap *cw,
  int (*cb)(int reqid,void *userdata),
  void *userdata
) {
  int i=cw->httpc;
  while (i-->0) {
    int err=cb(cw->httpv[i]->reqid,userdata);
    if (err) return err;
  }
  return 0;
}

int curlwrap_ws_get_message(void *dstpp,const struct curlwrap *cw,int wsid,int msgid) {
  struct curlwrap_ws *ws=curlwrap_ws_by_wsid(cw,wsid);
  if (!ws) return 0;
  struct curlwrap_ws_msg *msg=ws->msgv;
  int i=ws->msgc;
  for (;i-->0;msg++) {
    if (msg->msgid!=msgid) continue;
    if (dstpp) *(const void**)dstpp=msg->v;
    return msg->c;
  }
  return 0;
}

int curlwrap_ws_for_each(
  const struct curlwrap *cw,
  int (*cb)(int wsid,void *userdata),
  void *userdata
) {
  int i=cw->wsc;
  while (i-->0) {
    int err=cb(cw->wsv[i]->wsid,userdata);
    if (err) return err;
  }
  return 0;
}

/* Update.
 */

int curlwrap_update(struct curlwrap *cw) {

  // Drop anything defunct.
  int i=cw->httpc;
  while (i-->0) {
    struct curlwrap_http *http=cw->httpv[i];
    if (!http->removable) continue;
    curl_multi_remove_handle(cw->multi,http->removable);
    curlwrap_http_remove(cw,http);
  }
  for (i=cw->wsc;i-->0;) {
    struct curlwrap_ws *ws=cw->wsv[i];
    if (ws->removable) {
      curl_multi_remove_handle(cw->multi,ws->removable);
      curlwrap_ws_remove(cw,ws);
      continue;
    }
    if (ws->msgc) {
      // Drop all messages; they've already had their chance to fetch.
      int mi=ws->msgc;
      struct curlwrap_ws_msg *msg=ws->msgv;
      for (;mi-->0;msg++) {
        if (msg->v) free(msg->v);
      }
      ws->msgc=0;
    }
    if (ws->almost&&!ws->status) {
      // We got end-of-headers in the last update. Now it should be safe to report the connection.
      ws->status=1;
      if (cw->delegate.cb_ws_connect) {
        cw->delegate.cb_ws_connect(cw,ws->wsid);
      }
    }
  }

  int running_handles=0;
  CURLMcode perr=curl_multi_perform(cw->multi,&running_handles);
  if (perr) return -1;
    
  CURLMsg *msg;
  int msgs_left=0;
  while (msg=curl_multi_info_read(cw->multi,&msgs_left)) {
    if (msg->msg==CURLMSG_DONE) {
      CURL *easy=msg->easy_handle;
      struct curlwrap_http *http=0;
      curl_easy_getinfo(easy,CURLINFO_PRIVATE,&http);
      
      if (http&&(http->magic==CURLWRAP_MAGIC_HTTP)) {
        http->removable=easy;
        if (!msg->data.result) {
          long status=0;
          curl_easy_getinfo(easy,CURLINFO_RESPONSE_CODE,&status);
          http->status=status;
        }
        if (!http->status) http->status=-1;
        if (cw->delegate.cb_http_rsp) {
          cw->delegate.cb_http_rsp(cw,http->reqid,http->status,http->rspbodyc);
        }
        
      } else if (http&&(http->magic==CURLWRAP_MAGIC_WS)) {
        // Oops it's actually a websocket.
        struct curlwrap_ws *ws=(struct curlwrap_ws*)http;
        ws->removable=easy;
        if (cw->delegate.cb_ws_disconnect) {
          cw->delegate.cb_ws_disconnect(cw,ws->wsid);
        }
      
      } else {
        curl_multi_remove_handle(cw->multi,easy);
        curl_easy_cleanup(easy);
      }
    }
  }
    
  return 0;
}

/* HTTP request.
 */
 
static size_t curlwrap_cb_http_rspheader(char *v,size_t size,size_t c,void *userdata) {
  if ((size==1)&&(c>0)&&(c<INT_MAX)) {
    struct curlwrap_http *http=userdata;
    const char *src=v;
    int srcc=c;
    while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
    while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc++; }
    int i=srcc,ok=0;
    while (i-->0) if (src[i]==':') { ok=1; break; }
    
    if (ok) { // It's a header.
      if (http->headerc>=http->headera) {
        int na=http->headera+16;
        if (na>INT_MAX/sizeof(void*)) return -1;
        void *nv=realloc(http->headerv,sizeof(void*)*na);
        if (!nv) return -1;
        http->headerv=nv;
        http->headera=na;
      }
      char *nv=malloc(srcc+1);
      if (!nv) return -1;
      memcpy(nv,src,srcc);
      nv[srcc]=0;
      http->headerv[http->headerc++]=nv;
      
    } else { // Should be either Status-Line or blank.
      int srcp=0,status=0;
      while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++; // skip protocol
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) {
        if ((src[srcp]>='0')&&(src[srcp]<='9')) {
          status*=10;
          status+=src[srcp]-'0';
        } else {
          status=0;
          break;
        }
        srcp++;
      }
      if ((status>=100)&&(status<=999)) {
        http->status=status;
      }
    }
  }
  return c;
}
 
static size_t curlwrap_cb_http_rspbody(char *v,size_t size,size_t c,void *userdata) {
  if ((size==1)&&(c>0)&&(c<INT_MAX)) {
    struct curlwrap_http *http=userdata;
    if (http->rspbodyc>INT_MAX-c) return -1;
    if (http->rspbodyc>=http->rspbodya-(int)c) {
      int na=http->rspbodyc+c;
      if (na<INT_MAX-1024) na=(na+1024)&~1023;
      void *nv=realloc(http->rspbody,na);
      if (!nv) return -1;
      http->rspbody=nv;
      http->rspbodya=na;
    }
    memcpy((char*)http->rspbody+http->rspbodyc,v,c);
    http->rspbodyc+=c;
  }
  return c;
}
 
static size_t curlwrap_cb_http_reqbody(char *v,size_t size,size_t c,void *userdata) {
  if ((size==1)&&(c>0)&&(c<INT_MAX)) {
    struct curlwrap_http *http=userdata;
    int cpc=http->reqbodyc-http->reqbodyp;
    if (cpc>c) cpc=c;
    memcpy(v,http->reqbody+http->reqbodyp,cpc);
    http->reqbodyp+=cpc;
    return cpc;
  }
  return -1;
}
 
static int curlwrap_http_init(
  struct curlwrap *cw,
  struct curlwrap_http *http,
  CURL *easy,
  const char *method,
  const char *url,
  const void *body,int bodyc
) {
  if (!easy) return -1;
  if (curl_easy_setopt(easy,CURLOPT_PRIVATE,http)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_SSL_VERIFYHOST,2)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_SSL_VERIFYPEER,1)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_PROTOCOLS,CURLPROTO_HTTP|CURLPROTO_HTTPS)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_REDIR_PROTOCOLS,CURLPROTO_HTTP|CURLPROTO_HTTPS)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_FOLLOWLOCATION,1)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_HEADERFUNCTION,curlwrap_cb_http_rspheader)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_HEADERDATA,http)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_WRITEFUNCTION,curlwrap_cb_http_rspbody)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_WRITEDATA,http)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_CUSTOMREQUEST,method)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_URL,url)<0) return -1;
  if (bodyc) {
    if (curl_easy_setopt(easy,CURLOPT_READFUNCTION,curlwrap_cb_http_reqbody)<0) return -1;
    if (curl_easy_setopt(easy,CURLOPT_READDATA,http)<0) return -1;
    if (curl_easy_setopt(easy,CURLOPT_UPLOAD,1)<0) return -1;
    if (curl_easy_setopt(easy,CURLOPT_INFILESIZE,bodyc)<0) return -1;
    if (!(http->reqbody=malloc(bodyc))) return -1;
    memcpy(http->reqbody,body,bodyc);
    http->reqbodyc=bodyc;
  }
  if (curl_multi_add_handle(cw->multi,easy)<0) return -1;
  return 0;
}
 
int curlwrap_http_request(
  struct curlwrap *cw,
  const char *method,
  const char *url,
  const void *body,int bodyc
) {
  if (!cw) return -1;
  if (!method||!method[0]) method="GET";
  if (!url||!url[0]) return -1;
  if ((bodyc<0)||(bodyc&&!body)) return -1;
  struct curlwrap_http *http=curlwrap_http_new(cw);
  if (!http) return -1;
  CURL *easy=curl_easy_init();
  if (curlwrap_http_init(cw,http,easy,method,url,body,bodyc)<0) {
    curlwrap_http_remove(cw,http);
    if (easy) curl_easy_cleanup(easy);
    return -1;
  }
  return http->reqid;
}

/* Connect WebSocket.
 */
 
static size_t curlwrap_cb_ws_message(char *v,size_t size,size_t c,void *userdata) {
  struct curlwrap_ws *ws=userdata;
  struct curlwrap *cw=ws->parent;
  if ((size==1)&&(c>0)&&(c<INT_MAX)&&cw->delegate.cb_ws_message&&(cw->id_next>0)) {
    if (ws->msgc>=ws->msga) {
      int na=ws->msga+8;
      if (na>INT_MAX/sizeof(struct curlwrap_ws_msg)) return -1;
      void *nv=realloc(ws->msgv,sizeof(struct curlwrap_ws_msg)*na);
      if (!nv) return -1;
      ws->msgv=nv;
      ws->msga=na;
    }
    struct curlwrap_ws_msg *msg=ws->msgv+ws->msgc++;
    memset(msg,0,sizeof(struct curlwrap_ws_msg));
    msg->msgid=cw->id_next++;
    if (ws->easy) {
      const struct curl_ws_frame *frame=curl_ws_meta(ws->easy);
      if (frame) {
        if (frame->flags&CURLWS_BINARY) msg->opcode=2; else msg->opcode=1;
      }
    }
    if (!(msg->v=malloc(c))) return -1;
    memcpy(msg->v,v,c);
    msg->c=c;
    // If we didn't get the connect signal, pretend we did.
    if (ws->status==0) {
      ws->status=1;
      if (cw->delegate.cb_ws_connect) {
        cw->delegate.cb_ws_connect(cw,ws->wsid);
      }
    }
    cw->delegate.cb_ws_message(cw,ws->wsid,msg->msgid,c);
  }
  return c;
}

static size_t curlwrap_cb_ws_rspheader(char *v,size_t size,size_t c,void *userdata) {
  struct curlwrap_ws *ws=userdata;
  struct curlwrap *cw=ws->parent;
  
  /* We'll take the empty header as a signal that the socket is handshook.
   * Not sure if there's a more explicit signal available.
   * Unfortunately, at this moment curl_ws_send() will fail.
   * Seems to work reliably if we wait until the next update, and report the connection then.
   */
  if ((ws->status==0)&&(size==1)&&(c==2)&&!memcmp(v,"\r\n",2)) {
    ws->almost=1;
  }
  
  return c;
}
 
static int curlwrap_ws_init(
  struct curlwrap *cw,
  struct curlwrap_ws *ws,
  CURL *easy,
  const char *url
) {
  if (!easy) return -1;
  ws->easy=easy;
  if (curl_easy_setopt(easy,CURLOPT_PRIVATE,ws)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_SSL_VERIFYHOST,2)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_SSL_VERIFYPEER,1)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_PROTOCOLS,CURLPROTO_ALL)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_REDIR_PROTOCOLS,CURLPROTO_ALL)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_FOLLOWLOCATION,1)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_HEADERFUNCTION,curlwrap_cb_ws_rspheader)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_HEADERDATA,ws)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_WRITEFUNCTION,curlwrap_cb_ws_message)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_WRITEDATA,ws)<0) return -1;
  if (curl_easy_setopt(easy,CURLOPT_URL,url)<0) return -1;
  if (curl_multi_add_handle(cw->multi,easy)<0) return -1;
  return 0;
}

int curlwrap_ws_connect(struct curlwrap *cw,const char *url) {
  if (!cw||!url||!url[0]) return -1;
  struct curlwrap_ws *ws=curlwrap_ws_new(cw);
  if (!ws) return -1;
  CURL *easy=curl_easy_init();
  if (curlwrap_ws_init(cw,ws,easy,url)<0) {
    curlwrap_ws_remove(cw,ws);
    if (easy) curl_easy_cleanup(easy);
    return -1;
  }
  return ws->wsid;
}

/* Disconnect WebSocket.
 */

void curlwrap_ws_disconnect(struct curlwrap *cw,int wsid) {
  struct curlwrap_ws *ws=curlwrap_ws_by_wsid(cw,wsid);
  if (!ws) return;
  if (ws->easy) {
    curl_multi_remove_handle(cw->multi,ws->easy);
    curl_easy_cleanup(ws->easy);
    ws->easy=0;
  }
  curlwrap_ws_remove(cw,ws);
}

/* Send WebSocket packet.
 */
 
int curlwrap_ws_send(struct curlwrap *cw,int wsid,int opcode,const void *v,int c) {
  if ((c<0)||(c&&!v)) return -1;
  struct curlwrap_ws *ws=curlwrap_ws_by_wsid(cw,wsid);
  if (!ws) return -1;
  if (!ws->easy) return -1;
  size_t sent=0;
  int flags=0;
  switch (opcode) {
    case 1: flags=CURLWS_TEXT; break;
    case 2: flags=CURLWS_BINARY; break;
  }
  int err=curl_ws_send(ws->easy,v,c,&sent,0,flags);
  if (err||(sent!=c)) return -1;
  return 0;
}
