#!/usr/bin/env node
const fs = require("fs");

if (process.argv.length !== 4) {
  throw new Error(`Usage: ${process.argv[1]} INPUT OUTPUT`);
}
const srcpath = process.argv[2];
const dstpath = process.argv[3];

function lineno(src, srcp) {
  let nlc = 1;
  for (let p=0; p<srcp; p++) {
    if (src[p] === 0x0a) nlc++;
  }
  return nlc;
}

function readToken(src, srcp) {
  const srcp0 = srcp;
  
  // Whitespace.
  if (src[srcp] <= 0x20) {
    srcp++;
    while ((srcp < src.length) && (src[srcp] <= 0x20)) srcp++;
    return src.toString("utf8", srcp0, srcp);
  }
  
  // Anything else outside G0 is illegal.
  if (src[srcp] >= 0x7f) {
    throw new Error(`Unexpected byte ${src[srcp]}`);
  }
  
  // '#' or '//', read thru the next newline.
  if ((src[srcp] === 0x23) || ((src[srcp] === 0x2f) && (src[srcp+1] === 0x2f))) {
    srcp++;
    while ((srcp < src.length) && (src[srcp++] !== 0x0a)) ;
    return src.toString("utf8", srcp0, srcp);
  }
  
  // Block comments.
  if ((src[srcp] === 0x2f) && (src[srcp + 1] === 0x2a)) {
    srcp += 2;
    for (;;) {
      if (srcp > src.length - 2) throw new Error(`Unclosed block comment.`);
      if ((src[srcp] === 0x2a) && (src[srcp + 1] === 0x2f)) {
        return src.toString("utf8", srcp0, srcp + 2);
      }
      srcp++;
    }
  }
  
  // Letters, digits, and underscores stick together.
  const isident = (ch) => (
    ((ch >= 0x30) && (ch <= 0x39)) ||
    ((ch >= 0x41) && (ch <= 0x5a)) ||
    ((ch >= 0x61) && (ch <= 0x7a)) ||
    (ch === 0x5f)
  );
  if (isident(src[srcp])) {
    srcp++;
    while ((srcp < src.length) && isident(src[srcp])) srcp++;
    return src.toString("utf8", srcp0, srcp);
  }
  
  return src.toString("utf8", srcp0, srcp+1);
}

const src = fs.readFileSync(srcpath);

const instruments = [];
let started = false; // Throw away everything until we see "synth_builtin".
let ins = null; // Null at outer scope, otherwise an object collecting one instrument.
let nest = 0; // Track open and close parens in an instrument block.
let key = "";
let substate = 0;
for (let srcp=0; srcp<src.length; ) {
  let srcp0 = srcp;
  try {
    const token = readToken(src, srcp);
    srcp += token.length;
    if (token.match(/\s/)) continue; // Anything containing space is either space, comment, or preprocessor directive: Not interesting.
    if (token.startsWith("/*")) continue; // And block comments don't necessarily contain a space.
    if (!started) { // Doesn't matter much, but throw away everything until we see the main initializer.
      if (token === "synth_builtin") started = true;
      continue;
    }
    
    // If no instrument is in progress, look for our macros.
    if (!ins) {
      switch (token) {
        case "BLIP":
        case "WAVE":
        case "ROCK":
        case "FMREL":
        case "FMABS":
        case "SUB":
        case "FX":
        case "ALIAS": {
            ins = { macro: token, srcp: srcp0 };
            nest = 0;
            key = "";
          } break;
      }
      continue;
    }
    
    // Track open and close parens within the block. When nest reaches zero, the block is done.
    // We don't need the parens for anything else, either.
    if (token === "(") {
      nest++;
      continue;
    }
    if (token === ")") {
      nest--;
      if (nest <= 0) {
        if (typeof(ins.pid) !== "number") {
          srcp0 = ins.srcp;
          throw new Error(`Failed to acquire pid for instrument.`);
        }
        if (key) {
          throw new Error(`Instrument block ended while awaiting value for ${JSON.stringify(key)}`);
        }
        instruments[ins.pid] = ins;
        ins = null;
      }
      continue;
    }
    
    // There's a bunch of punctuation we can discard too. We're not going to enforce much grammar.
    if ((token === ".") || (token === ",") || (token === "=") || (token === "|")) {
      continue;
    }
    
    // If we haven't read pid yet, it should be the second token after the macro.
    // (There's an open paren before it, which would be caught already).
    if (!("pid" in ins)) {
      ins.pid = +token;
      if (isNaN(ins.pid) || (ins.pid < 0) || (ins.pid >= 0x80)) {
        throw new Error(`Expected pid in 0..127, found ${JSON.stringify(token)}`);
      }
      continue;
    }
    
    // "ALIAS" is unlike other macros, it takes just one unlabelled parameter after the pid.
    if (ins.macro === "ALIAS") {
      if ("other" in ins) throw new Error(`Unexpected token ${JSON.stringify(token)} in ALIAS block.`);
      const other = +token;
      if (isNaN(other) || (other < 0) || (other >= 0x80) || (other === ins.pid)) {
        throw new Error(`Expected alias pid in 0..127, found ${JSON.stringify(token)}`);
      }
      ins.other = other;
      continue;
    }
    
    // Awaiting a key, it must be a C identifier beginning with a lowercase letter.
    if (!key) {
      if (!token.match(/^[a-z][a-zA-Z0-9_]*$/)) {
        throw new Error(`Expected ')' or instrument key, found ${JSON.stringify(token)}`);
      }
      key = token;
      switch (key) {
        case "wave": ins.wave = []; break;
        case "level": ins.level = 0; substate = 0; break;
      }
      continue;
    }
    
    // "wave" is a little special. Content is "{", int, int, ..., "}"
    if (key === "wave") {
      if (token === "{") continue;
      if (token === "}") {
        key = "";
        continue;
      }
      const coef = +token;
      if (isNaN(coef) || (coef < 0) || (coef > 0xff)) {
        throw new Error(`Expected '}' or integer in 0..255 for wave coefficient, found ${JSON.stringify(token)}`);
      }
      ins.wave.push(coef);
      continue;
    }
    
    // "level" is also weird: .level=PLUCK|ATTACK(1)|RELEASE(2),
    // Which turns into: "level" "PLUCK" "ATTACK" "1" "RELEASE" "2"
    // Order of those three subfields is not important in C, but we're going to assume they appear in this order.
    if (key === "level") {
      switch (substate) {
        case 0: { // expecting shape
            if (token === "IMPULSE") ;
            else if (token === "PLUCK") ins.level |= 0x40;
            else if (token === "TONE") ins.level |= 0x80;
            else if (token === "BOW") ins.level |= 0xc0;
            else throw new Error(`Expected 'IMPULSE', 'PLUCK', 'TONE', or 'BOW' after 'level'. Found ${JSON.stringify(token)}`);
            substate = 1;
          } break;
        case 1: {
            if (token !== "ATTACK") throw new Error(`Expected 'ATTACK', found ${JSON.stringify(token)}`);
            substate = 2;
          } break;
        case 2: {
            const v = +token;
            if (isNaN(v) || (v < 0) || (v > 7)) {
              throw new Error(`Expected integer in 0..7 for attack time, found ${JSON.stringify(token)}`);
            }
            ins.level |= v << 3;
            substate = 3;
          } break;
        case 3: {
            if (token !== "RELEASE") throw new Error(`Expected 'RELEASE', found ${JSON.stringify(token)}`);
            substate = 4;
          } break;
        case 4: {
            const v = +token;
            if (isNaN(v) || (v < 0) || (v > 7)) {
              throw new Error(`Expected integer in 0..7 for release time, found ${JSON.stringify(token)}`);
            }
            ins.level |= v;
            key = "";
          } break;
      }
      continue;
    }
    
    // Every other key is an integer and we'll stuff it blindly into the instrument.
    const v = +token;
    if (isNaN(v)) throw new Error(`Expected integer for ${JSON.stringify(key)}, found ${JSON.stringify(token)}`);
    ins[key] = v;
    key = "";

  } catch (e) {
    console.log(`${srcpath}:${lineno(src, srcp0)}: ${e.message}`);
    process.exit(1);
  }
}

if (ins) {
  console.log(`${srcpath}:${lineno(src, ins.srcp)}: Unclosed instrument block.`);
  process.exit(1);
}

/**
let insc = 0;
for (let i=0; i<0x80; i++) {
  if (instruments[i]) insc++;
}
console.log(`Found ${insc} instruments.`);
console.log(instruments);
/**/

/* Verify that all ALIAS instruments point to something that is not ALIAS.
 */
for (const ins of instruments) {
  if (!ins) continue;
  if (ins.macro !== "ALIAS") continue;
  const other = instruments[ins.other];
  if (!other || (other.macro === "ALIAS")) {
    console.log(`${srcpath}:${lineno(src, ins.srcp)}: ALIAS block must refer to a valid non-ALIAS block.`);
    process.exit(1);
  }
}

// Tons of other validation we could do, but not sure it's worth much.

function bandpassQFromHz(hz) {
  const q = 100 - hz * 100 / 300;
  if (q <= 0) return 0;
  if (q >= 100) return 100;
  return q;
}

/* Produce the Javascript text.
 */
let dst = `// Do not edit! Generated from ${srcpath}.\nexport const Instruments = [\n`;
for (let pid=0; pid<0x80; pid++) {
  const ins = instruments[pid];
  
  // Anything missing becomes null = noop.
  if (!ins) {
    dst += "null,\n";
    
  // Aliases emit as an integer.
  } else if (ins.macro === "ALIAS") {
    dst += `${ins.other},\n`;
    
  // Modes with no parameters emit as strings. Only "BLIP" right now, and I don't expect others in the future.
  } else if (ins.macro === "BLIP") {
    dst += `"${ins.macro.toLowerCase()}",\n`;
    
  // Everything else emits as an object.
  } else {
    const dstobj = {
      mode: ins.macro.toLowerCase(),
    };
    for (const k of Object.keys(ins)) {
      switch (k) {
        // Strip a few fields:
        case "macro":
        case "srcp":
        case "pid":
          break;
      
        // Most fields have minor adjustment to key or format:
        case "wave": {
            dstobj.wave = [0]; // On the JS side, they start with DC.
            for (const i of ins.wave) dstobj.wave.push(i / 255.0);
          } break;
        case "level": dstobj.levelTiny = ins.level; break;
        case "rate": { // u4.4 for FMREL and FX; u8.8 for FMABS. Outputs float in all cases.
            if (ins.macros === "FMABS") dstobj.fmRate = ins.rate / 256.0;
            else dstobj.fmRate = ins.rate / 16.0;
          } break;
        case "scale": dstobj.fmRangeScale = ins.scale / 16.0; break;
        case "range": 
        case "rangeenv": dstobj.fmRangeEnv = ins[k]; break;
        case "width1": dstobj.subQ1 = bandpassQFromHz(ins[k]); break;
        case "width2": dstobj.subQ2 = bandpassQFromHz(ins[k]); break;
        case "gain": dstobj.subGain = ins.gain; break;
        case "rangelfo": dstobj.fmRangeLfo = ins.rangelfo / 16.0; break;
        case "rangelfo_depth": dstobj.fmRangeLfoDepth = ins.rangelfo_depth / 16.0; break;
        case "detune_rate": dstobj.detuneRate = ins.detune_rate / 16.0; break;
        case "detune_depth": dstobj.detuneDepth = ins.detune_depth / 255.0; break;
        case "overdrive": dstobj.overdrive = ins.overdrive / 255.0; break;
        case "delay_rate": dstobj.delayRate = ins.delay_rate / 16.0; break;
        case "delay_depth": dstobj.delayDepth = ins.delay_depth / 255.0; break;
        
        // And a few lucky fields transfer verbatim:
        case "mix":
            dstobj[k] = ins[k];
            break;
      }
    }
    dst += JSON.stringify(dstobj) + ",\n";
  }
}
dst += "];\n";

fs.writeFileSync(dstpath, dst);
