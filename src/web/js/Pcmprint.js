/* Pcmprint.js
 *
 * This is much less efficient than its C counterpart, and it's not just a C-vs-Javascript thing.
 * In the C implementation, we spread printing out over time, generating samples only at the moment before they're first needed.
 * With WebAudio, we don't get that frame-by-frame view of main output, so we can't do something similar.
 * Instead we print the entire sound effect in one shot.
 * 
 * On top of that, I've skipped plenty of opportunities for optimization, erring on the side of legibility over performance.
 * (since performance is going to suck no matter what).
 *
 * The upshot of all that, is that this implementation is much easier to read than the C one.
 * Prefer this implementation as a reference.
 */
 
export class Pcmprint {

  /* src: Uint8Array
   * rate: integer
   * Returns null or Float32Array.
   */
  static print(src, rate) {
    if ((rate < 200) || (rate > 200000)) return null;
    if (!src || (src.length < 2)) return null;
    const durationMs = (src[0] << 8) | src[1];
    if (!durationMs) return null;
    const durationFrames = Math.ceil((durationMs * rate) / 1000);
    const dst = new Float32Array(durationFrames);
    const scratch = new Float32Array(durationFrames);
    try {
      for (let srcp=2; srcp<src.length; ) {
        const np = this.printVoice(scratch, src, srcp, rate);
        if (np <= srcp) throw new Error(`Printing stalled at ${srcp}/${src.length}`);
        srcp = np;
        for (let i=durationFrames; i-->0; ) dst[i] += scratch[i];
      }
    } catch (error) {
      console.error(`Failed to decode sound.`, { error, src, rate, durationMs });
      return null;
    }
    return dst;
  }
  
  /* Decode one voice from serial and write it to (dst).
   * Must overwrite (dst) completely.
   * Returns new (srcp).
   */
  static printVoice(dst, src, srcp, rate) {
    // Consume redundant VOICE commands.
    while ((srcp < src.length) && (src[srcp] === 0x01)) srcp++;
    
    // Decode config.
    const cfg = { mainRate: rate };
    const np = this.decodeVoice(cfg, src, srcp, rate);
    if (np <= srcp) throw new Error(`Failed to decode sound voice at ${srcp}/${src.length}`);
    
    // Run oscillator. Overwrites (dst).
    if ("shape" in cfg) {
      switch (cfg.shape) {
        case 0: this.oscillateShape(dst, cfg, p => Math.sin(p)); break;
        case 1: this.oscillateShape(dst, cfg, p => (p < 0.5) ? 1 : -1); break;
        case 2: this.oscillateShape(dst, cfg, p => p * 2 - 1); break;
        case 3: this.oscillateShape(dst, cfg, p => 1 - p * 2); break;
        case 4: this.oscillateShape(dst, cfg, p => (p < 0.5) ? (p * 4 - 1) : (1 - (p - 0.5) * 4)); break;
        default: throw new Error(`Unknown pcmprint shape ${cfg.shape}`);
      }
    } else if (cfg.harmonics) {
      this.oscillateHarmonics(dst, cfg);
    } else if (cfg.fmRate && cfg.fmRange) {
      this.oscillateFm(dst, cfg);
    } else if (cfg.noise) {
      for (let i=dst.length; i-->0; ) dst[i] = Math.random() * 2 - 1;
    } else {
      // No oscillator configure, emit silence.
      // No sense proceeding to ops in this case; they can't unsilence silence.
      for (let i=dst.length; i-->0; ) dst[i] = 0;
      return np;
    }
    
    // Run post ops.
    for (const op of cfg.ops) switch (op.name) {
      case "LEVEL": this.applyLevel(dst, op); break;
      case "GAIN": this.applyGain(dst, op); break;
      case "CLIP": this.applyClip(dst, op); break;
      case "DELAY": this.applyDelay(dst, op); break;
      case "FILTER": this.applyFilter(dst, op); break;
      default: throw new Error(`Unknown pcmprint op ${JSON.stringify(op.name)}`);
    }
    return np;
  }
  
  /* Run oscillator with a wave function.
   */
  static oscillateShape(dst, cfg, fn) {
    let p = 0;
    let rateLfo = this.beginLfo(cfg.rateLfoRate, cfg.rateLfoDepth, true);
    if (!rateLfo) rateLfo = this.constantLfo(true);
    for (let i=0; i<dst.length; i++) {
      dst[i] = fn(p);
      p += this.updateEnv(cfg.rate) * rateLfo();
      if (p >= 1) p -= 1;
    }
  }
  
  /* Run oscillator with harmonics.
   */
  static oscillateHarmonics(dst, cfg) {
    const wave = new Float32Array(1024);
    for (let i=0; i<cfg.harmonics.length; i++) {
      if (!cfg.harmonics[i]) continue;
      this.addHarmonic(wave, i + 1, cfg.harmonics[i]);
    }
    let p = 0;
    let rateLfo = this.beginLfo(cfg.rateLfoRate, cfg.rateLfoDepth, true);
    if (!rateLfo) rateLfo = this.constantLfo(true);
    for (let i=0; i<dst.length; i++) {
      dst[i] = wave[Math.round(p * wave.length) % wave.length];
      p += this.updateEnv(cfg.rate) * rateLfo();
      if (p >= 1) p -= 1;
    }
  }
  static addHarmonic(dst, step, level) {
    step = (step * Math.PI * 2) / dst.length;
    for (let dstp=0, p=0; dstp<dst.length; dstp++, p+=step) {
      if (p >= Math.PI) p -= Math.PI * 2;
      dst[dstp] += Math.sin(p) * level;
    }
  }
  
  /* Run oscillator with FM.
   */
  static oscillateFm(dst, cfg) {
    let rateLfo = this.beginLfo(cfg.rateLfoRate, cfg.rateLfoDepth, true);
    if (!rateLfo) rateLfo = this.constantLfo(true);
    let rangeLfo = this.beginLfo(cfg.rangeLfoRate, cfg.rangeLfoRange, false);
    if (!rangeLfo) rangeLfo = this.constantLfo(false);
    this.scaleEnv(cfg.rate, Math.PI * 2);
    if (cfg.range) this.scaleEnv(cfg.range, cfg.fmRange);
    else cfg.range = this.constantEnv(cfg.fmRange);
    let carp = 0;
    let modp = 0;
    for (let dstp=0; dstp<dst.length; dstp++) {
      const baseRate = this.updateEnv(cfg.rate) * rateLfo();
      dst[dstp] = Math.sin(carp);
      let mod = Math.sin(modp) + rangeLfo();
      modp += baseRate * cfg.fmRate;
      mod *= baseRate * this.updateEnv(cfg.range);
      carp += baseRate + mod;
      if (modp >= Math.PI) modp -= Math.PI * 2;
      if (carp >= Math.PI) carp -= Math.PI * 2;
    }
  }
  
  static applyLevel(dst, op) {
    for (let i=0; i<dst.length; i++) {
      dst[i] *= this.updateEnv(op.env);
    }
  }
  
  static applyGain(dst, op) {
    for (let i=0; i<dst.length; i++) {
      dst[i] *= op.gain;
    }
  }
  
  static applyClip(dst, op) {
    const hi = op.clip, lo = -op.clip;
    for (let i=0; i<dst.length; i++) {
      if (dst[i] > hi) dst[i] = hi;
      else if (dst[i] < lo) dst[i] = lo;
    }
  }
  
  static applyDelay(dst, op) {
    const buf = new Float32Array(op.framec);
    let bufp = 0;
    for (let i=0; i<dst.length; i++) {
      const next = dst[i];
      const prev = buf[bufp];
      buf[bufp] = next * op.sto + prev * op.fbk;
      dst[i] = next * op.dry + prev * op.wet;
      bufp++;
      if (bufp >= buf.length) bufp = 0;
    }
  }
  
  static applyFilter(dst, op) {
    const state = [0, 0, 0, 0, 0];
    for (let i=0; i<dst.length; i++) {
      state[2] = state[1];
      state[1] = state[0];
      state[0] = dst[i];
      dst[i] = (
        state[0] * op.coefv[0] +
        state[1] * op.coefv[1] +
        state[2] * op.coefv[2] +
        state[3] * op.coefv[3] +
        state[4] * op.coefv[4]
      );
      state[4] = state[3];
      state[3] = dst[i];
    }
  }
  
  /* Decode voice into (cfg), initially a blank object.
   * Returns new (srcp).
   */
  static decodeVoice(cfg, src, srcp, rate) {
    cfg.ops = [];
    while ((srcp < src.length) && (src[srcp] !== 0x01)) {
      const opcode = src[srcp++];
      switch (opcode) {
      
        // One-time commands.
        case 0x02: cfg.shape = src[srcp++]; break;
        case 0x03: cfg.noise = true; break;
        case 0x04: { // HARMONICS
            if (srcp >= src.length) throw new Error(`Unexpected EOF`);
            const coefc = src[srcp++];
            if (srcp > src.length - coefc) throw new Error(`Unexpected EOF`);
            cfg.harmonics = src.slice(srcp, srcp + coefc);
            srcp += coefc;
          } break;
        case 0x05: { // FM
            if (srcp > src.length - 4) throw new Error(`Unexpected EOF`);
            cfg.fmRate = src[srcp] + src[srcp + 1] / 256.0;
            srcp += 2;
            cfg.fmRange = src[srcp] + src[srcp + 1] / 256.0;
            srcp += 2;
          } break;
        case 0x06: { // RATE
            if (srcp > src.length - 3) throw new Error(`Unexpected EOF`);
            const cmdlen = 3 + src[srcp + 2] * 4;
            if (srcp > src.length - cmdlen) throw new Error(`Unexpected EOF`);
            cfg.rate = this.decodeEnv(src, srcp, cmdlen, 1 / rate, rate);
            srcp += cmdlen;
          } break;
        case 0x07: { // RATELFO
            if (srcp > src.length - 4) throw new Error(`Unexpected EOF`);
            const hz = src[srcp] + src[srcp + 1] / 256.0;
            srcp += 2;
            cfg.rateLfoRate = hz / rate
            cfg.rateLfoDepth = (src[srcp] << 8) | src[srcp + 1];
            srcp += 2;
          } break;
        case 0x08: { // RANGEENV
            if (srcp > src.length - 3) throw new Error(`Unexpected EOF`);
            const cmdlen = 3 + src[srcp + 2] * 4;
            if (srcp > src.length - cmdlen) throw new Error(`Unexpected EOF`);
            cfg.range = this.decodeEnv(src, srcp, cmdlen, 1 / 65536.0, rate);
            srcp += cmdlen;
          } break;
        case 0x09: { // RANGELFO
            if (srcp > src.length - 4) throw new Error(`Unexpected EOF`);
            const hz = src[srcp] + src[srcp + 1] / 256.0;
            srcp += 2;
            cfg.rangeLfoRate = hz / rate;
            cfg.rangeLfoDepth = src[srcp] = src[srcp + 1] / 256.0;
            srcp += 2;
          } break;
          
        // Multi stages.
        case 0x0a: { // LEVEL
            if (srcp > src.length - 3) throw new Error(`Unexpected EOF`);
            const cmdlen = 3 + src[srcp + 2] * 4;
            if (srcp > src.length - cmdlen) throw new Error(`Unexpected EOF`);
            cfg.ops.push({
              name: "LEVEL",
              env: this.decodeEnv(src, srcp, cmdlen, 1 / 65536.0, rate),
            });
            srcp += cmdlen;
          } break;
        case 0x0b: { // GAIN
            cfg.ops.push({
              name: "GAIN",
              gain: src[srcp] + src[srcp + 1] / 256.0,
            });
            srcp += 2;
          } break;
        case 0x0c: { // CLIP
            cfg.ops.push({
              name: "CLIP",
              clip: src[srcp++] / 255.0,
            });
          } break;
        case 0x0d: { // DELAY
            if (srcp > src.length - 6) throw new Error(`Unexpected EOF`);
            const ms = (src[srcp] << 8) | src[srcp + 1];
            srcp += 2;
            const framec = Math.round((ms * rate) / 1000);
            if (framec > 0) {
              cfg.ops.push({
                name: "DELAY",
                framec,
                dry: src[srcp++] / 255.0,
                wet: src[srcp++] / 255.0,
                sto: src[srcp++] / 255.0,
                fbk: src[srcp++] / 255.0,
              });
            } else {
              srcp += 4;
            }
          } break;
        case 0x0e: { // BANDPASS
            let mid = (src[srcp] << 8) | src[srcp + 1];
            let wid = (src[srcp + 2] << 8) | src[srcp + 3];
            mid /= rate;
            wid /= rate;
            cfg.ops.push({
              name: "FILTER",
              coefv: this.iirBandpass(mid, wid),
            });
          } break;
        case 0x0f: { // NOTCH
            let mid = (src[srcp] << 8) | src[srcp + 1];
            let wid = (src[srcp + 2] << 8) | src[srcp + 3];
            mid /= rate;
            wid /= rate;
            cfg.ops.push({
              name: "FILTER",
              coefv: this.iirNotch(mid, wid),
            });
          } break;
        case 0x10: { // LOPASS
            let freq = (src[srcp] << 8) | src[srcp + 1];
            freq /= rate;
            cfg.ops.push({
              name: "FILTER",
              coefv: this.iirLopass(freq),
            });
          } break;
        case 0x11: { // HIPASS
            let freq = (src[srcp] << 8) | src[srcp + 1];
            freq /= rate;
            cfg.ops.push({
              name: "FILTER",
              coefv: this.iirHipass(freq),
            });
          } break;
          
        default: throw new Error(`Unknown pcmprint opcode ${opcode}`);
      }
    }
    return srcp;
  }
  
  static decodeEnv(src, srcp, srcc, norm, rate) {
    const env = {
      points: [],
    };
    env.value0 = ((src[srcp] << 8) | src[srcp + 1]) * norm;
    srcp += 2;
    const pointc = src[srcp++];
    for (let i=0; i<pointc; i++) {
      const ms = (src[srcp] << 8) | src[srcp + 1];
      srcp += 2;
      const value = ((src[srcp] << 8) | src[srcp + 1]) * norm;
      srcp += 2;
      const framec = Math.max(1, Math.round((ms * rate) / 1000));
      env.points.push({ framec, value });
    }
    this.resetEnv(env);
    return env;
  }
  
  static constantEnv(v) {
    const env = { points: [], value0: v };
    this.resetEnv(env);
    return env;
  }
  
  static resetEnv(env) {
    env.pointp = 0;
    env.v = env.value0;
    if (env.points.length > 0) {
      env.ttl = env.points[0].framec;
      env.dv = (env.points[0].value - env.v) / env.ttl;
    } else {
      env.ttl = 0x7fffffff;
      env.dv = 0;
    }
  }
  
  static scaleEnv(env, mlt) {
    env.value0 *= mlt;
    for (const point of env.points) point.value *= mlt;
    this.resetEnv(env);
  }
  
  static updateEnv(env) {
    if (env.ttl < 1) {
      env.pointp++;
      if (env.pointp >= env.points.length) {
        env.v = env.points.length ? env.points[env.points.length - 1].value : env.value0;
        env.dv = 0;
        env.ttl = 0x7fffffff;
      } else {
        env.v = env.points[env.pointp - 1].value;
        env.ttl = env.points[env.pointp].framec;
        env.dv = (env.points[env.pointp].value - env.v) / env.ttl;
      }
    }
    env.ttl--;
    env.v += env.dv;
    return env.v;
  }
  
  // Returns a self-contained function or null if not needed.
  // If null, call constantLfo to fake it.
  static beginLfo(rate, depth, isCents) {
    if (!rate || !depth) return null;
    let p = 0;
    const dp = rate * Math.PI * 2;
    if (isCents) {
      return () => {
        p += dp;
        if (p >= Math.PI) p -= Math.Pi * 2;
        return 2 ** ((Math.sin(p) * depth) / 1200);
      };
    }
    return () => {
      p += dp;
      if (p >= Math.PI) p -= Math.PI * 2;
      return Math.sin(p) * depth;
    };
  }
  
  static constantLfo(isCents) {
    if (isCents) return () => 1;
    return () => 0;
  }
  
  static iirBandpass(mid, wid) {
    const r = 1 - 3 * wid;
    const cosfreq = Math.cos(Math.PI * 2 * mid);
    const k = (1 - 2 * r * cosfreq + r * r) / (2 - 2 * cosfreq);
    return [
      1 - k,
      2 * (k - r) * cosfreq,
      r * r - k,
      2 * r * cosfreq,
      -r * r,
    ];
  }
  
  static iirNotch(mid, wid) {
    const r = 1 - 3 * wid;
    const cosfreq = Math.cos(Math.PI * 2 * mid);
    const k = (1 - 2 * r * cosfreq + r * r) / (2 - 2 * cosfreq);
    return [
      k,
      -2 * k * cosfreq,
      k,
      2 * r * cosfreq,
      -r * r,
    ];
  }
  
  static iirLopass(freq) {
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
    return [
      (x0 - x1 * k + x2 * k * k) / d,
      (-2 * x * k + x1 + x1 * k * k - 2 * x2 * k) / d,
      (x0 * k * k - x1 * k + x2) / d,
      (2 * k + y1 + y1 * k * k - 2 * y2 * k) / d,
      (-k * k - y1 * k + y2) / d,
    ];
  }
  
  static iirHipass(freq) {
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
    return [
      -(x0 - x1 * k + x2 * k * k) / d,
      (-2 * x0 * k + x1 + x1 * k * k - 2 * x2 * k) / d,
      (x0 * k * k - x1 * k + x2) / d,
      -(2 * k + y1 + y1 * k * k - 2 * y2 * k) / d,
      (-k * k - y1 * k + y2) / d,
    ];
  }
}
