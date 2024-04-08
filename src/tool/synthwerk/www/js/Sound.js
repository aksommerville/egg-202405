/* Sound.js
 * Model of a sound effect, split out for UI purposes.
 * We manage conversion to and from the text format.
 * Use SfgCompiler and SfgPrinter to get to the binary or wave format from there.
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
      if (i) dst += "endvoice\n";
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
      } else if (words[0] === "endvoice") {
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
  
  decodeBinary(src) {//XXX Not sure there's any value in this. Consider removing instead of implementing.
    throw new Error(`TODO: Sound.decodeBinary`);
  }
}

Sound.shapes = [ // indexed by encoded value, and strings are the proper text values -- don't pretty up for display
  "sine",
  "square",
  "sawup",
  "sawdown",
  "triangle",
  "noise",
  "silence",
];

Sound.opnames = [ // indexed by opcode, and strings are the proper text values
  "",
  "level",
  "gain",
  "clip",
  "delay",
  "bandpass",
  "notch",
  "lopass",
  "hipass",
];

/* Voice.
 ************************************************************************/

export class Voice {
  constructor() {
    this.clear();
  }
  
  static newWithDefaults() {
    const voice = new Voice();
    voice.ops.push(new Op(["level", "0", "50", "1", "80", "0.250", "250", "0"]));
    return voice;
  }
  
  clear() {
    this.shape = Sound.shapes[0];
    this.harmonics = []; // 0..1, up to 255
    this.fmRate = 0; // 0..256
    this.fmScale = 0; // 0..256
    this.fmEnv = null; // Env
    this.rate = null; // Env
    this.rateLfoRate = 0; // Hz, 0..256
    this.rateLfoDepth = 0; // Cents, 0..65535
    this.ops = [];
  }
  
  encodeText() {
    let dst = "";
    
    if (this.shape !== "sine") {
      dst += `shape ${this.shape}\n`;
    }
    
    if (this.shape === "silence") return dst;
    
    if (this.shape !== "noise") { // Noise oscillator doesn't need any further config.
    
      // Technically [1] is the noop harmonics, but we'll treat empty like that too.
      if (!this.harmonics.length) {
      } else if ((this.harmonics.length === 1) && (this.harmonics[0] === 1)) {
      } else {
        dst += `harmonics ${this.harmonics.join(' ')}\n`;
      }
      
      if (this.fmRate || this.fmScale) {
        dst += `fm ${this.fmRate} ${this.fmScale}\n`;
      }
      
      if (this.fmEnv) {
        dst += `fmenv ${this.fmEnv.encodeText()}\n`;
      }
      
      if (this.rate) {
        dst += `rate ${this.rate.encodeText()}\n`;
      }
      
      if (this.rateLfoRate || this.rateLfoDepth) {
        dst += `ratelfo ${this.rateLfoRate} ${this.rateLfoDepth}\n`;
      }
    }
    
    for (const op of this.ops) dst += op.encodeText();
    return dst;
  }
  
  decodeTextWords(words) {
    switch (words[0]) {
      case "shape": this.shape = words[1] || ""; break;
      case "harmonics": this.harmonics = words.slice(1).map(v => (+v || 0)); break;
      case "fm": this.fmRate = +words[1] || 0; this.fmScale = +words[2] || 0; break;
      case "fmenv": this.fmEnv = new Env(words.slice(1)); break;
      case "rate": this.rate = new Env(words.slice(1)); break;
      case "ratelfo": this.rateLfoRate = +words[1] || 0; this.rateLfoDepth = +words[2] || 0; break;
      default: this.ops.push(new Op(words));
    }
  }
}

/* Op.
 ******************************************************************************/

export class Op {
  constructor(words) {
    this.cmd = words[0] || "";
    if ((this.opcode = Sound.opnames.indexOf(this.cmd)) < 0) {
      throw new Error(`Unknown op name ${JSON.stringify(words[0])}`);
    }
    this.decodeText(words.slice(1));
  }
  
  decodeText(words) {
    switch (this.cmd) {
      case "level": this.env = new Env(words); break;
      case "gain": this.gain = +words[0] || 0; break;
      case "clip": this.clip = +words[0] || 0; break;
      case "delay": {
          this.period = +words[0] || 0;
          this.dry = +words[1] || 0;
          this.wet = +words[2] || 0;
          this.sto = +words[3] || 0;
          this.fbk = +words[4] || 0;
        } break;
      case "bandpass": this.freq = +words[0] || 0; this.width = +words[1] || 0; break;
      case "notch": this.freq = +words[0] || 0; this.width = +words[1] || 0; break;
      case "lopass": this.freq = +words[0] || 0; break;
      case "hipass": this.freq = +words[0] || 0; break;
    }
  }
  
  encodeText() {
    switch (this.cmd) {
      case "level": return `level ${this.env.encodeText()}\n`;
      case "gain": return `gain ${this.gain}\n`;
      case "clip": return `clip ${this.clip}\n`;
      case "delay": return `delay ${this.period} ${this.dry} ${this.wet} ${this.sto} ${this.fbk}\n`;
      case "bandpass": return `bandpass ${this.freq} ${this.width}\n`;
      case "notch": return `notch ${this.freq} ${this.width}\n`;
      case "lopass": return `lopass ${this.freq}\n`;
      case "hipass": return `hipass ${this.freq}\n`;
    }
    return "";
  }
}

/* Env.
 * Encoded, point times are in relative ms.
 * Live, we make them absolute.
 * Values live are taken verbatim from the text, various commands have their own ranges.
 ****************************************************************/
 
export class Env {
  constructor(words) {
    this.init = +words[0] || 0;
    this.points = [];
    for (let p=1, now=0; p<words.length; p+=2) {
      const delay = +words[p] || 0;
      const value = +words[p+1] || 0;
      now += delay;
      this.points.push({
        t: now,
        v: value,
      });
    }
  }
  
  encodeText() {
    let dst = this.init.toString();
    let now = 0;
    for (const { t, v } of this.points) {
      dst += " " + (t - now).toString();
      dst += " " + v.toString();
      now = t;
    }
    return dst;
  }
}
