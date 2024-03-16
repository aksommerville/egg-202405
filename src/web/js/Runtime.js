/* Runtime.js
 * Top-level coordinator.
 */
 
import { Rom } from "./Rom.js";
 
export class Runtime {
  constructor(rom, canvas, window) {
    this.rom = rom;
    this.canvas = canvas;
    this.window = window;
    Runtime.singleton = this;
    
    this.pendingAnimationFrame = null;
    this.lastFrameTime = 0;
  }
  
  run() {
    console.log(`Runtime.run`, this);
    this._findAndApplyMetadata();
    this._initializeGlobals();
    this._findClientEntryPoints().then(() => {
      if (this.egg_client_init) {
        if (this.egg_client_init() < 0) throw new Error(`egg_client_init failed`);
      }
      this.lastFrameTime = Date.now();
      this.pendingAnimationFrame = this.window.requestAnimationFrame(() => this.update());
    }).catch(e => {
      console.error(`Error loading game code`, e);
    });
  }
  
  _initializeGlobals() {
    this.gl = this.canvas.getContext("webgl");
    if (!this.gl) throw new Error(`Failed to acquire WebGL context`);
  }
  
  _findClientEntryPoints() {
    let jscb, wasmcb;
    const jsready = new Promise((resolve, reject) => { jscb = { resolve, reject }; });
    const wasmready = new Promise((resolve, reject) => { wasmcb = { resolve, reject }; });
    const jsbin = this.rom.getResource(Rom.TID_js, 0, 1);
    if (jsbin) {
      console.log(`TODO js:1, ${jsbin.length} bytes`);
      this.window.exportModule = mod => {
        console.log(`Runtime exportModule`, mod);
        if (mod.egg_client_init) this.egg_client_init = mod.egg_client_init;
        if (mod.egg_client_quit) this.egg_client_quit = mod.egg_client_quit;
        if (mod.egg_client_update) this.egg_client_update = mod.egg_client_update;
        if (mod.egg_client_render) this.egg_client_render = mod.egg_client_render;
        delete this.window.exportModule;
        jscb.resolve();
      };
      const jsstr = new TextDecoder("utf8").decode(jsbin);
      console.log(jsstr);
      const tag = this.window.document.createElement("SCRIPT");
      tag.innerHTML = jsstr;
      tag.setAttribute("type", "module");
      this.window.document.head.appendChild(tag);
      console.log(`done loading js:1`);
    } else {
      jscb.resolve();
    }
    const wasmbin = this.rom.getResource(Rom.TID_wasm, 0, 1);
    if (wasmbin) {
      console.log(`TODO wasm:1 ${wasmbin.length} bytes`);
      wasmcb.resolve();
    } else {
      wasmcb.resolve();
    }
    return Promise.all([jsready, wasmready]);
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
    const crop = new Uint8Array(serial.length);
    crop.set(serial);
    const mimeType = "image/gif"; // TODO service to guess format from serial
    const blob = new Blob([crop.buffer], { type: mimeType });
    const url = URL.createObjectURL(blob);
    for (const link of this.window.document.querySelectorAll("link[rel='icon']")) link.remove();
    const link = this.window.document.createElement("LINK");
    link.setAttribute("rel", "icon");
    link.setAttribute("type", mimeType);
    link.setAttribute("href", url);
    this.window.document.head.appendChild(link);
  }
  
  update() {
    this.pendingAnimationFrame = null;
    const now = Date.now();
    const elapsed = (now - this.lastUpdateTime) / 1000;
    this.lastUpdateTime = now;
    if (this.egg_client_update) {
      this.egg_client_update(elapsed);
    }
    if (this.egg_client_render) {
      this.egg_client_render(this.gl);
    }
    this.pendingAnimationFrame = this.window.requestAnimationFrame(() => this.update());
  }
}
