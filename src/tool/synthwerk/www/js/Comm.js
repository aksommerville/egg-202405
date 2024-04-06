/* Comm.js
 * Manages HTTP calls to our server, to compile and print waves.
 * We also serve as a bus for the sound text, bouncing changes between TextUi and BoardUi.
 */
 
const COMPILE_DEBOUNCE_MS = 1000;
 
export class Comm {
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
    
    this.nextListenerId = 1;
    this.listeners = [];
    this.text = "";
    this.dirty = false;
    this.compileTimeout = null;
  }
  
  /* cb(event). event:
   *   { type:"dirty" }
   *   { type:"clean" } // fires immediately after "ready" or "error"
   *   { type:"ready", bin:Uint8Array, wave:Int16Array }
   *   { type:"error", error:any }
   *   { type:"text", text:string, sender:any } // every change, immediately
   */
  listen(cb) {
    const id = this.nextListenerId++;
    this.listeners.push({ cb, id });
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p >= 0) this.listeners.splice(p, 1);
  }
  
  broadcast(event) {
    for (const { cb } of this.listeners) cb(event);
  }
  
  /* Call every time something changes, fine to call at keystroke cadence.
   * We will regulate outgoing requests and dirty state.
   * Provide a sender if you need to distinguish your own changes from those of others.
   * (don't depend on the returned content; that will be normalized).
   */
  textChanged(text, sender) {
    text = this.normalizeText(text);
    if (text === this.text) return;
    this.text = text;
    this.compileLater();
    if (!this.dirty) {
      this.dirty = true;
      this.broadcast({ type: "dirty" });
    }
    this.broadcast({ type: "text", text, sender });
  }
  
  normalizeText(src) {
    return src.split("\n").map(line => line.replace(/\s+/g, ' ').trim()).join('\n');
  }
  
  compileLater() {
    if (this.compileTimeout) {
      this.window.clearTimeout(this.compileTimeout);
    }
    this.compileTimeout = this.window.setTimeout(() => {
      this.compileTimeout = null;
      this.compileNow();
    }, COMPILE_DEBOUNCE_MS);
  }
  
  compileNow() {
    this.window.fetch("/api/compile", {
      method: "POST",
      body: this.text,
    }).then(rsp => {
      if (!rsp.ok) throw rsp;
      return rsp.json();
    }).then(body => {
      this.dirty = false;
      this.decodeResponse(body);
      this.broadcast({ type: "ready", bin: this.bin, wave: this.wave });
    }).catch(error => {
      this.dirty = false;
      this.broadcast({ type: "error", error });
    });
  }
  
  /* Take JSON response body from POST /api/compile, populate (bin,wave).
   */
  decodeResponse(rsp) {
    this.bin = this.window.atob(rsp.bin);
    if (rsp.wave) {
      this.wave = new Int16Array(rsp.wave.length >> 2);
      for (let dstp=0, srcp=0; dstp<this.wave.length; dstp++, srcp+=4) {
        this.wave[dstp] = parseInt(rsp.wave.substring(srcp, srcp+4), 16);
      }
    } else {
      this.wave = new Int16Array(1);
    }
  }
}

Comm.singleton = true;
