/* SfgCompiler.js
 * Produce SFG binaries from text.
 * Our runtime doesn't need this; only binaries are stored in ROM files.
 * Our main tooling doesn't need it either; compilation uses the C API.
 * Only thing this implementation is for, is our live sound editor.
 */
 
export class SfgCompiler {
  constructor(src, refname, lineno0) {
    this.src = src;
    this.refname = refname || "";
    this.lineno0 = lineno0 || 0;
    this.dst = null; // Uint8Array, with head room
    this.dstc = 0;
  }
  
  /* Convenience to deal with both single and multi resource files.
   * Returns an object whose keys are the sound IDs, and values are Uint8Array compiled sounds.
   * Single-resource file should end up with a single key, the empty string.
   */
  compileAll() {
    const all = {};
    this.split((id, src, refname, lineno0) => {
      const bin = new SfgCompiler(src, refname, lineno0).compile();
      all[id] = bin;
    });
    return all;
  }
  
  /* For each sound in a multi-resource file, call:
   *   cb(id: string, src:string, refname:string, lineno0:int)
   * Return anything true to stop iteration, and we'll return the same (or you can throw).
   * Arguments should be used verbatim to create a new SfgCompiler for the proper compile.
   * If no fences are present but some text is, we'll trigger (cb) once with the entire input and empty id.
   */
  split(cb) {
    let blockp = 0;
    let blocklineno = 0;
    let blockid = "";
    for (
      let srcp=0, lineno=this.lineno0+1;
      srcp<this.src.length;
      lineno++
    ) {
      const srcp0 = srcp;
      let nlp = this.src.indexOf('\n', srcp);
      if (nlp < 0) nlp = this.src.length;
      const line = this.src.substring(srcp, nlp).split('#')[0].trim();
      srcp = nlp + 1;
      if (!line) continue;
      const words = line.split(/\s+/);
      
      // "end" ends the current block.
      if ((words.length === 1) && (words[0] === "end")) {
        if (!blockp) throw new Error(`${this.refname}:${lineno}: 'end' not in a sound block`);
        const block = this.src.substring(blockp, srcp0);
        const result = cb(blockid, block, this.refname, blocklineno);
        if (result) return result;
        blockp = 0;
        blocklineno = 0;
        blockid = "";
        continue;
      }
      
      // "sound ID" begins a new block.
      if ((words.length === 2) && (words[0] === "sound")) {
        if (blockp) throw new Error(`${this.refname}:${lineno}: Sound block on line ${blocklineno} was not closed.`);
        blockp = srcp;
        blocklineno = lineno;
        blockid = words[1];
        continue;
      }
      
      // Anything else, if we're not in a block, report the whole thing as one block and finish.
      if (!blockp) {
        return cb("", this.src, this.refname, this.lineno0);
      }
      
      // And anything elser is content for the block, carry on until we reach "end".
    }
    if (blockp) throw new Error(`${this.refname}:${blocklineno}: Unclosed sound block`);
  }
  
  /* If our source has no fences, ie it's a single sound effect,
   * compile and return the binary as a Uint8Array.
   */
  compile() {
    this.dst = new Uint8Array(256);
    this.dstc = 4; // Include placeholders for duration and master.
    this.dst[2] = 0x01; // Master default 1.0.
    let firstCommand = true;
    let featuresp = 0; // Zero if no voice started, otherwise position in (this.dst).
    let positional = false; // True if current voice has issued a positional command (ie no more feature commands allowed).
    for (
      let srcp=0, lineno=this.lineno0+1;
      srcp<this.src.length;
      lineno++
    ) {
      const srcp0 = srcp;
      let nlp = this.src.indexOf('\n', srcp);
      if (nlp < 0) nlp = this.src.length;
      const line = this.src.substring(srcp, nlp).split('#')[0].trim();
      srcp = nlp + 1;
      if (!line) continue;
      const words = line.split(/\s+/);
      switch (words[0]) {
      
        case "master": {
            if (!firstCommand) throw new Error(`${this.refname}:${lineno}: 'master' may only be the first command in a sound`);
            const v = Math.min(0xffff, Math.max(0, Math.round(+words[1] * 256.0)));
            this.dst[2] = v >> 8;
            this.dst[3] = v;
          } break;
          
        case "endvoice": {
            if (featuresp) this._outU8(0x00);
            featuresp = 0;
            positional = false;
          } break;
          
        default: {
            if (!featuresp) {
              featuresp = this.dstc;
              this._outU8(0x00); // placeholder features
            }
            const fbit = this._evalFeatureBit(words[0]);
            if (fbit) {
              if (positional) throw new Error(`${this.refname}:${lineno}: ${JSON.stringify(words[0])} must come before all positional commands.`);
              const features = this.dst[featuresp];
              if (fbit & features) throw new Error(`${this.refname}:${lineno}: ${JSON.stringify(words[0])} must not appear twice in the same voice.`);
              if (fbit < features) throw new Error(`${this.refname}:${lineno}: ${JSON.stringify(words[0])} not allowed here, must come earlier.`);
              this.dst[featuresp] |= fbit;
            } else {
              positional = true;
            }
            this.lineno = lineno;
            this._compileVoiceLine(words);
          }
          
      }
      firstCommand = false;
    }
    return new Uint8Array(this.dst.buffer, 0, this.dstc);
  }
  
  _evalFeatureBit(kw) {
    switch (kw) {
      case "shape": return 0x01;
      case "harmonics": return 0x02;
      case "fm": return 0x04;
      case "fmenv": return 0x08;
      case "rate": return 0x10;
      case "ratelfo": return 0x20;
    }
    return 0;
  }
  
  // hey andy: This is designed to match sfg_compile.c, try to keep them in sync futureward.
  _compileVoiceLine(words) {
    switch (words[0]) {
      case "shape": return this._cmEnum(words, -1, "sine", "square", "sawup", "sawdown", "triangle", "noise", "silence"); 
      case "harmonics": return this._cmFloatlist(words, -1, 0, 8);
      case "fm": return this._cmFixed(words, -1, "u8.8", "u8.8");
      case "fmenv": return this._cmEnv(words, -1, 16);
      case "rate": return this._cmEnv(words, -1, 0);
      case "ratelfo": return this._cmFixed(words, -1, "u8.8", "u16.0");
      case "level": return this._cmEnv(words, 0x01, 16);
      case "gain": return this._cmFixed(words, 0x02, "u8.8");
      case "clip": return this._cmFixed(words, 0x03, "u0.8");
      case "delay": return this._cmFixed(words, 0x04, "u16.0", "u0.8", "u0.8", "u0.8", "u0.8");
      case "bandpass": return this._cmFixed(words, 0x05, "u16.0", "u16.0");
      case "notch": return this._cmFixed(words, 0x06, "u16.0", "u16.0");
      case "lopass": return this._cmFixed(words, 0x07, "u16.0");
      case "hipass": return this._cmFixed(words, 0x08, "u16.0");
      default: throw new Error(`${this.refname}:${this.lineno}: Unknown keyword ${JSON.stringify(words[0])}`);
    }
  }
  
  /* Generic line formats...
   */
  
  _cmEnum(words, opcode, ...names) {
    if (opcode >= 0) this._outU8(opcode);
    if (words.length !== 2) throw new Error(`${this.refname}:${this.lineno}: ${JSON.stringify(words[0])} takes exactly one parameter`);
    const p = names.indexOf(words[1]);
    if (p < 0) throw new Error(`${this.refname}:${this.lineno}: ${JSON.stringify(words[1])} not in enum for ${JSON.stringify(words[0])}: ${JSON.stringify(names)}`);
    this._outU8(p);
  }
  
  _cmFloatlist(words, opcode, wholesize, fractsize) {
    if (opcode >= 0) this._outU8(opcode);
    const vc = words.length - 1;
    if (vc > 0xff) throw new Error(`${this.refname}:${this.lineno}: Too many values for ${JSON.stringify(words[0])}. ${vc}, limit 255`);
    this._outU8(vc);
    const totalsize = wholesize + fractsize;
    const limit = 1 << wholesize;
    let out;
    switch (totalsize) {
      case 8: out = v => this._outU8(Math.max(0, Math.min(0xff, v * (1 << fractsize)))); break;
      case 16: out = v => this._outU16(Math.max(0, Math.min(0xffff, v * (1 << fractsize)))); break;
      case 24: out = v => this._outU24(Math.max(0, Math.min(0xffffff, v * (1 << fractsize)))); break;
      case 32: out = v => this._outU32(Math.max(0, Math.min(0xffffffff, v * (1 << fractsize)))); break;
      default: throw new Error(`invalid floatlist params ${wholesize},${fractsize}`);
    }
    for (let i=1; i<words.length; i++) {
      const verbatim = +words[i];
      if (isNaN(verbatim) || (verbatim < 0) || (verbatim > limit)) {
        throw new Error(`${this.refname}:${this.lineno}: Expected float in 0..${limit}, found ${JSON.stringify(words[i])}`);
      }
      out(verbatim);
    }
  }
  
  _cmEnv(words, opcode, fractsize) {
    if (opcode >= 0) this._outU8(opcode);
    const scale = 1 << fractsize;
    const limit = 1 << (16 - fractsize);
    let wordsp = 1;
    const value = () => {
      const v = +words[wordsp];
      if (isNaN(v) || (v < 0) || (v > limit)) {
        throw new Error(`${this.refname}:${this.lineno}: Expected float in 0..${limit}, found ${JSON.stringify(words[wordsp])}`);
      }
      wordsp++;
      this._outU16(Math.max(0, Math.min(0xffff, v * scale)));
    };
    value();
    const cp = this.dstc;
    this._outU8(0); // point count placeholder
    let c = 0;
    let dur = 0;
    while (wordsp < words.length) {
      const ms = +words[wordsp];
      if (isNaN(ms) || (ms < 0) || (ms > 0xffff)) {
        throw new Error(`${this.refname}:${this.lineno}: Expected envelope delay in 0..65535 (ms), found ${JSON.stringify(words[wordsp])}`);
      }
      wordsp++;
      this._outU16(ms);
      dur += ms;
      value();
      c++;
    }
    if (c > 0xff) throw new Error(`${this.refname}:${this.lineno}: Too many envelope points: ${c}, limit 255`);
    this.dst[cp] = c;
    
    // "level" command, opcode 0x01, compare to encoded duration.
    if (opcode === 0x01) {
      const prevdur = (this.dst[0] << 8) | this.dst[1];
      if (dur > prevdur) {
        this.dst[0] = dur >> 8;
        this.dst[1] = dur;
      }
    }
  }
  
  _cmFixed(words, opcode, ...types) {
    if (opcode >= 0) this._outU8(opcode);
    if (words.length !== types.length + 1) {
      throw new Error(`${this.refname}:${this.lineno}: ${JSON.stringify(words[0])} takes exactly ${types.length} params, found ${words.length - 1}`);
    }
    for (let i=0; i<types.length; i++) {
      const [_, wholesize, fractsize] = types[i].match(/^u(\d+)\.(\d+)$/).map(v => +v); // everything we do is currently unsigned.
      const totalsize = wholesize + fractsize;
      const limit = 1 << wholesize;
      const verbatim = +words[1 + i];
      if (isNaN(verbatim) || (verbatim < 0) || (verbatim > limit)) {
        throw new Error(`${this.refname}:${this.lineno}: Expected float in 0..${limit}, found ${JSON.stringify(words[1 + i])}`);
      }
      switch (totalsize) {
        case 8: this._outU8(Math.max(0, Math.min(0xff, verbatim * (1 << fractsize)))); break;
        case 16: this._outU16(Math.max(0, Math.min(0xffff, verbatim * (1 << fractsize)))); break;
        case 24: this._outU24(Math.max(0, Math.min(0xffffff, verbatim * (1 << fractsize)))); break;
        case 32: this._outU32(Math.max(0, Math.min(0xffffffff, verbatim * (1 << fractsize)))); break;
      } 
    }
  }
  
  /* Raw output...
   */
  
  _outRequire(c) {
    if (c < 1) return;
    if (this.dstc <= this.dst.length - c) return;
    const na = this.dstc + c + 256;
    const nv = new Uint8Array(na);
    nv.set(this.dst);
    this.dst = nv;
  }
  
  _outU8(v) {
    this._outRequire(1);
    this.dst[this.dstc++] = v;
  }
  
  _outU16(v) {
    this._outRequire(2);
    this.dst[this.dstc++] = v >> 8;
    this.dst[this.dstc++] = v;
  }
  
  _outU24(v) {
    this._outRequire(3);
    this.dst[this.dstc++] = v >> 16;
    this.dst[this.dstc++] = v >> 8;
    this.dst[this.dstc++] = v;
  }
  
  _outU32(v) {
    this._outRequire(4);
    this.dst[this.dstc++] = v >> 24;
    this.dst[this.dstc++] = v >> 16;
    this.dst[this.dstc++] = v >> 8;
    this.dst[this.dstc++] = v;
  }
}
