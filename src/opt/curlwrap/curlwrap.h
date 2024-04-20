/* curlwrap.h
 * Wrapper around libcurl.
 * This is designed to export an interface amenable to Egg, the project I'm writing for initially.
 * That means we do a bunch of bookkeeping which would not be necessary in more direct APIs, in fact that's most of what we do.
 * (One might ask "why not send the payload with cb_http_rsp?", and in any other context, that is exactly what we would do).
 */
 
#ifndef CURLWRAP_H
#define CURLWRAP_H

struct curlwrap;

/* Finished HTTP responses, and any WebSocket events, all get reported during update to your delegate.
 * We may report ws_disconnect without ws_connect, eg if the handshake fails.
 * We guarantee not to call ws_message without a prior ws_connect.
 * Every ID'd request that you initiate will eventually trigger rsp or disconnect, unless:
 *  - You close the WebSocket manually.
 *  - You delete the curlwrap while the thing is live.
 */
struct curlwrap_delegate {
  void *userdata;
  void (*cb_http_rsp)(struct curlwrap *cw,int reqid,int status,int length);
  void (*cb_ws_connect)(struct curlwrap *cw,int wsid);
  void (*cb_ws_disconnect)(struct curlwrap *cw,int wsid);
  void (*cb_ws_message)(struct curlwrap *cw,int wsid,int msgid,int length);
};

void curlwrap_del(struct curlwrap *cw);

struct curlwrap *curlwrap_new(const struct curlwrap_delegate *delegate);

void *curlwrap_get_userdata(const struct curlwrap *cw);

/* Poll curl and fire callbacks as warranted.
 * HTTP responses and WebSocket messages will be retained by curlwrap at least until the next update.
 */
int curlwrap_update(struct curlwrap *cw);

/* Returns (reqid>0) on success, or <0 on error.
 */
int curlwrap_http_request(
  struct curlwrap *cw,
  const char *method,
  const char *url,
  const void *body,int bodyc
);

int curlwrap_http_get_status(const struct curlwrap *cw,int reqid);
int curlwrap_http_get_header(void *dstpp,const struct curlwrap *cw,int reqid,const char *k,int kc);
int curlwrap_http_get_body(void *dstpp,const struct curlwrap *cw,int reqid);

int curlwrap_http_for_each(
  const struct curlwrap *cw,
  int (*cb)(int reqid,void *userdata),
  void *userdata
);

/* Returns (wsid>0) on success, or <0 on error.
 */
int curlwrap_ws_connect(struct curlwrap *cw,const char *url);

/* Disconnecting manually does not trigger a callback.
 */
void curlwrap_ws_disconnect(struct curlwrap *cw,int wsid);
int curlwrap_ws_get_message(void *dstpp,const struct curlwrap *cw,int wsid,int msgid);
int curlwrap_ws_send(struct curlwrap *cw,int wsid,int opcode,const void *v,int c);

int curlwrap_ws_for_each(
  const struct curlwrap *cw,
  int (*cb)(int wsid,void *userdata),
  void *userdata
);

#endif
