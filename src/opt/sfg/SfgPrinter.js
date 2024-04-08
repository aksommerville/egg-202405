/* SfgPrinter.js
 * Produce PCM dumps from our tiny binary sound format.
 *
 * Unlike our C counterpart, we run completely synchronously.
 * That's because in WebAudio you don't get intimate access to the signal generator,
 * what we use to time and pay out printing in C.
 * Printing is not trivial. There's a real possibility of missing video frames due to sound effects being printed.
 *
 * It's written as a class to keep things flexible.
 * But typical usage is a one-shot kind of deal:
 *   const myPcm = new SfgPrinter(serial).print();
 * Uint8Array in, Float32Array out.
 */
 
export class SfgPrinter {
  constructor(src, rate) {
    if (!(src instanceof Uint8Array)) throw new Error(`Expected Uint8Array`);
    if (!rate || (rate < 200) || (rate > 200000)) throw new Error(`Invalid rate for SfgPrinter`);
    this.src = src;
    this.rate = rate;
    this.dst = null; // Float32Array
  }
  
  print() {
    if (!this.dst) this._print();
    return this.dst;
  }
  
  /* Private below this point.
   */
  
  _print() {
    if (this.src.length < 4) throw new Error(`Invalid SFG`);
    const durms = (this.src[0] << 8) | this.src[1];
    const durframes = Math.max(1, Math.round(durms * this.rate / 1000));
    const master = this.src[2] + this.src[3] / 256.0;
    this.dst = new Float32Array(durframes);
    const tmp = new Float32Array(durframes);
    // Add one voice at a time to (this.dst).
    // Unlike the C implementation, we're synchronous, so we kind of process everything as we read it.
    for (let srcp=4; srcp<this.src.length; ) {
      const np = this._printVoice(tmp, srcp);
      if (!np || (np <= srcp)) throw new Error(`PCM print stalled at ${srcp}/${this.src.length}`);
      srcp = np;
      for (let i=durframes; i-->0; ) this.dst[i] += tmp[i];
    }
    // Apply master level.
    for (let i=durframes; i-->0; ) this.dst[i] *= master;
  }
  
  // Overwrite (dst), and return new (srcp).
  _printVoice(dst, srcp) {
    const waveSizeBits = 10;
    const waveSizeSamples = 1 << waveSizeBits;

    const features = this.src[srcp++];
    
    let wave = null; // Float32Array(waveSizeSamples) | "noise" | "silence"
    if (features & 0x01) { // shape
      switch (this.src[srcp++]) {
        case 0: wave = this._printSine(waveSizeSamples); break;
        case 1: wave = this._printSquare(waveSizeSamples); break;
        case 2: wave = this._printSawup(waveSizeSamples); break;
        case 3: wave = this._printSawdown(waveSizeSamples); break;
        case 4: wave = this._printTriangle(waveSizeSamples); break;
        case 5: wave = "noise"; break;
        case 6: wave = "silence"; break;
        default: throw new Error(`Unknown wave shape ${this.src[srcp-1]}`);
      }
    } else {
      wave = this._printSine(waveSizeSamples);
    }
    
    if (features & 0x02) { // harmonics
      const coefc = this.src[srcp++];
      if (wave instanceof Float32Array) {
        const nwave = new Float32Array(waveSizeSamples);
        for (let step=1; step<=coefc; step++) {
          this._printHarmonic(nwave, wave, step, this.src[srcp++] / 255.0);
        }
        wave = nwave;
      } else {
        // harmonics is meaningless for noise or silence, but it is technically legal. skip it.
        srcp += coefc;
      }
    }
    
    let fmrate = 0;
    let fmscale = 0;
    if (features & 0x04) { // fm
      fmrate = this.src[srcp] + this.src[srcp+1] / 256.0; srcp += 2;
      fmscale = this.src[srcp] + this.src[srcp+1] / 256.0; srcp += 2;
      fmrate *= Math.PI * 2;
    }
    
    const fmrange = {};
    if (features & 0x08) { // fmenv
      srcp = this._decodeEnv(fmrange, srcp, fmscale / 65535.0);
    } else {
      this._constantEnv(fmrange, fmscale);
    }
    
    const rate = {};
    if (features & 0x10) { // rate
      srcp = this._decodeEnv(rate, srcp, 1 / this.rate);
    } else {
      this._constantEnv(rate, 440 / this.rate);
    }
    
    let ratelforate = 0;
    let ratelfodepth = 0;
    if (features & 0x20) { // ratelfo
      const rate = this.src[srcp] + this.src[srcp+1] / 256.0; srcp += 2; // hz
      const depth = (this.src[srcp] << 8) | this.src[srcp+1]; srcp += 2; // cents
      ratelforate = (rate * Math.PI * 2) / this.rate; // radians/frame
      ratelfodepth = depth / 1200; // power of 2
    }
    
    if (features & 0xc0) throw new Error(`Unknown features (0x${(features & 0xc0).toString(16)}) in voice`);
    
    /* We now have everything we need for the oscillator.
     * Run it to completion, overwriting (dst).
     */
    if (wave === "silence") {
      for (let i=dst.length; i-->0; ) dst[i] = 0;
    } else if (wave === "noise") {
      for (let i=dst.length; i-->0; ) dst[i] = Math.random() * 2 - 1;
    } else { //TODO Opportunities here to run simpler oscillators, eg if rate LFO or FM not in play. See sfg_update.c
      this._oscillateFull(
        dst, wave, rate, ratelforate, ratelfodepth, fmrate, fmrange
      );
    }
    
    /* Process all positional operations individually.
     */
    while (srcp < this.src.length) {
      const opcode = this.src[srcp++];
      if (!opcode) break; // End Of Voice
      switch (opcode) {
        case 0x01: srcp = this._printOpLevel(dst, srcp); break;
        case 0x02: srcp = this._printOpGain(dst, srcp); break;
        case 0x03: srcp = this._printOpClip(dst, srcp); break;
        case 0x04: srcp = this._printOpDelay(dst, srcp); break;
        case 0x05: srcp = this._printOpBandpass(dst, srcp); break;
        case 0x06: srcp = this._printOpNotch(dst, srcp); break;
        case 0x07: srcp = this._printOpLopass(dst, srcp); break;
        case 0x08: srcp = this._printOpHipass(dst, srcp); break;
        default: throw new Error(`Unknown voice op ${opcode}`);
      }
    }
  
    return srcp;
  }
  
  /* Oscillator.
   */
   
  _oscillateFull(dst, wave, rate, ratelforate, ratelfodepth, fmrate, fmrange) {
    for (let i=0, ratelfop=0, modp=0, carp=0; i<dst.length; i++) {
  
      // Acquire carrier rate.
      let crate = this._updateEnv(rate);
      crate *= Math.pow(2, Math.sin(ratelfop) * ratelfodepth);
      ratelfop += ratelforate;
      if (ratelfop >= Math.PI) ratelfop -= Math.PI * 2;
    
      // Acquire modulation.
      let mod = Math.sin(modp);
      mod *= this._updateEnv(fmrange);
      modp += crate * fmrate;
      if (modp >= Math.PI) modp -= Math.PI * 2;
    
      // Acquire sample and advance carrier.
      const sp = Math.floor(carp * wave.length);
      dst[i] = wave[sp] || 0;
      crate += crate * mod;
      carp += crate;
      while (carp >= 1) carp -= 1;
      while (carp < 0) carp += 1;
    }
  }
  
  /* Positional ops.
   * These all read from (this.src) and return the new position.
   */
  
  _printOpLevel(dst, srcp) {
    const env = {};
    srcp = this._decodeEnv(env, srcp, 1 / 65535.0);
    let lo=dst[0], hi=dst[0];
    for (let i=dst.length; i-->0; ) {
      if (dst[i]<lo) lo=dst[i];
      else if (dst[i]>hi) hi=dst[i];
    }
    for (let i=0; i<dst.length; i++) {
      dst[i] *= this._updateEnv(env);
    }
    return srcp;
  }
  
  _printOpGain(dst, srcp) {
    const gain = this.src[srcp] + this.src[srcp+1] / 256.0; srcp += 2;
    for (let i=dst.length; i-->0; ) {
      dst[i] *= gain;
    }
    return srcp;
  }
  
  _printOpClip(dst, srcp) {
    const hi = this.src[srcp++] / 255.0;
    const lo = -hi;
    for (let i=dst.length; i-->0; ) {
      const v = dst[i];
      if (v > hi) dst[i] = hi;
      else if (v < lo) dst[i] = lo;
    }
    return srcp;
  }
  
  _printOpDelay(dst, srcp) {
    const ms = (this.src[srcp] << 8) | this.src[srcp+1]; srcp += 2;
    const period = Math.ceil((ms * this.rate) / 1000);
    if (isNaN(period) || (period < 1)) return srcp + 4;
    const buf = new Float32Array(period);
    let bufp = 0;
    const dry = this.src[srcp++] / 255.0;
    const wet = this.src[srcp++] / 255.0;
    const sto = this.src[srcp++] / 255.0;
    const fbk = this.src[srcp++] / 255.0;
    for (let i=0; i<dst.length; i++) {
      const next = dst[i];
      const prev = buf[bufp];
      dst[i] = next * dry + prev * wet;
      buf[bufp] = next * sto + prev * fbk;
      if (++bufp >= period) bufp = 0;
    }
    return srcp;
  }
  
  _printOpBandpass(dst, srcp) {
    const mid = ((this.src[srcp] << 8) | this.src[srcp+1]) / this.rate; srcp += 2;
    const wid = ((this.src[srcp] << 8) | this.src[srcp+1]) / this.rate; srcp += 2;
    const r = 1 - 3 * wid;
    const cosfreq = Math.cos(Math.PI * 2 * mid);
    const k = (1 - 2 * r * cosfreq + r * r) / (2 - 2 * cosfreq);
    const coefv = [
      1 - k,
      2 * (k - r) * cosfreq,
      r * r - k,
      2 * r * cosfreq,
      -r * r,
    ];
    this._applyIir(dst, coefv);
    return srcp;
  }
  
  _printOpNotch(dst, srcp) {
    const mid = ((this.src[srcp] << 8) | this.src[srcp+1]) / this.rate; srcp += 2;
    const wid = ((this.src[srcp] << 8) | this.src[srcp+1]) / this.rate; srcp += 2;
    const r = 1 - 3 * wid;
    const cosfreq = Math.cos(Math.PI * 2 * mid);
    const k = (1 - 2 * r * cosfreq + r * r) / (2 - 2 * cosfreq);
    const coefv = [
      k,
      -2 * k * cosfreq,
      k,
      2 * r * cosfreq,
      -r * r,
    ];
    this._applyIir(dst, coefv);
    return srcp;
  }
  
  _printOpLopass(dst, srcp) {
    const freq = ((this.src[srcp] << 8) | this.src[srcp+1]) / this.rate; srcp += 2;
    const rp = -Math.cos(Math.PI / 2);
    const ip = Math.sin(Math.PI / 2);
    const t = 2 * Math.tan(0.5);
    const w = 2 * Math.PI * freq;
    const m = rp * rp + ip * ip;
    const d = 4 - 4 * rp * t + m * t * t;
    const x0 = (t * t) / d;
    const x1 = (2 * t * t) / d;
    const x2 = (t * t) / d;
    const y1 = (8 - 2 * m * t * t) / d;
    const y2 = (-4 - 4 * rp * t - m * t * t) / d;
    const k = Math.sin(0.5 - w / 2) / Math.sin(0.5 + w / 2);
    const coefv = [
      (x0 - x1 * k + x2 * k * k) / d,
      (-2 * x0 * k + x1 + x1 * k * k - 2 * x2 * k) / d,
      (x0 * k * k - x1 * k + x2) / d,
      (2 * k + y1 + y1 * k * k - 2 * y2 * k) / d,
      (-k * k - y1 * k + y2) / d,
    ];
    this._applyIir(dst, coefv);
    return srcp;
  }
  
  _printOpHipass(dst, srcp) {
    const freq = ((this.src[srcp] << 8) | this.src[srcp+1]) / this.rate; srcp += 2;
    const rp = -Math.cos(Math.PI / 2);
    const ip = Math.sin(Math.PI / 2);
    const t = 2 * Math.tan(0.5);
    const w = 2 * Math.PI * freq;
    const m = rp * rp + ip * ip;
    const d = 4 - 4 * rp * t + m * t * t;
    const x0 = (t * t) / d;
    const x1 = (2 * t * t) / d;
    const x2 = (t * t) / d;
    const y1 = (8 - 2 * m * t * t) / d;
    const y2 = (-4 - 4 * rp * t - m * t * t) / d;
    const k = -Math.cos(w / 2 + 0.5) / Math.cos(w / 2 - 0.5);
    const coefv = [
      -(x0 - x1 * k + x2 * k * k) / d,
      (-2 * x0 * k + x1 + x1 * k * k - 2 * x2 * k) / d,
      (x0 * k * k - x1 * k + x2) / d,
      -(2 * k + y1 + y1 * k * k - 2 * y2 * k) / d,
      (-k * k - y1 * k + y2) / d,
    ];
    this._applyIir(dst, coefv);
    return srcp;
  }
  
  _applyIir(dst, coefv) {
    const statev = [0, 0, 0, 0, 0];
    for (let i=0; i<dst.length; i++) {
      statev[2]=statev[1];
      statev[1]=statev[0];
      statev[0]=dst[i];
      dst[i]=(
        statev[0]*coefv[0]+
        statev[1]*coefv[1]+
        statev[2]*coefv[2]+
        statev[3]*coefv[3]+
        statev[4]*coefv[4]
      );
      statev[4]=statev[3];
      statev[3]=dst[i];
    }
  }
  
  /* Envelopes.
   * Decode into a blank object.
   * Values are in 0..65535 if you don't scale. (eg 1/65535 to normalize)
   */
   
  _decodeEnv(env, srcp, scale) {
    env.v0 = (this.src[srcp] << 8) | this.src[srcp+1]; srcp += 2;
    env.v0 *= scale;
    const pointc = this.src[srcp++];
    if (isNaN(pointc) || (srcp > this.src.length - pointc * 4)) throw new Error(`Envelope overruns EOF`);
    env.pointv = [];
    for (let i=pointc; i-->0; ) {
      const ms = (this.src[srcp] << 8) | this.src[srcp+1]; srcp += 2;
      const t = Math.max(1, Math.round(ms * this.rate / 1000));
      let v = (this.src[srcp] << 8) | this.src[srcp+1]; srcp += 2;
      v *= scale;
      env.pointv.push({ t, v });
    }
    env.pointp = 0;
    env.v = env.v0;
    if (pointc > 0) {
      env.ttl = env.pointv[0].t;
      env.dv = (env.pointv[0].v - env.v) / env.ttl;
    } else {
      env.ttl = 0x7fffffff;
      env.dv = 0;
    }
    return srcp;
  }
  
  _constantEnv(env, k) {
    env.v0 = k;
    env.v = k;
    env.ttl = 0x7fffffff;
    env.dv = 0;
    env.pointv = [];
    env.pointp = 0;
  }
  
  _updateEnv(env) {
    if (env.ttl-- > 0) {
      env.v += env.dv;
    } else {
      env.pointp++;
      if (env.pointp >= env.pointv.length) {
        env.pointp = env.pointv.length;
        env.dv = 0;
        env.ttl = 0x7fffffff;
        if (env.pointv.length) env.v = env.pointv[env.pointv.length-1].v;
        else env.v = env.v0;
      } else {
        env.v = env.pointv[env.pointp-1].v;
        env.ttl = env.pointv[env.pointp].t;
        env.dv = (env.pointv[env.pointp].v - env.v) / env.ttl;
      }
    }
    return env.v;
  }
  
  /* Wave generators.
   */
          
  _printSine(len) {
    const dst = new Float32Array(len);
    for (let i=0, p=0, dp=Math.PI*2/len; i<dst.length; i++, p+=dp) {
      dst[i] = Math.sin(p);
    }
    return dst;
  }
  
  _printSquare(len) {
    const dst = new Float32Array(len);
    const halflen = len >> 1;
    for (let i=halflen; i-->0; ) dst[i] = 1;
    for (let i=halflen; i<dst.length; i++) dst[i] = -1;
    return dst;
  }
  
  _printSawup(len) {
    const dst = new Float32Array(len);
    for (let i=0, p=-1, dp=2/len; i<dst.length; i++, p+=dp) {
      dst[i] = p;
    }
    return dst;
  }
  
  _printSawdown(len) {
    const dst = new Float32Array(len);
    for (let i=0, p=1, dp=-2/len; i<dst.length; i++, p+=dp) {
      dst[i] = p;
    }
    return dst;
  }
  
  _printTriangle(len) {
    const dst = new Float32Array(len);
    const halflen = len >> 1;
    for (let i=0, p=-1, dp=2/halflen; i<halflen; i++, p+=dp) {
      dst[i] = p;
    }
    for (let i=halflen, p=1, dp=-2/halflen; i<dst.length; i++, p+=dp) {
      dst[i] = p;
    }
    return dst;
  }
  
  _printHarmonic(dst, src, step, level) {
    if (level <= 0) return;
    for (let dstp=0, srcp=0; dstp<dst.length; dstp++, srcp+=step) {
      if (srcp >= src.length) srcp -= src.length;
      dst[dstp] += src[srcp] * level;
    }
  }
}
