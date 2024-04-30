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
      memcpy: (dst, src, c) => this.memcpy(dst, src, c),
      __main_argc_argv: () => { throw new Error('not implemented'); }, // required with wasi libc
    };
  }
  
  load(serial) {
    this.abort();
    return this.window.WebAssembly.instantiate(serial, this.generateOptions()).then((result) => {
      this.instance = result.instance;
      const buffer = this.instance.exports.memory?.buffer || new ArrayBuffer(0);
      this.memU8 = new Uint8Array(buffer);
      this.memU16 = new Uint16Array(buffer);
      this.memS16 = new Int16Array(buffer);
      this.memU32 = new Uint32Array(buffer);
      this.memS32 = new Int32Array(buffer);
      this.memF32 = new Float32Array(buffer);
      this.memF64 = new Float64Array(buffer);
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
    const invalid = () => { throw new Error(`not implemented`); };
    return {
      env: this.env,
      wasi_snapshot_preview1: {
        args_get: invalid,
        args_sizes_get: invalid,
        clock_res_get: invalid,
        clock_time_get: invalid,
        environ_get: invalid,
        environ_sizes_get: invalid,
        fd_advise: invalid,
        fd_allocate: invalid,
        fd_close: invalid,
        fd_datasync: invalid,
        fd_fdstat_get: invalid,
        fd_fdstat_set_flags: invalid,
        fd_fdstat_set_rights: invalid,
        fd_filestat_get: invalid,
        fd_filestat_set_size: invalid,
        fd_filestat_set_times: invalid,
        fd_pread: invalid,
        fd_prestat_dir_name: invalid,
        fd_prestat_get: invalid,
        fd_pwrite: invalid,
        fd_read: invalid,
        fd_readdir: invalid,
        fd_renumber: invalid,
        fd_seek: invalid,
        fd_sync: invalid,
        fd_tell: invalid,
        fd_write: invalid,
        path_create_directory: invalid,
        path_filestat_get: invalid,
        path_filestat_set_times: invalid,
        path_link: invalid,
        path_open: invalid,
        path_readlink: invalid,
        path_remove_directory: invalid,
        path_rename: invalid,
        path_symlink: invalid,
        path_unlink_file: invalid,
        poll_oneoff: invalid,
        proc_exit: invalid,
        proc_raise: invalid,
        random_get: invalid,
        sched_yield: invalid,
        sock_accept: invalid,
        sock_recv: invalid,
        sock_send: invalid,
        sock_shutdown: invalid,
      },
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
    if (!this.instance.exports.memory?.buffer) throw new Error(`No exported memory`);
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
  
  memcpy(dst, src, c) {
    dst = this.getMemoryView(dst, c);
    src = this.getMemoryView(src, c);
    dst.set(src);
  }
}
