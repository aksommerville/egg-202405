/* Runtime.js
 * Top-level coordinator.
 */
 
import { Rom } from "./Rom.js";
import { SysExtra } from "./SysExtra.js";
import { Input } from "./Input.js";
import { Audio } from "./Audio.js";
import { Net } from "./Net.js";
import { Wasm } from "./Wasm.js";
import { RenderGl } from "./RenderGl.js";
 
export class Runtime {
  constructor(rom, canvas, window) {
    this.rom = rom;
    this.canvas = canvas;
    this.window = window;
    this.sysExtra = new SysExtra(window);
    this.input = new Input(window, this.canvas);
    this.audio = new Audio(window, this.rom);
    this.net = new Net(window, this.input);
    this.wasm = new Wasm(window);
    this.render = new RenderGl(canvas, this.rom);
    Runtime.singleton = this;
    
    this.pendingAnimationFrame = null;
    this.lastFrameTime = 0;
    this.terminated = false;
    this.gl = null;
    this.initialCanvasWidth = this.canvas.width;
    this.initialCanvasHeight = this.canvas.height;
    this.originalTitle = this.window.document.title;
    
    this.wasm.env = {
      ...this.wasm.env,
      ...this.getWasmExports(),
    };
  }
  
  run() {
    if (this.terminated) return Promise.reject('terminated');
    this._findAndApplyMetadata();
    this._initializeGlobals();
    return this._findClientEntryPoints()
      .then(() => this.audio.start())
      .then(() => {
      if (this.egg_client_init) {
        if (this.egg_client_init() < 0) throw new Error(`egg_client_init failed`);
      }
      this.lastFrameTime = Date.now();
      this.pendingAnimationFrame = this.window.requestAnimationFrame(() => this.update());
    });
  }
  
  _initializeGlobals() {
    this.gl = this.canvas.getContext("webgl");
    if (!this.gl) throw new Error(`Failed to acquire WebGL context`);
  }
  
  _findClientEntryPoints() {
    return Promise.all([
      this._loadClientJs(),
      this._loadClientWasm(),
    ]).then(() => {
      // Wasm entry points take precedence over JS.
      if (this.wasm.instance) {
        this.egg_client_init = this.wasm.instance.exports.egg_client_init;
        this.egg_client_quit = this.wasm.instance.exports.egg_client_quit;
        this.egg_client_update = this.wasm.instance.exports.egg_client_update;
        this.egg_client_render = this.wasm.instance.exports.egg_client_render;
      }
      if (this.jsexports) {
        if (this.jsexports.egg_client_init && !this.egg_client_init) this.egg_client_init = this.jsexports.egg_client_init;
        if (this.jsexports.egg_client_quit && !this.egg_client_quit) this.egg_client_quit = this.jsexports.egg_client_quit;
        if (this.jsexports.egg_client_update && !this.egg_client_update) this.egg_client_update = this.jsexports.egg_client_update;
        if (this.jsexports.egg_client_render && !this.egg_client_render) this.egg_client_render = this.jsexports.egg_client_render;
      }
      if (!this.egg_client_update && !this.egg_client_render) {
        throw new Error(`Game does not implement egg_client_update or egg_client_render.`);
      }
    });
  }  
  
  _loadClientJs() {
    const jsbin = this.rom.getResource(Rom.TID_js, 0, 1);
    if (!jsbin) return Promise.resolve();
    return new Promise((resolve, reject) => {
      this.window.exportModule = mod => {//XXX exportModule will live on "egg", not "window"
        this.jsexports = mod;
        if (mod.egg_client_init) this.egg_client_init = mod.egg_client_init;
        if (mod.egg_client_quit) this.egg_client_quit = mod.egg_client_quit;
        if (mod.egg_client_update) this.egg_client_update = mod.egg_client_update;
        if (mod.egg_client_render) this.egg_client_render = mod.egg_client_render;
        delete this.window.exportModule;
        resolve();//TODO Should we race a timeout, in case the game doesn't call exportModule?
      };
      const jsstr = new TextDecoder("utf8").decode(jsbin);
      const tag = this.window.document.createElement("SCRIPT");
      tag.innerHTML = jsstr;
      tag.setAttribute("type", "module");
      this.window.document.head.appendChild(tag);
    });
  }
  
  _loadClientWasm() {
    const wasmbin = this.rom.getResource(Rom.TID_wasm, 0, 1);
    if (!wasmbin) return Promise.resolve();
    return this.wasm.load(wasmbin);
  }
  
  _findAndApplyMetadata() {
    const src = this.rom.getResource(Rom.TID_metadata, 0, 1);
    if (!src) return;
    const textDecoder = new TextDecoder("utf8");
    for (let srcp=0; srcp<src.length; ) {
      const kc = src[srcp++];
      const vc = src[srcp++] || 0;
      if (srcp > src.length - vc - kc) break;
      const k = textDecoder.decode(new Uint8Array(src.buffer, src.byteOffset + srcp, kc));
      srcp += kc;
      const v = textDecoder.decode(new Uint8Array(src.buffer, src.byteOffset + srcp, vc));
      srcp += vc;
      this._applyMetadata(k, v);
    }
  }
  
  _applyMetadata(k, v) {
    switch (k) {
      case "title": this.window.document.title = v; break;
      case "framebuffer": {
          const match = v.match(/^(\d+)x(\d+)$/);
          if (match) this._resizeCanvas(+match[1], +match[2]);
        } break;
      case "iconImage": {
          const serial = this.rom.getResource(Rom.TID_image, 0, +v);
          if (serial) this._setFavicon(serial);
        } break;
    }
  }
  
  _resizeCanvas(w, h) {
    if ((w > 0) && (h > 0)) {
      this.canvas.width = w;
      this.canvas.height = h;
    }
  }
  
  _setFavicon(serial) {
    let url = null, mimeType = "";
    if (serial && serial.length) {
      const crop = new Uint8Array(serial.length);
      crop.set(serial);
      if ((crop[0] === 0) && (crop[1] === 0) && (crop[2] === 1) && (crop[3] === 0)) mimeType = "image/x-icon";
      else if ((crop[0] === 0x47) && (crop[1] === 0x49) && (crop[2] === 0x46)) mimeType = "image/gif";
      const blob = new Blob([crop.buffer], { type: mimeType });
      url = URL.createObjectURL(blob);
    }
    for (const link of this.window.document.querySelectorAll("link[rel='icon']")) link.remove();
    if (url) {
      const link = this.window.document.createElement("LINK");
      link.setAttribute("data-egg-favicon", "");
      link.setAttribute("rel", "icon");
      link.setAttribute("type", mimeType);
      link.setAttribute("href", url);
      this.window.document.head.appendChild(link);
    }
  }
  
  terminate() {
    if (this.terminated) return;
    this.input.reset();
    this.input.detach();
    this.audio.stop();
    this.net.detach();
    if (this.egg_client_quit) {
      this.egg_client_quit();
    }
    this.terminated = true;
    if (this.pendingAnimationFrame) {
      this.window.cancelAnimationFrame(this.pendingAnimationFrame);
      this.pendingAnimationFrame = null;
    }
    if (this.gl) {
      // To make it clear that we are not in fact running anymore, black out the canvas.
      this.gl.clearColor(0, 0, 0, 1);
      this.gl.clear(this.gl.COLOR_BUFFER_BIT);
      this.gl.flush();
    }
    this.canvas.width = this.initialCanvasWidth;
    this.canvas.height = this.initialCanvasHeight;
    this._setFavicon(null);
    this.window.document.title = this.originalTitle;
  }
  
  update() {
    this.pendingAnimationFrame = null;
    if (this.terminated) return;
    const now = Date.now();
    const elapsed = (now - this.lastUpdateTime) / 1000;
    this.lastUpdateTime = now;
    this.input.update();
    this.audio.update();
    if (this.egg_client_update) {
      this.egg_client_update(elapsed);
    }
    if (this.terminated) {
      // Client terminated us during update, probably. All good, but don't render or ask for another frame!
      return;
    }
    if (this.egg_client_render) {
      this.render.draw_mode(0, 0, 0xff);
      this.egg_client_render(this.gl);
      this.render.draw_to_main();
    }
    this.pendingAnimationFrame = this.window.requestAnimationFrame(() => this.update());
  }
  
  /* Returns the "egg" object to be made visible to the game.
   * This is our public Javascript API.
   */
  getClientExports() {
    return {
      log: (fmt, ...vargs) => this.sysExtra.log(fmt, vargs),
      event_next: () => this.input.event_next(),
      event_enable: (eventType, eventState) => this.input.event_enable(eventType, eventState),
      input_device_get_name: (devid) => this.input.input_device_get_name(devid),
      input_device_get_ids: (devid) => this.input.input_device_get_ids(devid),
      input_device_get_button: (devid, index) => this.input.input_device_get_button(devid, index),
      input_device_disconnect: (devid) => this.input.input_device_disconnect(devid),
      texture_del: (texid) => this.render.texture_del(texid),
      texture_new: () => this.render.texture_new(),
      texture_load_image: (texid, qual, imageid) => this.render.texture_load_image(texid, qual, imageid),
      texture_upload: (texid, w, h, stride, fmt, src) => this.render.texture_upload(texid, w, h, stride, fmt, src),
      texture_get_header: (texid) => this.render.texture_get_header(texid),
      texture_clear: (texid) => this.render.texture_clear(texid),
      draw_mode: (xfermode, tint, alpha) => this.render.draw_mode(xfermode, tint, alpha),
      draw_rect: (dsttexid, x, y, w, h, pixel) => this.render.draw_rect(dsttexid, x, y, w, h, pixel),
      draw_decal: (dsttexid, srctexid, dstx, dsty, srcx, srcy, w, h, xform) => this.render.draw_decal(dsttexid, srctexid, dstx, dsty, srcx, srcy, w, h, xform),
      draw_tile: (dsttexid, srctexid, v, c) => this.render.draw_tile(dsttexid, srctexid, v, c),
      audio_play_song: (qual, songid, force, repeat) => this.audio.audio_play_song(qual, songid, force, repeat),
      audio_play_sound: (qual, soundid, trim, pan) => this.audio.audio_play_sound(qual, soundid, trim, pan),
      audio_get_playhead: () => this.audio.audio_get_playhead(),
      res_get: (tid, qual, rid) => this.rom.getResource(tid, qual, rid),
      res_id_by_index: (index) => this.rom.resv[index]?.v,
      store_set: (k, v) => this.sysExtra.store_set(k, v),
      store_get: (k) => this.sysExtra.store_get(k),
      store_key_by_index: (index) => this.sysExtra.store_key_by_index(index),
      http_request: (method, url, body) => this.net.http_request(method, url, body),
      http_get_status: (reqid) => this.net.http_get_status(reqid),
      http_get_header: (reqid, k) => this.net.http_get_header(reqid, k),
      http_get_body: (reqid) => this.net.http_get_body(reqid),
      ws_connect: (url) => this.net.ws_connect(url),
      ws_disconnect: (wsid) => this.net.ws_disconnect(url),
      ws_get_message: (wsid, msgid) => this.net.ws_get_message(wsid, msgid),
      ws_send: (wsid, opcode, v) => this.net.ws_send(wsid, opcode, v),
      time_real: () => Date.now() / 1000,
      time_get: () => this.sysExtra.time_get(),
      get_user_languages: () => this.sysExtra.get_user_languages(),
      request_termination: () => this.terminate(),
      is_terminable: () => false,
    };
  }
  
  /* Same API, but some tweaks, what we export to Wasm code.
   */
  getWasmExports() {
    return {
      egg_event_next: (v, a) => this.wasm_event_next(v, a),
      egg_event_enable: (t, s) => this.input.event_enable(t, s),
      egg_input_device_get_name: (v, a, id) => this.wasm_input_device_get_name(v, a, id),
      egg_input_device_get_ids: (vid, pid, ver, id) => this.wasm_input_device_get_ids(vid, pid, ver, id),
      egg_input_device_get_button: (a, b, c, d, e, id, p) => this.wasm_input_device_get_button(a, b, c, d, e, id, p),
      egg_input_device_disconnect: (id) => this.input.input_device_disconnect(id),
      egg_texture_del: (texid) => this.render.texture_del(texid),
      egg_texture_new: () => this.render.texture_new(),
      egg_texture_load_image: (texid, qual, imageid) => this.render.texture_load_image(texid, qual, imageid),
      egg_texture_upload: (texid, w, h, stride, fmt, src, srcc) => this.wasm_texture_upload(texid, w, h, stride, fmt, src, srcc),
      egg_texture_get_header: (wp, hp, fmtp, texid) => this.wasm_texture_get_header(wp, hp, fmtp, texid),
      egg_texture_clear: (texid) => this.render.texture_clear(texid),
      egg_draw_mode: (xfermode, tint, alpha) => this.render.draw_mode(xfermode, tint, alpha),
      egg_draw_rect: (dsttexid, x, y, w, h, pixel) => this.render.draw_rect(dsttexid, x, y, w, h, pixel),
      egg_draw_decal: (dsttexid, srctexid, dstx, dsty, srcx, srcy, w, h, xform) => this.render.draw_decal(dsttexid, srctexid, dstx, dsty, srcx, srcy, w, h, xform),
      egg_draw_tile: (dsttexid, srctexid, v, c) => this.wasm_draw_tile(dsttexid, srctexid, v, c),
      egg_audio_play_song: (q, id, f, r) => this.audio.audio_play_song(q, id, f, r),
      egg_audio_play_sound: (q, id, t, p) => this.audio.audio_play_sound(q, id, t, p),
      egg_audio_get_playhead: () => this.audio.audio_get_playhead(),
      egg_res_get: (v, a, t, q, r) => this.wasm_res_get(v, a, t, q, r),
      egg_res_id_by_index: (t, q, r, p) => this.wasm_res_id_by_index(t, q, r, p),
      egg_store_set: (k, kc, v, vc) => this.wasm_store_set(k, kc, v, vc),
      egg_store_get: (v, vc, k, kc) => this.wasm_store_get(v, vc, k, kc),
      egg_store_key_by_index: (v, a, p) => this.wasm_store_key_by_index(v, a, p),
      egg_http_request: (m, u, v, c) => this.wasm_http_request(m, u, v, c),
      egg_http_get_status: (id) => this.net.http_get_status(id),
      egg_http_get_header: (v, a, id, k, kc) => this.wasm_http_get_header(v, a, id, k, kc),
      egg_http_get_body: (v, a, id) => this.wasm_http_get_body(v, a, id),
      egg_ws_connect: (u) => this.wasm_ws_connect(u),
      egg_ws_disconnect: (id) => this.net.ws_disconnect(id),
      egg_ws_get_message: (v, a, wsid, msgid) => this.wasm_ws_get_message(v, a, wsid, msgid),
      egg_ws_send: (id, o, v, c) => this.wasm_ws_send(id, o, v, c),
      egg_log: (f, v) => this.wasm_log(f, v),
      egg_time_real: () => Date.now() / 1000,
      egg_time_get: (y, m, d, h, M, s, l) => this.wasm_time_get(y, m, d, h, M, s, l),
      egg_get_user_languages: (v, a) => this.wasm_get_user_languages(v, a),
      egg_request_termination: () => this.terminate(),
      egg_is_terminable: () => 0,
    };
  }
  
  /* Wee adapters for WebAssembly.
   */

  wasm_event_next(v, a) {
    let c = this.input.evtq.length;
    if (c > a) c = a;
    if (c > 0) {
      v >>= 2; // all fields are 32-bit
      for (let i=0; i<c; i++) {
        const evt = this.input.evtq[i];
        this.wasm.memU32[v++] = evt.eventType;
        this.wasm.memU32[v++] = evt.v0;
        this.wasm.memU32[v++] = evt.v1;
        this.wasm.memU32[v++] = evt.v2;
        this.wasm.memU32[v++] = evt.v3;
      }
      this.input.evtq.splice(0, c);
    }
    return c;
  }
  
  wasm_input_device_get_name(dst, dsta, devid) {
    const name = this.input.input_device_get_name(devid);
    return this.wasm.safeWrite(dst, dsta, name);
  }
  
  wasm_input_device_get_ids(vid, pid, version, devid) {
    const ids = this.input.input_device_get_ids(devid);
    if (ids) {
      if (vid) this.wasm.memU32[vid >> 2] = ids.vid;
      if (pid) this.wasm.memU32[pid >> 2] = ids.pid;
      if (version) this.wasm.memU32[version >> 2] = ids.version;
    }
  }
  
  wasm_input_device_get_button(btnid, hidusage, lo, hi, value, devid, p) {
    const btn = this.input.input_device_get_button(devid, p);
    if (btn) {
      if (btnid) this.wasm.memU32[btnid >> 2] = btn.btnid;
      if (hidusage) this.wasm.memU32[hidusage >> 2] = btn.hidusage;
      if (lo) this.wasm.memU32[lo >> 2] = btn.lo;
      if (hi) this.wasm.memU32[hi >> 2] = btn.hi;
      if (value) this.wasm.memU32[value >> 2] = btn.value;
    }
  }
  
  wasm_texture_upload(texid, w, h, stride, fmt, src, srcc) {
    src = this.wasm.getMemoryView(src, srcc);
    return this.render.texture_upload(texid, w, h, stride, fmt, src);
  }
  
  wasm_texture_get_header(wp, hp, fmtp, texid) {
    const header = this.render.texture_get_header(texid);
    if (header) {
      if (wp) this.wasm.memU32[wp >> 2] = header.w;
      if (hp) this.wasm.memU32[hp >> 2] = header.h;
      if (fmtp) this.wasm.memU32[fmtp >> 2] = header.fmt;
    }
  }
  
  wasm_draw_tile(dsttexid, srctexid, vtxp, vtxc) {
    const src = this.wasm.getMemoryView(vtxp, vtxc * 6);
    return this.render.draw_tile(dsttexid, srctexid, src, vtxc);
  }
  
  wasm_res_get(dst, dsta, tid, qual, rid) {
    const res = this.rom.getResource(tid, qual, rid);
    return this.wasm.safeWrite(dst, dsta, res);
  }
  
  wasm_res_id_by_index(tid, qual, rid, p) {
    const res = this.rom.resv[p];
    if (res) {
      if (tid) this.wasm.memU32[tid >> 2] = res.tid;
      if (qual) this.wasm.memU32[qual >> 2] = res.qual;
      if (rid) this.wasm.memU32[rid >> 2] = res.rid;
    }
  }
  
  wasm_store_set(k, kc, v, vc) {
    k = this.wasm.stringFromMemory(k, kc);
    v = this.wasm.stringFromMemory(v, vc);
    return this.sysExtra.store_set(k, v);
  }
  
  wasm_store_get(dst, dsta, k, kc) {
    k = this.wasm.stringFromMemory(k, kc);
    const src = this.sysExtra.store_get(k);
    return this.wasm_safeWrite(dst, dsta, src);
  }
  
  wasm_store_key_by_index(dst, dsta, p) {
    const src = this.sysExtra.store_key_by_index(p);
    return this.wasm.safeWrite(dst, dsta, src);
  }
  
  wasm_http_request(method, url, body, bodyc) {
    method = this.wasm.zstringFromMemory(method);
    url = this.wasm.zstringFromMemory(url);
    if (body) body = this.wasm.getMemoryView(body, bodyc);
    return this.net.http_request(method, url, body);
  }
  
  wasm_http_get_header(dst, dsta, reqid, k, kc) {
    k = this.wasm.stringFromMemory(k, kc);
    const v = this.net.http_get_header(reqid, k);
    return this.wasm.safeWrite(dst, dsta, v);
  }
  
  wasm_http_get_body(dst, dsta, reqid) {
    const src = this.net.http_get_body(reqid);
    return this.wasm.safeWrite(dst, dsta, src);
  }
  
  wasm_ws_connect(url) {
    url = this.wasm.zstringFromMemory(url);
    return this.net.ws_connect(url);
  }
  
  wasm_ws_get_message(dst, dsta, wsid, msgid) {
    const src = this.net.ws_get_message(wsid, msgid);
    return this.wasm.safeWrite(dst, dsta, src);
  }
  
  wasm_ws_send(wsid, opcode, v, c) {
    v = this.wasm.getMemoryView(v, c);
    this.net.ws_send(wsid, opcode, v);
  }
  
  wasm_log(fmt, vargs) {
    fmt = this.wasm.zstringFromMemory(fmt);
    this.sysExtra.log(fmt, (spec) => {
      switch (spec) {
        case 'u':
        case 'd':
        case 'x':
        case 'p':
        case 'c': {
            const v = this.wasm.memS32[vargs >> 2];
            vargs += 4;
            return v;
          }
        case 'l': {
            if (vargs & 7) vargs += 8 - (vargs & 7);
            const v = this.wasm.memS32[vargs >> 2] * 4295976296.0 + this.wasm.memS32[(vargs >> 2) + 1];
            vargs += 8;
            return v;
          }
        case 'f': {
            if (vargs & 7) vargs += 8 - (vargs & 7);
            const v = this.wasm.memF64[vargs >> 3];
            vargs += 8;
            return v;
          }
        case 's': {
            let v = "";
            try {
              v = this.wasm.zstringFromMemory(this.wasm.memU32[vargs >> 2]);
            } catch (e) {}
            vargs += 4;
            return v;
          }
      }
      return null;
    });
  }
  
  wasm_time_get(y, mon, d, h, min, sec, mil) {
    const t = this.sysExtra.time_get();
    if (y) this.wasm.memU32[y >> 2] = t.year;
    if (mon) this.wasm.memU32[mon >> 2] = t.month;
    if (d) this.wasm.memU32[d >> 2] = t.day;
    if (h) this.wasm.memU32[h >> 2] = t.hour;
    if (min) this.wasm.memU32[min >> 2] = t.minute;
    if (sec) this.wasm.memU32[sec >> 2] = t.second;
    if (mil) this.wasm.memU32[mil >> 2] = t.milli;
  }
  
  wasm_get_user_languages(dst, dsta) {
    const src = this.sysExtra.get_user_languages();
    const dstc = Math.min(src.length, dsta);
    dst >>= 2;
    for (let i=0; i<dstc; i++, dst++) {
      this.wasm.memU32[dst] = src[i];
    }
    return dstc;
  }

}
