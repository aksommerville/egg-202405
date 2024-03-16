/* Rom.js
 * Decodes the Egg ROM file and makes its resources available.
 */
 
export class Rom {
  constructor(src) {
    this._initDefault();
    if (!src) ;
    else if (typeof(src) === "string") this._initBase64(src);
    else if (src instanceof ArrayBuffer) this._initBinary(new Uint8Array(src));
    else if (src instanceof Uint8Array) this._initBinary(src);
    else if (src instanceof Rom) this._initCopy(src);
    else throw new Error(`Expected Uint8Array of Egg ROM file`);
  }
  
  getResource(tid, qual, rid) {
    return this.resv.find(res => ((res.tid === tid) && (res.qual === qual) && (res.rid === rid)))?.v;
  }
  
  _initDefault() {
    this.resv = []; // {tid,qual,rid,v:Uint8Array}
  }
  
  _initBinary(src) {
    if (src.length < 16) throw new Error(`Impossible length ${src.length} for Egg ROM`);
    if ((src[0] !== 0x00) || (src[1] !== 0x0e) || (src[2] !== 0x67) || (src[3] !== 0x67)) {
      throw new Error(`Egg ROM signature mismatch`);
    }
    const hdrlen = (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];
    const toclen = (src[8] << 24) | (src[9] << 16) | (src[10] << 8) | src[11];
    const heaplen = (src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15];
    if ((hdrlen < 16) || (toclen < 0) || (heaplen < 0) || (hdrlen + toclen + heaplen > src.length)) {
      throw new Error(`Invalid Egg ROM file geometry (${hdrlen},${toclen},${heaplen},${src.length})`);
    }
    const toc0 = hdrlen;
    const heap0 = toc0 + toclen;
    let tocp = 0, heapp = 0, tid = 1, qual = 0, rid = 1;
    while (tocp < toclen) {
      const lead = src[toc0 + tocp++];
      
      if (!(lead & 0xc0)) { // NEXTTYPE
        tid += lead + 1;
        qual = 0;
        rid = 1;
        continue;
      }
      
      if (lead === 0x40) { // NEXTQUAL
        if (tocp > toclen - 2) throw new Error(`Premature EOF in Egg ROM`);
        let d = (src[toc0 + tocp] << 8) | src[toc0 + tocp + 1];
        d += 1;
        tocp += 2;
        qual += d;
        rid = 1;
        continue;
      }
      
      if ((lead & 0xc0) === 0x80) { // SHORT
        if (rid > 0xffff) throw new Error(`Invalid resource ID in Egg ROM`);
        const len = lead & 0x3f;
        if (heapp > heaplen - len) throw new Error(`Premature EOF in Egg ROM`);
        this._append(tid, qual, rid, src, heap0 + heapp, len);
        heapp += len;
        rid++;
        continue;
      }
      
      if ((lead & 0xf0) === 0xc0) { // LONG
        if (rid > 0xffff) throw new Error(`Invalid resource ID in Egg ROM`);
        if (tocp > toclen - 2) throw new Error(`Premature EOF in Egg ROM`);
        const len = ((lead & 0x0f) << 16) | (src[toc0 + tocp] << 8) | src[toc0 + tocp + 1];
        tocp += 2;
        if (heapp > heaplen - len) throw new Error(`Premature EOF in Egg ROM`);
        this._append(tid, qual, rid, src, heap0 + heapp, len);
        heapp += len;
        rid++;
        continue;
      }
      
      throw new Error(`Unexpected TOC byte ${lead} at ${toc0 + tocp - 1}/${src.length} in Egg ROM`);
    }
  }
  
  _initBase64(src) {
    // base64 decode string to ArrayBuffer... why doesn't that exist yet?
    const bina = Math.ceil((src.length * 3) / 4);
    const bin = new Uint8Array(bina);
    const tmp = new Uint8Array(4); // intake, values are 0..63
    let tmpc = 0, binc = 0;
    for (let srcp=0; srcp<src.length; srcp++) {
      const ch = src.charCodeAt(srcp);
      if (ch <= 0x20) continue;
      if (ch === 0x3d) { // '=': Flush buffer.
        if (tmpc) {
          for (; tmpc<4; tmpc++) tmp[tmpc] = 0;
        }
      } else if ((ch >= 0x41) && (ch <= 0x5a)) {
        tmp[tmpc++] = ch - 0x41;
      } else if ((ch >= 0x61) && (ch <= 0x7a)) {
        tmp[tmpc++] = ch - 0x61 + 26;
      } else if ((ch >= 0x30) && (ch <= 0x39)) {
        tmp[tmpc++] = ch - 0x30 + 52;
      } else if (ch === 0x2b) {
        tmp[tmpc++] = 0x3e;
      } else if (ch === 0x2f) {
        tmp[tmpc++] = 0x3f;
      } else {
        throw new Error(`Unexpected character ${ch} in base64.`);
      }
      if (tmpc >= 4) {
        bin[binc++] = (tmp[0] << 2) | (tmp[1] >> 4);
        bin[binc++] = (tmp[1] << 4) | (tmp[2] >> 2);
        bin[binc++] = (tmp[2] << 6) | tmp[3];
        tmpc = 0;
      }
    }
    switch (tmpc) {
      case 1: {
          bin[binc++] = (tmp[0] << 2);
        } break;
      case 2: {
          bin[binc++] = (tmp[0] << 2) | (tmp[1] >> 4);
        } break;
      case 3: {
          bin[binc++] = (tmp[0] << 2) | (tmp[1] >> 4);
          bin[binc++] = (tmp[1] << 4) | (tmp[2] >> 2);
        } break;
    }
    if (binc > bina) throw new Error(`Miscalculated base64 bounds (${binc}>${bina})`);
    return this._initBinary(new Uint8Array(bin.buffer, 0, binc));
  }
  
  _initCopy(src) {
    // Don't bother copying the serial dumps, they are supposed to be read-only.
    this.resv = src.resv.map(res => ({ ...res }));
  }
  
  _append(tid, qual, rid, src, srcp, srcc) {
    if (!srcc) return; // No need to store empties.
    this.resv.push({ tid, qual, rid, v: new Uint8Array(src.buffer, src.byteOffset + srcp, srcc) });
  }
}

Rom.TID_metadata = 1;
Rom.TID_wasm = 2;
Rom.TID_js = 3;
Rom.TID_image = 4;
Rom.TID_string = 5;
Rom.TID_song = 6;
Rom.TID_sound = 7;
Rom.TID_map = 8;
