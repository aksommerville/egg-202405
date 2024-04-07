/* Sound.js
 * Model of a sound effect, split out for UI purposes.
 * We manage conversion to and from both the text and binary formats.
 * In production, it always goes text=>bin=>wave. But for us, you can skip text.
 */
 
export class Sound {
  constructor() {
    this.clear();
  }
  
  clear() {
    this.master = 0.5;
    this.voices = []; // Voice
  }
  
  encodeText() {
    let dst = `master ${this.master}\n`;
    for (let i=0; i<this.voices.length; i++) {
      const voice = this.voices[i];
      if (i) dst += "voice\n";
      dst += voice.encodeText();
    }
    return dst;
  }
  
  decodeText(src) {
    this.clear();
    let voice = null;
    for (const line of src.split('\n')) {
      const words = line.split('#')[0].split(/\s+/).filter(v => v);
      if (!words.length) continue;
      if (words[0] === "master") {
        this.master = +words[1];
      } else if (words[0] === "voice") {
        voice = null;
      } else {
        if (!voice) {
          voice = new Voice();
          this.voices.push(voice);
        }
        voice.decodeTextWords(words);
      }
    }
  }
  
  encodeBinary() {
    throw new Error(`TODO: Sound.encodeBinary`);
  }
  
  decodeBinary(src) {
    throw new Error(`TODO: Sound.decodeBinary`);
  }
  
  /* Binary format does not store a master level, instead it gets baked into level envelopes at compile time.
   * (and it applies more than once if there's multiple level envelopes; that's a quirk that I'm not going to fix).
   * 
   */
  inferMasterFromVoices() {
    this.master = 1;
  }
}

export class Voice {
  constructor() {
    this.clear();
  }
  
  clear() {
    this.waveMode = "sine"; // sine square sawup sawdown triangle noise silence harmonics fm
    this.fmRate = 1; // Multiplier; waveMode=="fm"
    this.fmScale = 1; // waveMode=="fm"
    this.harmonics = [1]; // 0..1; waveMode=="harmonics"
    this.rate = new HzEnv(440); // waveMode!="noise"
    this.rateLfo = null; // CentsLfo; waveMode!="noise"
    this.fmRange = null; // Env; waveMode=="fm"
    this.fmLfo = null; // Lfo; waveMode=="fm"
    this.ops = []; // Op
  }
  
  encodeText() {
    let dst = "";
    switch (this.waveMode) {
      case "silence": return "";
      case "noise": {
          dst = "wave noise\n";
        } break;
      case "harmonics": {
          dst = `wave ${this.harmonics.join(' ')}\n`;
          dst += this.encodeTextRate();
        } break;
      case "fm": {
          dst = `wave fm=${this.fmRate},${this.fmScale}\n`;
          dst += this.encodeTextFm();
          dst += this.encodeTextRate();
        } break;
      default: {
          dst = `wave ${this.waveMode}\n`;
          dst += this.encodeTextRate();
        } break;
    }
    for (const op of this.ops) {
      dst += op.encodeText();
    }
    return dst;
  }
  
  encodeTextRate() {
    let dst = `rate ${this.rate.encodeText()}\n`;
    if (this.rateLfo) dst += `ratelfo ${this.rateLfo.encodeText()}\n`;
    return dst;
  }
  
  encodeTextFm() {
    let dst = "";
    if (this.fmRange) dst += `fmrange ${this.fmRange.encodeText()}\n`;
    if (this.fmLfo) dst += `fmlfo ${this.fmLfo.encodeText()}\n`;
    return dst;
  }
  
  decodeTextWords(words) {
    switch (words[0]) {
    
      case "wave": {
          if (!words[1]) throw new Error(`Expected coefficients, 'fm=RATE,SCALE', or a shape name, after 'wave'`);
          if (words[1].startsWith("fm=")) {
            this.waveMode = "fm";
            const nv = words[1].substring(3).split(',').map(v => +v);
            this.fmRate = nv[0] || 0;
            this.fmScale = nv[1] || 0;
          } else if (words[1].match(/^\d/)) {
            this.waveMode = "harmonics";
            this.harmonics = words.slice(1).map(v => +v);
          } else {
            this.waveMode = words[1];
          }
        } break;
      case "rate": this.rate.decodeTextWords(words.slice(1)); break;
      case "ratelfo": {
          this.rateLfo = new CentsLfo();
          this.rateLfo.decodeTextWords(words.slice(1));
        } break;
      case "fmrange": {
          this.fmRange = new Env();
          this.fmRange.decodeTextWords(words.slice(1));
        } break;
      case "fmlfo": {
          this.fmLfo = new Lfo();
          this.fmLfo.decodeTextWords(words.slice(1));
        } break;
        
      case "level": {
          const op = new LevelOp();
          op.decodeTextWords(words.slice(1));
          this.ops.push(op);
        } break;
      case "gain": {
          const op = new ScalarOp(words[0], 8, 8);
          op.decodeTextWords(words.slice(1));
          this.ops.push(op);
        } break;
      case "clip": {
          const op = new ScalarOp(words[0], 0, 8);
          op.decodeTextWords(words.slice(1));
          this.ops.push(op);
        } break;
      case "delay": {
          const op = new DelayOp();
          op.decodeTextWords(words.slice(1));
          this.ops.push(op);
        } break;
      case "bandpass":
      case "notch": {
          const op = new FilterOp(words[0], 2);
          op.decodeTextWords(words.slice(1));
          this.ops.push(op);
        } break;
      case "lopass":
      case "hipass": {
          const op = new FilterOp(words[0], 1);
          op.decodeTextWords(words.slice(1));
          this.ops.push(op);
        } break;
      
      default: throw new Error(`Unexpected sound command ${JSON.stringify(words[0])}`);
    }
  }
}

/* Ops.
 ******************************************************************************/

export class Op {
  encodeText() {
    return "";
  }
  decodeTextWords(src) {
    throw new Error(`decodeTextWords not implemented for this op: ${JSON.stringify(this)}`);
  }
}

export class LevelOp extends Op {
  constructor() {
    super();
    this.env = new Env();
  }
  
  encodeText() {
    return `level ${this.env.encodeText()}\n`;
  }
  
  decodeTextWords(words) {
    this.env.decodeTextWords(words);
  }
}

// gain, clip
export class ScalarOp extends Op {
  constructor(opname, wholeSize, fractSize) {
    super();
    this.opname = opname;
    this.wholeSize = wholeSize;
    this.fractSize = fractSize;
    this.v = 0;
  }
  
  encodeText() {
    return `${this.opname} ${this.v}\n`;
  }
  
  decodeTextWords(words) {
    this.v = +words[0] || 0;
  }
}

export class DelayOp extends Op {
  constructor() {
    super();
    this.period = 250; // ms
    this.dry = 0.5;
    this.wet = 0.5;
    this.sto = 0.5;
    this.fbk = 0.5;
  }
  
  encodeText() {
    return `delay ${this.period} ${this.dry} ${this.wet} ${this.sto} ${this.fbk}\n`;
  }
  
  decodeTextWords(words) {
    this.period = +words[0] || 0;
    this.dry = +words[1] || 0;
    this.wet = +words[2] || 0;
    this.sto = +words[3] || 0;
    this.fbk = +words[4] || 0;
  }
}

// bandpass, notch, lopass, hipass
export class FilterOp extends Op {
  constructor(opname, argc) {
    super();
    this.opname = opname;
    this.argv = [];
    for (let i=argc; i-->0; ) this.argv.push(0);
  }
  
  encodeText() {
    return `${this.opname} ${this.argv.join(' ')}\n`;
  }
  
  decodeTextWords(words) {
    for (let i=0; i<this.argv.length; i++) {
      this.argv[i] = +words[i] || 0;
    }
  }
}

/* Primitives.
 ***************************************************************************/
 
export class Env {
  constructor(fractSize=16) {
    this.fractSize = fractSize;
    this.value0 = 0; // nominal range (0..1 for fractSize 16; 0..65535 for fractSize 0; etc)
    this.points = []; // { t:abs ms, v:nominal }
  }
  
  encodeText() {
    let dst = this.value0.toString();
    let now = 0;
    for (const { t, v } of this.points) {
      dst += ` ${t - now} ${v}`;
      now = t;
    }
    return dst;
  }
  
  decodeTextWords(words) {
    this.value0 = +words[0] || 0;
    this.points = [];
    for (let wordsp=1, now=0; wordsp<words.length; ) {
      const delay = +words[wordsp++] || 1;
      const t = now + delay;
      now = t;
      const v = +words[wordsp++] || 0;
      this.points.push({ t, v });
    }
  }
}

export class HzEnv extends Env {
  constructor() {
    super(0);
    this.value0 = 440;
  }
}

export class Lfo {
  constructor(fractSize=8) {
    this.fractSize = fractSize;
    this.rate = 1; // u8.8 hz
    this.depth = 0; // 16 bits, depends on fractSize
  }
  
  encodeText() {
    return `${this.rate} ${this.depth / (1 << this.fractSize)}`;
  }
  
  decodeTextWords(words) {
    this.rate = +words[0] || 0;
    this.depth = +words[1] || 0;
  }
}

export class CentsLfo extends Lfo {
  constructor() {
    super(0);
  }
}
