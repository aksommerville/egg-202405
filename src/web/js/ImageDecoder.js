/* ImageDecoder.js
 * Synchronous decoder for image files.
 * We're not using browser facilities for this, since those can only operate asynchronously.
 * Our public API requires a synchronous pass/fail on loading images.
 * So only three simple formats are supported: rawimg, qoi, and rlead.
 * (rawimg and rlead are my own design)
 */
 
export class ImageDecoder {
  constructor() {
  }
  
  /* (serial) is a Uint8Array or ArrayBuffer containing an encoded image.
   * Returns null or {
   *   v: ArrayBuffer,
   *   w: number,
   *   h: number,
   *   stride: number,
   *   fmt: number, // egg.TextureFormat. [1,2,3,4,5]=[RGBA,A8,A1,Y8,Y1]
   * }
   */
  decode(serial) {
    if (serial instanceof ArrayBuffer) serial = new Uint8Array(serial);
    if (!(serial instanceof Uint8Array)) return null;
    switch (this.detectFormat(serial)) {
      case "rawimg": return this.decode_rawimg(serial);
      case "qoi": return this.decode_qoi(serial);
      case "rlead": return this.decode_rlead(serial);
    }
    return null;
  }
  
  detectFormat(v) {
    if (v.length >= 4) {
      if ((v[0] === 0x00) && (v[1] === 0x72) && (v[2] === 0x61) && (v[3] === 0x77)) return "rawimg";
      if ((v[0] === 0x71) && (v[1] === 0x6f) && (v[2] === 0x69) && (v[3] === 0x66)) return "qoi";
    }
    if (v.length >= 2) {
      if ((v[0] === 0xbb) && (v[1] === 0xad)) return "rlead";
    }
    return "";
  }
  
  /* rawimg
   *******************************************************************/
   
  decode_rawimg(src) {
    
    // 32-byte header. Validate everything.
    const hdrlen = 32;
    if (src.length < hdrlen) return null;
    if (src[0] !== 0x00) return null;
    if (src[1] !== 0x72) return null;
    if (src[2] !== 0x61) return null;
    if (src[3] !== 0x77) return null;
    const w = (src[4] << 8) | src[5];
    if ((w < 1) || (w > 0x7fff)) return null;
    const h = (src[6] << 8) | src[7];
    if ((h < 1) || (h > 0x7fff)) return null;
    // 8..11 rmask
    // 12..15 gmask
    // 16..19 bmask
    // 20..23 amask
    // 24..27 chorder
    // 28 bitorder
    const pixelsize = src[29];
    let stride;
    switch (pixelsize) { // All multiples and factors of 8 are legal for the format but we only accept 1, 8, and 32.
      case 1: {
          if (src[28] !== 0x3e) return null; // bitorder must be '>'
          stride = (w + 7) >> 3;
        } break;
      case 8: stride = w; break;
      case 32: stride = w << 2; break;
      default: return null;
    }
    const pixelslen = stride * h;
    const ctabc = (src[30] << 8) | src[31];
    const ctablen = ctabc << 2;
    if (hdrlen + pixelslen + ctablen > src.length) return null;
    
    // If 32-bit pixels, assume it's RGBA (don't even check), and emit verbatim.
    if (pixelsize === 32) {
      const v = new Uint8Array(pixelslen);
      v.set(new Uint8Array(src.buffer, src.byteOffset + hdrlen, pixelslen));
      return {
        v: v.buffer, w, h, stride,
        fmt: 1, // EGG_TEX_FMT_RGBA
      };
    }
    
    // If a color table is present, expand to RGBA.
    if (ctabc > 0) {
      const ctab = new Uint32Array(ctabc); // don't create as a view; it might require 4-byte alignment (TODO does it?)
      ctab.set(new Uint8Array(src.buffer, src.byteOffset + hdrlen + pixelslen, ctablen));
      const srcpixels = new Uint8Array(src.buffer, src.byteOffset + hdrlen, pixelslen);
      const v = new Uint32Array(w * h);
      switch (pixelsize) {
        case 1: this.expandCtab1(v, srcpixels, ctab, w, h, stride); break;
        case 8: this.expandCtab8(v, srcpixels, ctab, w, h, stride); break;
        default: return null;
      }
      return {
        v: v.buffer,
        w, h,
        stride: w << 2,
        fmt: 1, // EGG_TEX_FMT_RGBA
      };
    }
    
    // Emit as 1-bit or 8-bit, interpretation depending on (amask).
    let fmt;
    if (src[20]) { // (amask) nonzero. Only the first byte matters; since pixelsize must be 1 or 8.
      switch (pixelsize) {
        case 1: fmt = 3; break; // EGG_TEX_FMT_A1
        case 8: fmt = 2; break; // EGG_TEX_FMT_A8
        default: return null;
      }
    } else { // (amask) zero, treat pixels as gray.
      switch (pixelsize) {
        case 1: fmt = 5; break; // EGG_TEX_FMT_Y1
        case 8: fmt = 4; break; // EGG_TEX_FMT_Y8
        default: return null;
      }
    }
    const v = new Uint8Array(pixelslen);
    v.set(new Uint8Array(src.buffer, src.byteOffset + hdrlen, pixelslen));
    return { v: v.buffer, w, h, stride, fmt };
  }
  
  expandCtab1(dst, src, ctab, w, h, stride) {
    for (let rowp=0, dstp=0, yi=h; yi-->0; rowp+=stride) {
      for (let srcp=rowp, srcmask=0x80, xi=w; xi-->0; dstp++) {
        dst[dstp] = ctab[(src[srcp] & srcmask) ? 1 : 0];
        if (srcmask === 0x01) {
          srcmask = 0x80;
          srcp++;
        } else {
          srcmask >>= 1;
        }
      }
    }
  }
  
  expandCtab8(dst, src, ctab, w, h, stride) {
    for (let rowp=0, dstp=0, yi=h; yi-->0; rowp+=stride) {
      for (let srcp=rowp, xi=w; xi-->0; srcp++, dstp++) {
        dst[dstp] = ctab[src[srcp]];
      }
    }
  }
  
  /* qoi
   *******************************************************************/
   
  decode_qoi(src) {
    
    // Header.
    if (src.length < 22) return null;
    if (src[0] !== 0x71) return null;
    if (src[1] !== 0x6f) return null;
    if (src[2] !== 0x69) return null;
    if (src[3] !== 0x66) return null;
    const w = (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];
    const h = (src[8] << 24) | (src[9] << 16) | (src[10] << 8) | src[11];
    if ((w < 1) || (w > 0x7fff)) return null;
    if ((h < 1) || (h > 0x7fff)) return null;
    // src[12] channels
    // src[13] colorspace
    const stride = w << 2;
    const dst = new Uint8Array(stride * h);
    let dstp = 0;
    let srcp = 14;
    
    // Prediction buffer and previous pixel.
    const prev = [0, 0, 0, 0xff];
    const buf = new Uint8Array(256);
    const prevtobuf = () => {
      let bufp = ((prev[0] * 3 + prev[1] * 5 + prev[2] * 7 + prev[3] * 11) & 0x3f) << 2;
      buf[bufp++] = prev[0];
      buf[bufp++] = prev[1];
      buf[bufp++] = prev[2];
      buf[bufp] = prev[3];
    };
    
    // Read encoded stream.
    while (srcp < src.length) {
      if (dstp >= dst.length) break;
      const lead = src[srcp++];
      
      if (lead === 0xfe) { // QOI_OP_RGB
        dst[dstp++] = prev[0] = src[srcp++];
        dst[dstp++] = prev[1] = src[srcp++];
        dst[dstp++] = prev[2] = src[srcp++];
        dst[dstp++] = prev[3];
        prevtobuf();
        continue;
      }
      
      if (lead === 0xff) { // QOI_OP_RGBA
        dst[dstp++] = prev[0] = src[srcp++];
        dst[dstp++] = prev[1] = src[srcp++];
        dst[dstp++] = prev[2] = src[srcp++];
        dst[dstp++] = prev[3] = src[srcp++];
        prevtobuf();
        continue;
      }
      
      switch (lead & 0xc0) {
      
        case 0x00: { // QOI_OP_INDEX
            let bufp = (lead & 0x3f) << 2;
            dst[dstp++] = prev[0] = buf[bufp++];
            dst[dstp++] = prev[1] = buf[bufp++];
            dst[dstp++] = prev[2] = buf[bufp++];
            dst[dstp++] = prev[3] = buf[bufp++];
          } break;
          
        case 0x40: { // QOI_OP_DIFF
            const dr = ((lead >> 4) & 3) - 2;
            const dg = ((lead >> 2) & 3) - 2;
            const db = (lead & 3) - 2;
            prev[0] += dr;
            prev[1] += dg;
            prev[2] += db;
            dst[dstp++] = prev[0];
            dst[dstp++] = prev[1];
            dst[dstp++] = prev[2];
            dst[dstp++] = prev[3];
            prevtobuf();
          } break;
          
        case 0x80: { // QOI_OP_LUMA
            const dg = (lead & 0x3f) - 32;
            const dr = dg + (src[srcp] >> 4) - 8;
            const db = dg + (src[srcp] & 15) - 8;
            srcp++;
            prev[0] += dr;
            prev[1] += dg;
            prev[2] += db;
            dst[dstp++] = prev[0];
            dst[dstp++] = prev[1];
            dst[dstp++] = prev[2];
            dst[dstp++] = prev[3];
            prevtobuf();
          } break;
          
        case 0xc0: { // QOI_OP_RUN
            let c = (lead & 0x3f) + 1;
            while (c-- > 0) {
              if (dstp >= dst.length) break;
              dst[dstp++] = prev[0];
              dst[dstp++] = prev[1];
              dst[dstp++] = prev[2];
              dst[dstp++] = prev[3];
            }
          } break;
      }
    }
    
    return {
      v: dst.buffer,
      w, h, stride,
      fmt: 1, // EGG_TEX_FMT_RGBA
    };
  }
  
  /* rlead
   *******************************************************************/
   
  decode_rlead(src) {
    
    // Header.
    if (src.length < 7) return null;
    if (src[0] !== 0xbb) return null;
    if (src[1] !== 0xad) return null;
    const w = (src[2] << 8) | src[3];
    const h = (src[4] << 8) | src[5];
    if ((w < 1) || (w > 0x7fff)) return null;
    if ((h < 1) || (h > 0x7fff)) return null;
    const flags = src[6];
    const stride = (w + 7) >> 3;
    const dst = new Uint8Array(stride * h);
    let dstp = 0, dstx = 0, dsty = 0;
    let dstmask = 0x80;
    let srcp = 7;
    let srcmask = 0x80;
    let color = flags & 1;
    
    // Read stream.
    while (srcp < src.length) {
      
      // Read next run length.
      let runlen = 0;
      let wordsize = 1;
      for (;;) {
        let word = 0;
        let wordmask = 1 << (wordsize - 1);
        while (wordmask) {
          if (srcp >= src.length) return null;
          const srcbit = src[srcp] & srcmask;
          if (srcmask === 1) {
            srcp++;
            srcmask = 0x80;
          } else {
            srcmask >>= 1;
          }
          if (srcbit) word |= wordmask;
          wordmask >>= 1;
        }
        runlen += word;
        if (word !== (1 << wordsize) - 1) break;
        if (wordsize >= 30) return null;
        wordsize++;
      }
      runlen++;
      
      // When the color is zero, just skip forward. Our output buffer begins all zeroes.
      // This could be done better, you really shouldn't need to count like this.
      if (!color) {
        while (runlen-- > 0) {
          dstx++;
          if (dstmask === 1) {
            dstmask = 0x80;
            dstp++;
          } else {
            dstmask >>= 1;
          }
          if (dstx >= w) {
            dstx = 0;
            dsty++;
            if (dstmask !== 0x80) {
              dstmask = 0x80;
              dstp++;
            }
          }
        }
      
      // Setting pixels. This could also be done better.
      } else {
        while (runlen-- > 0) {
          dst[dstp] |= dstmask;
          if (dstmask === 1) {
            dstmask = 0x80;
            dstp++;
          } else {
            dstmask >>= 1;
          }
          dstx++;
          if (dstx >= w) {
            dstx = 0;
            dsty++;
            if (dstmask !== 0x80) {
              dstmask = 0x80;
              dstp++;
            }
          }
        }
      }
      
      // And then each run toggles the color.
      color ^= 1;
    }
    
    // Undo filter if applicable.
    if (flags & 2) {
      for (let rp=0, wp=stride; wp<dst.length; rp++, wp++) {
        dst[wp] ^= dst[rp];
      }
    }
  
    return {
      v: dst.buffer,
      w, h, stride,
      fmt: (flags & 4) ? 3 : 5, // EGG_TEX_FMT_A1 : EGG_TEX_FMT_Y1
    };
  }
}
