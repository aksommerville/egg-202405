/* Net.js
 * Network manager, including public API.
 */
 
import { Input } from "./Input.js";
 
export class Net {
  constructor(window, input) {
    this.window = window;
    this.input = input;
    
    this.http = [];
    this.ws = [];
    this.nextId = 1; // All ID'd things share one namespace.
  }
  
  detach() {
    for (const http of this.http) {
      if (http.reqid > 0) {
        http.reqid = 0; // Let it complete, but suppress callbacks.
      }
    }
    for (let i=this.ws.length; i-->0; ) {
      this._removeWsNow(this.ws[i].wsid);
    }
  }
  
  _removeHttpSoon(reqid) {
    // Let the request linger at least across the next update cycle,
    // so the game has a chance to retrieve the response.
    // We're not privy to update cycles, but we can assume that 500 ms is plenty, I think.
    this.window.setTimeout(() => this._removeHttpNow(reqid), 500);
  }
  
  _removeWsSoon(wsid) {
    this.window.setTimeout(() => this._removeWsNow(wsid), 500);
  }
  
  _removeMessageSoon(wsid, msgid) {
    this.window.setTimeout(() => this._removeMessageNow(wsid, msgid), 500);
  }
  
  _removeHttpNow(reqid) {
    const p = this.http.findIndex(http => http.reqid === reqid);
    if (p < 0) return;
    this.http[p].reqid = -1;
    this.http.splice(p, 1);
  }
  
  _removeWsNow(wsid) {
    const p = this.ws.findIndex(ws => ws.wsid === wsid);
    if (p < 0) return;
    const ws = this.ws[p];
    this.ws.splice(p, 1);
    ws.status = 0;
    ws.sock.close();
  }
  
  _removeMessageNow(wsid, msgid) {
    const ws = this.ws.find(w => w.wsid === wsid);
    if (!ws) return;
    const p = ws.msgv.findIndex(m => m.msgid === msgid);
    if (p < 0) return;
    ws.msgv.splice(p, 1);
  }
  
  onWs(event) {
    const ws = this.ws.find(w => w.sock === event.target);
    if (!ws) return;
    switch (event.type) {
      case "open": {
          ws.status = 1;
          this.input.pushEvent(Input.EVENT_WS_CONNECT, ws.wsid);
        } break;
      case "close":
      case "error": {
          ws.status = 0;
          if (ws.msgv.length) this._removeWsSoon(ws.wsid);
          else this._removeWsNow(ws.wsid);
          this.input.pushEvent(Input.EVENT_WS_DISCONNECT, ws.wsid);
        } break;
      case "message": {
          const msgid = this.nextId++;
          ws.msgv.push({
            msgid,
            msg: event.data,
          });
          this._removeMessageSoon(ws.wsid, msgid);
          this.input.pushEvent(Input.EVENT_WS_MESSAGE, ws.wsid, msgid, event.data.length);
        } break;
    }
  }
  
  /* Public API.
   *****************************************************************************/
      
  http_request(method, url, body) {
    // TODO Access controls.
    const params = { method };
    if (body) params.body = body;
    const http = {
      reqid: this.nextId++,
      method,
      url,
      status: 0,
      // Don't store the request body.
    };
    this.http.push(http);
    this.window.fetch(url, params).then(r => {
      if (http.reqid < 1) throw "cancelled";
      return r.text().then(rspbody => [r, rspbody]); // TODO Should we use ArrayBuffer instead? Get guidance from the caller?
    }).then(([rsp, rspbody]) => {
      if (http.reqid > 0) {
        http.body = rspbody || "";
        http.status = rsp.status || -1;
        http.headers = rsp.headers;
        this.input.pushEvent(Input.EVENT_HTTP_RSP, http.reqid, http.status, rspbody?.length || 0);
        this._removeHttpSoon(http.reqid);
      }
    }).catch(e => {
      if (http.reqid > 0) {
        http.body = "";
        http.status = -1;
        this.input.pushEvent(Input.EVENT_HTTP_RSP, http.reqid, -1, 0);
        this._removeHttpSoon(http.reqid);
      }
    });
    return http.reqid;
  }
  
  http_get_status(reqid) {
    if (reqid < 1) return -1;
    const http = this.http.find(h => h.reqid === reqid);
    if (!http) return -1;
    return http.status;
  }
  
  http_get_header(reqid, k) {
    if (reqid < 1) return "";
    const http = this.http.find(h => h.reqid === reqid);
    if (!http) return "";
    return http.headers?.get(k) || "";
  }
  
  http_get_body(reqid) {
    if (reqid < 1) return "";
    const http = this.http.find(h => h.reqid === reqid);
    if (!http) return "";
    return http.body || "";
  }
  
  ws_connect(url) {
    const sock = new WebSocket(url);
    sock.addEventListener("open", e => this.onWs(e));
    sock.addEventListener("close", e => this.onWs(e));
    sock.addEventListener("error", e => this.onWs(e));
    sock.addEventListener("message", e => this.onWs(e));
    const ws = {
      sock,
      wsid: this.nextId++,
      status: 0,
      msgv: [],
    };
    this.ws.push(ws);
    return ws.wsid;
  }
  
  ws_disconnect(wsid) {
    if (wsid < 1) return;
    const p = this.ws.findIndex(w => w.wsid === wsid);
    if (p < 0) return;
    const ws = this.ws[p];
    this.ws.splice(p, 1);
    ws.sock.close();
  }
  
  ws_get_message(wsid, msgid) {
    if (wsid < 1) return "";
    const ws = this.ws.find(w => w.wsid === wsid);
    if (!ws) return "";
    const msgp = ws.msgv.findIndex(m => m.msgid === msgid);
    if (msgp < 0) return "";
    const msg = ws.msgv[msgp].msg;
    ws.msgv.splice(msgp, 1);
    return msg;
  }
  
  ws_send(wsid, opcode, v) {
    if (wsid < 1) return;
    const ws = this.ws.find(w => w.wsid === wsid);
    if (!ws) return;
    if (!ws.status) return;
    ws.sock.send(v);
  }
}
