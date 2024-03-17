/* Wasm.js
 * Manages the relationship with our WebAssembly module.
 * In the interest of performance, we expose a lot of instance members for clients to work with directly.
 */
 
export class Wasm {
  constructor(window) {
    this.window = window;
    
    this.instance = null;
    this.memU8 = null;
    this.memU16 = null;
    this.memS16 = null;
    this.memU32 = null;
    this.memS32 = null;
    this.memF32 = null;
    this.memF64 = null;
    this.loadingPromise = null;
    this.textDecoder = new TextDecoder("utf8");
    this.textEncoder = new TextEncoder("utf8");
    
    // Owner should fill this in before loading.
    this.env = {
      rand: () => Math.floor(Math.random() * 0x10000),
      sinf: x => Math.sin(x),
      cosf: x => Math.cos(x),
      roundf: x => Math.round(x),
      atan2f: (x, y) => Math.atan2(x, y),
      modff: (x, yp) => { const fract = x % 1; this.memF32[yp >> 2] = x - fract; return fract; },
      fmodf: (x, y) => x % y,
    };
  }
  
  load(serial) {
    this.abort();
    return this.window.WebAssembly.instantiate(serial, this.generateOptions()).then((result) => {
      this.instance = result.instance;
      this.memU8 = new Uint8Array(this.instance.exports.memory.buffer);
      this.memU16 = new Uint16Array(this.instance.exports.memory.buffer);
      this.memS16 = new Int16Array(this.instance.exports.memory.buffer);
      this.memU32 = new Uint32Array(this.instance.exports.memory.buffer);
      this.memS32 = new Int32Array(this.instance.exports.memory.buffer);
      this.memF32 = new Float32Array(this.instance.exports.memory.buffer);
      this.memF64 = new Float64Array(this.instance.exports.memory.buffer);
      return this.instance;
    });
  }
  
  abort() {
    this.instance = null;
    this.memU8 = null;
    this.memU16 = null;
    this.memS16 = null;
    this.memU32 = null;
    this.memS32 = null;
    this.memF32 = null;
    this.memF64 = null;
  }
  
  generateOptions() {
    return {
      env: this.env,
    };
  }
  
  zstringFromMemory(p) {
    if (!p) return "";
    if (!this.instance) throw new Error(`WASM instance not loaded`);
    if (typeof p !== "number") throw new Error(`expected integer address, got ${p}`);
    if ((p < 0) || (p >= this.memU8.length)) throw new Error(`invalid address ${p}`);
    const sanityLimit = 1024;
    let c = 0;
    while ((p + c < this.memU8.length) && this.memU8[p + c] && (c < sanityLimit)) c++;
    return this.textDecoder.decode(this.memU8.slice(p, p + c));
  }
  
  stringFromMemory(p, c) {
    if (!p) return "";
    if (c < 0) return this.zstringFromMemory(p);
    return this.textDecoder.decode(this.getMemoryView(p, c));
  }
  
  getMemoryView(v, c) {
    if (!this.instance) throw new Error(`WASM instance not loaded`);
    const memory = this.instance.exports.memory.buffer;
    if ((typeof(v) !== "number") || (typeof(c) !== "number") || (v < 0) || (c < 0) || (v > memory.byteLength - c)) {
      throw new Error(`Invalid address ${c} @ ${v}`);
    }
    return new Uint8Array(memory, v, c);
  }
  
  copyMemoryView(v, c) {
    const src = this.getMemoryView(v, c);
    const dst = new Uint8Array(src.length);
    dst.set(src);
    return dst;
  }
  
  safeWrite(dst, dsta, src) {
    if (!src) return 0;
    if (typeof(src) === "string") {
      src = this.textEncoder.encode(src);
    }
    if (src instanceof ArrayBuffer) {
      src = new Uint8Array(src);
    }
    if ((src.BYTES_PER_ELEMENT > 0) && (src.length >= 0)) {
      const srcc = src.BYTES_PER_ELEMENT * src.length;
      if (srcc <= dsta) {
        const dstp = dst;
        dst = this.getMemoryView(dst, srcc);
        dst.set(src);
        if (srcc < dsta) {
          this.memU8[dstp + srcc] = 0;
        }
      }
      return srcc;
    }
    throw new Error(`Expected string, ArrayBuffer, or TypedArray`);
  }
}
