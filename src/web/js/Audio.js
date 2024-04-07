/* Audio.js
 * Implements our synthesizer and exposes the public API.
 */

import { Rom } from "./Rom.js"; 
import { SfgPrinter } from "./SfgPrinter.js";
import { Instruments } from "./Instruments.js";
 
export class Audio {
  constructor(window, rom) {
    this.window = window;
    this.rom = rom;
    
    this.rate = 44100; // TODO configurable?
    this.context = null;
    this.song = null;
    this.channels = [
      null, null, null, null,
      null, null, null, null,
      null, null, null, null,
      null, null, null, null,
    ];
    this.voices = [];
    this.sounds = {}; // key:"QUAL.RID", value:AudioBuffer
    this.soundEffects = [];
    this.noise = null; // AudioBuffer, null until we need it.
    
    this.hzByNoteid = [];
    for (let noteid=0; noteid<0x80; noteid++) {
      this.hzByNoteid.push(440 * Math.pow(2, (noteid - 69) / 12));
    }
  }
  
  start() {
    if (!this.window.AudioContext) return;
    this.context = new this.window.AudioContext({
      sampleRate: this.rate,
      latencyHint: "interactive",
    });
    if (this.context.state === "suspended") {
      this.context.resume();
    }
    return Promise.resolve();
  }
  
  stop() {
    this.endSong();
    //TODO
  }
  
  update() {
    for (let i=this.voices.length; i-->0; ) {
      const voice = this.voices[i];
      if (!voice.isFinished()) continue;
      voice.terminate();
      this.voices.splice(i, 1);
    }
    if (this.song) {
      this.song.update();
    }
  }
  
  /* Public API.
   *****************************************************************/
  
  audio_play_song(qual, songid, force, repeat) {
    if (!force && this.song && this.song.isResource(qual, songid)) return;
    this.endSong();
    const serial = this.rom.getResource(Rom.TID_song, qual, songid);
    if (!serial || !serial.length) return;
    try {
      this.song = new Song(serial, this, repeat);
    } catch (e) {
      console.error(`Failed to play song:${qual}:${songid}.`, e);
      return;
    }
    this.beginSong();
  }
  
  audio_play_sound(qual, soundid, trim, pan, when) {
    if (!this.context) return;
    if (!when) when = this.context.currentTime;
    const pcm = this.acquireSound(qual, soundid);
    if (!pcm) return;
    const node = new AudioBufferSourceNode(this.context, {
      buffer: pcm,
      channelCount: 1,
    });
    const gain = new GainNode(this.context, { gain: trim });
    node.connect(gain);
    gain.connect(this.context.destination);
    node.start(when);
    this.soundEffects.push(node);
    node.onended = () => {
      const p = this.soundEffects.indexOf(node);
      if (p >= 0) this.soundEffects.splice(p, 1);
    };
  }
  
  audio_get_playhead() {
    return -1;//TODO
  }
  
  /* Internals.
   ******************************************************************/
   
  endSong() {
    if (!this.song) return;
    this.song = null;
    for (const voice of this.voices) voice.release();
    for (let chid=0; chid<8; chid++) {
      if (this.channels[chid]) {
        this.channels[chid].stop();
        this.channels[chid] = null;
      }
    }
    //TODO Cancel scheduled notes.
  }
  
  beginSong() {
    if (!this.song) return;
    for (let i=0; i<8; i++) {
      this.channels[i] = null;
      const src = this.song.channels[i];
      if (!src.volume) continue;
      this.channels[i] = new Channel(this, src.pid, src.volume, src.pan);
    }
  }
  
  acquireSound(qual, rid) {
    if (!this.context) return null;
    const key = `${qual}.${rid}`;
    if (key in this.sounds) return this.sounds[key];
    let v = null;
    const serial = this.rom.getResource(Rom.TID_sound, qual, rid);
    if (serial) {
      const fv = new SfgPrinter(serial, this.rate).print();
      if (fv && fv.length) {
        v = new AudioBuffer({
          length: fv.length,
          numberOfChannels: 1,
          sampleRate: this.rate,
          channelCount: 1,
        });
        v.copyToChannel(fv, 0);
      }
    }
    this.sounds[key] = v;
    return v;
  }
  
  // (velocity) in 0..127 like MIDI. (when) in AudioContext time.
  playNote(chid, noteid, velocity, durs, when) {
    const channel = this.channels[chid];
    if (!channel) return;
    channel.playNote(this, noteid, velocity / 127.0, durs, when);
  }
  
  // (v) in 0..0x3fff like MIDI. (when) in AudioContext time.
  changeWheel(chid, v, when) {
    const channel = this.channels[chid];
    if (!channel) return;
    if (!channel.wheelRange) return;
    if (v === channel.wheel) return;
    channel.wheel = v;
    channel.wheelCents = ((v - 0x2000) * channel.wheelRange) / 0x2000;
    //TODO Apply to in-flight voices. Must respect (when) too!
  }
  
  requireNoise() {
    if (this.noise) return;
    const fv = new Float32Array(this.rate);
    for (let i=fv.length; i-->0; ) fv[i] = Math.random() * 2 - 1;
    this.noise = new AudioBuffer({
      length: fv.length,
      numberOfChannels: 1,
      sampleRate: this.rate,
      channelCount: 1,
    });
    this.noise.copyToChannel(fv, 0);
  }
}

/* Song.
 **********************************************************************/
 
const SONG_READAHEAD_WINDOW_S = 0.500;
 
class Song {
  constructor(src, audio, repeat) {
    this.audio = audio;
    this.repeat = repeat;
    if (!(src instanceof Uint8Array)) throw new Error(`Expected Uint8Array`);
    if (src.length < 42) throw new Error(`Invalid song`);
    if ((src[0] !== 0xbe) || (src[1] !== 0xee) || (src[2] !== 0xee) || (src[3] !== 0x50)) throw new Error(`Invalid song`);
    this.msperqnote = (src[4] << 8) | src[5];
    this.startp = (src[6] << 8) | src[7];
    this.loopp = (src[8] << 8) | src[9];
    if ((this.startp < 42) || (this.loopp < this.startp) || (this.loopp >= src.length)) throw new Error(`Invalid song`);
    this.src = src;
    this.channels = [];
    for (let srcp=10; srcp<42; srcp+=4) {
      this.channels.push({
        pid: src[srcp],
        volume: src[srcp+1],
        pan: src[srcp+2],
        rsv: src[srcp+3],
      });
    }
    this.startTime = audio.context.currentTime;
    this.readp = this.startp;
    this.readTime = this.startTime;
  }
  
  update() {
    const now = this.audio.context.currentTime;
    const later = now + SONG_READAHEAD_WINDOW_S;
    while (this.readTime < later) {
      const event = this.readEvent();
      
      // End of song?
      if (!event) {
        this.audio.endSong();
        return;
      }
      
      // Delay?
      if (typeof(event) === "number") {
        this.readTime += event;
        continue;
      }
      
      // Anything else, readEvent() dispatched it. Carry on.
    }
  }
  
  /* Advance readp and return one event:
   *  - null: EOF and not repeating.
   *  - number: Delay, seconds. Never zero.
   *  - "ok": Processed one event (we dispatch it from here).
   */
  readEvent() {
    const lead = this.src[this.readp++];
    
    // Zero or end of input is End of Song.
    if (!lead) {
      if (!this.repeat) return null;
      this.readp = this.loopp;
      // Must delay a little, in case the song has no explicit delays, so we don't loop forever.
      // Note that if this happens, it's a disaster no matter what.
      return 0.010;
    }
    
    // High bit unset is a delay in ms.
    if (!(lead & 0x80)) {
      return lead / 1000;
    }

    // 1000vvvv cccnnnnn nntttttt : NOTE. duration=(t<<5)ms (~2s max)
    if ((lead & 0xf0) === 0x80) {
      const a = this.src[this.readp++] || 0;
      const b = this.src[this.readp++] || 0;
      let velocity = (lead & 0x0f) << 3;
      velocity |= velocity >> 4;
      const chid = a >> 5;
      const noteid = ((a & 0x1f) << 2) | (b >> 6);
      const durms = ((b & 0x3f) << 5);
      this.audio.playNote(chid, noteid, velocity, durms / 1000, this.readTime);
      return "ok";
    }
    
    // 1001vvcc cnnnnnnn : FIREFORGET. Same as NOTE but duration zero (and coarser velocity).
    if ((lead & 0xf0) === 0x90) {
      const a = this.src[this.readp++] || 0;
      let velocity = ((lead & 0x0c) << 2);
      velocity |= velocity >> 2;
      velocity |= velocity >> 4;
      const chid = ((lead & 0x03) << 1) | (a >> 7);
      const noteid = (a & 0x7f);
      this.audio.playNote(chid, noteid, velocity, 0, this.readTime);
      return "ok";
    }
    
    // 10100ccc wwwwwwww : WHEEL. 8 bits unsigned. 0x40 by default.
    if ((lead & 0xf8) === 0xa0) {
      const a = this.src[this.readp++] || 0;
      const chid = lead & 0x07;
      const v = (a << 6) | (a >> 2);
      this.audio.changeWheel(chid, v, this.readTime);
      return "ok";
    }
    
    // Anything else is reserved and illegal. End the song.
    console.log(`Illegal song command ${lead}.`);
    return null;
  }
}

/* Channel.
 *********************************************************************/
 
class Channel {
  constructor(audio, pid, volume, pan) {
    this.audio = audio;
    this.pid = pid;
    this.volume = volume / 255.0;
    this.master = 0.250;
    this.pan = (pan - 0x80) / 128.0;
    this.mode = "noop";
    this.wheelRange = 200; // cents
    this.wheel = 0; // Last value, 0..0x3fff
    this.wheelCents = 0;
    
    if (this.pid < 0x00) this._initNoop();
    else if (this.pid < 0x80) this._initBuiltin(audio);
    else if (this.pid < 0x100) this._initDrum();
    else this._initNoop();
  }
  
  _initNoop() {
    this.mode = "noop";
    this.wheelRange = 0;
  }
  
  _initDrum() {
    this.mode = "drum";
    this.wheelRange = 0;
    this.drumBase = (this.pid - 0x80) * 0x80;
  }
  
  _initBuiltin(audio) {
    let cfg = Instruments[this.pid];
    if (typeof(cfg) === "number") {
      cfg = Instruments[cfg];
    }
    if (!cfg) return this._initNoop();
    if (typeof(cfg) === "string") {
      this.mode = cfg;
      return;
    }
    for (const k of Object.keys(cfg)) {
      this[k] = cfg[k];
    }
    if (this.wave) {
      this.wave = new PeriodicWave(audio.context, { real: this.wave });
    }
    if (this.mode === "fx") {
      this.fxBegin(audio);
    }
  }
  
  // (velocity) normalized
  playNote(audio, noteid, velocity, durs, when) {
    switch (this.mode) {
      case "noop": break;
      
      case "drum": {
          const soundid = this.drumBase + noteid;
          const trim = 0.200 + (this.volume * this.master * velocity) * 0.900;
          audio.audio_play_sound(0, soundid, trim, this.pan, when);
        } break;
        
      case "blip": {
          const attackTime = 0.010;
          const releaseTime = 0.050;
          const level = this.volume * this.master * (velocity + 0.079) * 0.400;
          const voice = new Voice(audio);
          voice.oscillateShape("square", audio.hzByNoteid[noteid], this.wheelCents);
          voice.plateauLevel(when, attackTime, level, durs, releaseTime);
          voice.begin();
        } break;
        
      case "wave": {
          const voice = new Voice(audio);
          voice.oscillateWave(this.wave, audio.hzByNoteid[noteid], this.wheelCents);
          voice.tinyEnv(when, this.levelTiny, durs, velocity, this.volume * this.master);
          voice.begin();
        } break;
        
      case "rock": {
          const voice = new Voice(audio);
          voice.oscillateMix(this.wave, this.mix, audio.hzByNoteid[noteid], this.wheelCents);
          voice.tinyEnv(when, this.levelTiny, durs, velocity, this.volume * this.master);
          voice.begin();
        } break;
        
      case "fmrel": {
          const voice = new Voice(audio);
          voice.oscillateFmRelative(audio.hzByNoteid[noteid], this.wheelCents, this.fmRate, this.fmRangeScale, this.fmRangeEnv);
          voice.tinyEnv(when, this.levelTiny, durs, velocity, this.volume * this.master);
          voice.begin();
        } break;
        
      case "fmabs": {
          const voice = new Voice(audio);
          voice.oscillateFmAbsolute(audio.hzByNoteid[noteid], this.wheelCents, this.fmRate, this.fmRangeScale, this.fmRangeEnv);
          voice.tinyEnv(when, this.levelTiny, durs, velocity, this.volume * this.master);
          voice.begin();
        } break;
        
      case "sub": {
          const voice = new Voice(audio);
          voice.oscillateSubtractive(audio.hzByNoteid[noteid], this.wheelCents, this.subQ1, this.subQ2, this.subGain);
          voice.tinyEnv(when, this.levelTiny, durs, velocity, this.volume * this.master);
          voice.begin();
        } break;
        
      case "fx": {
          this.fxNote(audio, when, noteid, velocity, durs);
        } break;
    }
  }
  
  stop() {
    switch (this.mode) {
      case "fx": this.fxStop(); break;
    }
  }
  
  fxBegin(audio) {
    this.audio = audio;
    this.fxMaster = new GainNode(audio.context);
    this.fxMaster.gain.setValueAtTime(this.volume * this.master, 0);
    this.fxMaster.connect(audio.context.destination);
    this.fxVoices = [];
    this.fxAttach = this.fxMaster;
    
    //TODO detune. Can't do it in post like the C implementation, but I think we can vary the voice's frequencies.
    
    if ((this.fmRangeLfo > 0) && (this.fmRangeLfoDepth > 0)) {
      let beatRate = 2;
      if (this.audio.song && (this.audio.song.msperqnote > 0)) {
        beatRate = 1000 / this.audio.song.msperqnote;
      }
      const osc = new OscillatorNode(this.audio.context, {
        type: "sine",
        frequency: beatRate / this.fmRangeLfo,
      });
      const gain = new GainNode(this.audio.context, {
        gain: this.fmRangeLfoDepth * 100, // TODO No idea why this *100 is needed.
      });
      osc.connect(gain);
      osc.start();
      this.fmLfo = osc;
      this.fmLfoOut = gain;
    }
    
    if ((this.delayRate > 0) && (this.delayDepth > 0) && this.audio.song && (this.audio.song.msperqnote > 0)) {
      const wetLevel = this.delayDepth * 0.500;
      const dryLevel = 1 - wetLevel;
      const period = (this.delayRate * this.audio.song.msperqnote) / 1000;
      const delay = new DelayNode(this.audio.context, { delayTime: period });
      
      const intake = new GainNode(this.audio.context, { gain: 1 });
      const output = new GainNode(this.audio.context, { gain: 1 });
      const dryGain = new GainNode(this.audio.context, { gain: dryLevel });
      const wetGain = new GainNode(this.audio.context, { gain: wetLevel });
      intake.connect(dryGain);
      dryGain.connect(output);
      intake.connect(delay);
      delay.connect(wetGain);
      wetGain.connect(delay);
      wetGain.connect(output);
      output.connect(this.fxAttach);
      this.fxAttach = intake;
    }
    
    if (this.overdrive > 0) {
      const len = 99;
      const odrange = 8;
      const midp = len >> 1;
      const odscaled = 0.5 + this.overdrive * (odrange - 0.5);
      const odcurved = 1 / odscaled; // Now in (1/odrange)..2
      const odnormed = (odcurved - 1 / odrange) / (2 - 1 / odrange);
      const ramplen = midp * odnormed;
      const vv = new Float32Array(len);
      for (let i=0; i<ramplen; i++) {
        vv[midp + i + 1] = Math.sin((i * Math.PI / 2) / ramplen);
      }
      for (let i=midp+ramplen+1; i<len; i++) {
        vv[i] = 1;
      }
      for (let dst=midp, src=midp+1; dst-->0; src++) {
        vv[dst] = -vv[src];
      }
      const shaper = new WaveShaperNode(this.audio.context, {
        curve: vv,
      });
      // Also some attenuation, after the wave-shape, since we're raising its average level.
      if (this.overdrive >= 0.25) {
        const drop = new GainNode(this.audio.context, {
          gain: 1 - (this.overdrive - 0.25) * 0.8,
        });
        shaper.connect(drop);
        drop.connect(this.fxAttach);
      } else {
        shaper.connect(this.fxAttach);
      }
      this.fxAttach = shaper;
    }
  }
  
  fxStop() {
    if (this.fxMaster) {
      this.fxMaster.disconnect();
      this.fxMaster = null;
    }
    if (this.fmLfo) {
      this.fmLfo.stop();
      this.fmLfo.disconnect();
      this.fmLfo = null;
    }
    if (this.fmLfoOut) {
      this.fmLfoOut.disconnect();
      this.fmLfoOut = null;
    }
  }
  
  fxNote(audio, when, noteid, velocity, durs) {
    const voice = new Voice(this.audio);
    voice.oscillateFmRelative(this.audio.hzByNoteid[noteid], this.wheelCents, this.fmRate, this.fmRangeScale, this.fmRangeEnv, this.fmLfoOut);
    voice.tinyEnv(when, this.levelTiny, durs, velocity, 1);
    voice.begin(this.fxAttach);
  }
}

/* Voice.
 *******************************************************************/
 
class Voice {
  constructor(audio) {
    this.audio = audio;
    this.osc = null;
    this.env = null;
    this.endTime = 0;
  }
  
  isFinished() {
    if (!this.audio || !this.audio.context || !this.env) return true;
    return (this.audio.context.currentTime > this.endTime);
  }
  
  terminate() {
    if (this.env) {
      this.env.disconnect();
      this.env = null;
    }
    if (this.osc) {
      if (this.osc.stop) this.osc.stop();
      this.osc.disconnect();
      this.osc = null;
    }
    if (this.oscDry) {
      this.oscDry.stop();
      this.oscDry.disconnect();
      this.oscDry = null;
    }
    if (this.oscWet) {
      this.oscWet.stop();
      this.oscWet.disconnect();
      this.oscWet = null;
    }
    if (this.modosc) {
      this.modosc.stop();
      this.modosc.disconnect();
      this.modosc = null;
    }
    if (this.noiseNode) {
      this.noiseNode.stop();
      this.noiseNode.disconnect();
      this.noiseNode = null;
    }
    if (this.post) {
      this.post.disconnect();
      this.post = null;
    }
  }
  
  release() {
  }
  
  begin(dst) {
    if (!this.audio || !this.env) return;
    if (!dst) dst = this.audio.context.destination;
    if (this.osc) {
      this.osc.connect(this.env);
      if (this.osc.start) this.osc.start();
    } else if (this.gainDry) {
      this.gainDry.connect(this.env);
      this.gainWet.connect(this.env);
      this.oscDry.start();
      this.oscWet.start();
    } else {
      return;
    }
    if (this.post) this.post.connect(dst);
    else this.env.connect(dst);
    this.audio.voices.push(this);
  }
  
  oscillateShape(type, frequency, detune) {
    this.osc = new OscillatorNode(this.audio.context, {
      type,
      frequency,
      detune,
    });
  }
  
  oscillateWave(periodicWave, frequency, detune) {
    this.osc = new OscillatorNode(this.audio.context, {
      periodicWave,
      frequency,
      detune,
    });
  }
  
  // (mix) is 4 levels, 4 bits each, big-endian. eg 0xf842 = [1.0, 0.5, 0.25, 0.125]
  oscillateMix(periodicWave, mix, frequency, detune) {
    this.oscDry = new OscillatorNode(this.audio.context, {
      type: "sine",
      frequency,
      detune,
    });
    this.oscWet = new OscillatorNode(this.audio.context, {
      periodicWave,
      frequency,
      detune,
    });
    this.gainDry = new GainNode(this.audio.context);
    this.gainWet = new GainNode(this.audio.context);
    this.oscDry.connect(this.gainDry);
    this.oscWet.connect(this.gainWet);
    this.mix = mix;
  }
  
  oscillateFmRelative(frequency, detune, rate, scale, env, lfo) {
    this.osc = new OscillatorNode(this.audio.context, {
      type: "sine",
      frequency,
      detune,
    });
    this.modosc = new OscillatorNode(this.audio.context, {
      type: "sine",
      frequency: frequency * rate * Math.pow(2, detune / 1200),
    });
    this.modgain = new GainNode(this.audio.context);
    if (lfo) {
      this.fmGainLfo = lfo;
      //const rangeCombine = new GainNode(this.audio.context, { gain: 20 });
      //this.fmGainLfo.connect(rangeCombine);
      this.fmGainLfo.connect(this.modgain.gain);
      this.modgain.gain.setValueAtTime(1, 0);
      //rangeCombine.connect(this.modosc.frequency);
      //this.modgain.connect(rangeCombine);
      //rangeCombine.connect(this.osc.frequency);
      this.modgain.connect(this.osc.frequency);
    } else {
      this.modgain.connect(this.osc.frequency);
    }
    this.modgainPeak = scale * frequency;
    this.fmRangeEnv = env;
    this.modosc.connect(this.modgain);
    this.modosc.start();
  }
  
  oscillateFmAbsolute(frequency, detune, rate, scale, env) {
    this.osc = new OscillatorNode(this.audio.context, {
      type: "sine",
      frequency,
      detune,
    });
    this.modosc = new OscillatorNode(this.audio.context, {
      type: "sine",
      frequency: rate,
    });
    this.modgain = new GainNode(this.audio.context);
    this.modgainPeak = scale * frequency;
    this.fmRangeEnv = env;
    this.modgain.connect(this.osc.frequency);
    this.modosc.connect(this.modgain);
    this.modosc.start();
  }
  
  oscillateSubtractive(frequency, detune, q1, q2, gain) {
    this.audio.requireNoise();
    this.noiseNode = new AudioBufferSourceNode(this.audio.context, {
      buffer: this.audio.noise,
      channelCount: 1,
      loop: true,
      loopStart: 0,
      loopEnd: this.audio.noise.duration,
    });
    const filter1 = new BiquadFilterNode(this.audio.context, {
      type: "bandpass",
      Q: q1,
      frequency,
    });
    const filter2 = new BiquadFilterNode(this.audio.context, {
      type: "bandpass",
      Q: q2,
      frequency,
    });
    const gainNode = new GainNode(this.audio.context);
    gainNode.gain.setValueAtTime(gain, 0);
    this.noiseNode.connect(filter1);
    filter1.connect(filter2);
    filter2.connect(gainNode);
    this.osc = gainNode;
    this.noiseNode.start();
  }
  
  plateauLevel(when, attackTimeRel, peakLevel, sustainTimeRel, releaseTimeRel) {
    this.endTime = when + attackTimeRel + sustainTimeRel + releaseTimeRel;
    this.env = new GainNode(this.audio.context);
    this.env.gain.setValueAtTime(0, 0);
    this.env.gain.setValueAtTime(0, when);
    this.env.gain.linearRampToValueAtTime(peakLevel, when + attackTimeRel);
    this.env.gain.setValueAtTime(peakLevel, when + attackTimeRel + sustainTimeRel);
    this.env.gain.linearRampToValueAtTime(0, this.endTime);
  }
  
  /* A format I use in the C implementation for single-byte level envelopes:
   *   0xc0 Decay relative to attack:
   *         0x00 IMPULSE: Do not sustain.
   *         0x40 PLUCK: Heavy loss after attack.
   *         0x80 TONE: Attack noticeably louder than sustain.
   *         0xc0 BOW: No appreciable attack.
   *   0x38 Attack time: 0x00=Fast .. 0x38=Slow
   *   0x07 Release time: 0x00=Short .. 0x07=Long
   */
  tinyEnv(when, v, durs, velocity, trim) {
    let attackTimeHi;
    switch (v & 0x38) {
      case 0x00: attackTimeHi = 0.005; break;
      case 0x08: attackTimeHi = 0.008; break;
      case 0x10: attackTimeHi = 0.012; break;
      case 0x18: attackTimeHi = 0.018; break;
      case 0x20: attackTimeHi = 0.030; break;
      case 0x28: attackTimeHi = 0.045; break;
      case 0x30: attackTimeHi = 0.060; break;
      case 0x38: attackTimeHi = 0.080; break;
    }
    let releaseTimeHi;
    switch (v & 0x07) {
      case 0x00: releaseTimeHi = 0.040; break;
      case 0x01: releaseTimeHi = 0.060; break;
      case 0x02: releaseTimeHi = 0.100; break;
      case 0x03: releaseTimeHi = 0.200; break;
      case 0x04: releaseTimeHi = 0.400; break;
      case 0x05: releaseTimeHi = 0.600; break;
      case 0x06: releaseTimeHi = 0.800; break;
      case 0x07: releaseTimeHi = 1.200; break;
    }
    let sustain = true;
    let attackValueHi = 1;
    let sustainValueHi;
    switch (v & 0xc0) {
      case 0x00: { // IMPULSE
          sustain = false;
          sustainValueHi = 0.250;
        } break;
      case 0x40: { // PLUCK
          sustainValueHi = 0.200;
        } break;
      case 0x80: { // TONE
          attackValueHi = 0.750;
          sustainValueHi = 0.400;
        } break;
      case 0xc0: { // BOW
          attackValueHi = 0.400;
          sustainValueHi = 0.400;
        } break;
    }
    let decayTimeHi = (attackTimeHi * 3) / 2;
    const attackTimeLo = attackTimeHi * 2;
    const decayTimeLo = decayTimeHi * 2;
    const releaseTimeLo = releaseTimeHi * 0.5;
    const attackValueLo = attackValueHi * 0.333;
    const sustainValueLo = sustainValueHi * 0.500;
    let a, b;
    if (velocity <= 0) { a=1; b=0; }
    else if (velocity >= 1) { a=0; b=1; }
    else { a = 1 - velocity; b = velocity; }
    const attackTime = attackTimeLo * a + attackTimeHi * b;
    const attackValue = (attackValueLo * a + attackValueHi * b) * trim;
    const decayTime = decayTimeLo * a + decayTimeHi * b;
    const sustainValue = (sustainValueLo * a + sustainValueHi * b) * trim;
    if (!sustain) durs = 0;
    const releaseTime = releaseTimeLo * a + releaseTimeHi * b;
    this.endTime = when + attackTime + decayTime + durs + releaseTime;
    
    this.env = new GainNode(this.audio.context);
    this.env.gain.setValueAtTime(0, 0);
    this.env.gain.setValueAtTime(0, when);
    this.env.gain.linearRampToValueAtTime(attackValue, when + attackTime);
    this.env.gain.linearRampToValueAtTime(sustainValue, when + attackTime + decayTime);
    this.env.gain.setValueAtTime(sustainValue, when + attackTime + decayTime + durs);
    this.env.gain.linearRampToValueAtTime(0, this.endTime);
    
    if (this.gainDry && this.gainWet) {
      const v0 = (this.mix >> 12) / 15.0;
      const v1 = ((this.mix >> 8) & 15) / 15.0;
      const v2 = ((this.mix >> 4) & 15) / 15.0;
      const v3 = (this.mix & 15) / 15.0;
      this.gainDry.gain.setValueAtTime(0, 0);
      this.gainDry.gain.setValueAtTime(1 - v0, when);
      this.gainDry.gain.linearRampToValueAtTime(1 - v1, when + attackTime);
      this.gainDry.gain.linearRampToValueAtTime(1 - v2, when + attackTime + decayTime);
      this.gainDry.gain.linearRampToValueAtTime(1 - v2, when + attackTime + decayTime + durs);
      this.gainDry.gain.linearRampToValueAtTime(1 - v3, this.endTime);
      this.gainWet.gain.setValueAtTime(0, 0);
      this.gainWet.gain.setValueAtTime(v0, when);
      this.gainWet.gain.linearRampToValueAtTime(v1, when + attackTime);
      this.gainWet.gain.linearRampToValueAtTime(v2, when + attackTime + decayTime);
      this.gainWet.gain.linearRampToValueAtTime(v2, when + attackTime + decayTime + durs);
      this.gainWet.gain.linearRampToValueAtTime(v3, this.endTime);
    }
    
    if (this.fmRangeEnv && !this.fmRangeLfo) {
      if (this.fmRangeEnv === 0xffff) {
        this.modgain.gain.setValueAtTime(this.modgainPeak, 0);
      } else {
        const v0hi = (this.modgainPeak * (this.fmRangeEnv >> 12)) / 15.0;
        const v1hi = (this.modgainPeak * ((this.fmRangeEnv >> 8) & 15)) / 15.0;
        const v2hi = (this.modgainPeak * ((this.fmRangeEnv >> 4) & 15)) / 15.0;
        const v3hi = (this.modgainPeak * (this.fmRangeEnv & 15)) / 15.0;
        const avg = (v0hi + v1hi + v2hi + v3hi) / 4;
        const v0lo = (v0hi + avg) / 2;
        const v1lo = (v1hi + avg) / 2;
        const v2lo = (v2hi + avg) / 2;
        const v3lo = (v3hi + avg) / 2;
        const v0 = v0lo * (1 - velocity) + v0hi * velocity;
        const v1 = v1lo * (1 - velocity) + v1hi * velocity;
        const v2 = v2lo * (1 - velocity) + v2hi * velocity;
        const v3 = v3lo * (1 - velocity) + v3hi * velocity;
        this.modgain.gain.setValueAtTime(v0, 0);
        this.modgain.gain.setValueAtTime(v0, when);
        this.modgain.gain.linearRampToValueAtTime(v1, when + attackTime);
        this.modgain.gain.linearRampToValueAtTime(v2, when + attackTime + decayTime);
        this.modgain.gain.setValueAtTime(v2, when + attackTime + decayTime + durs);
        this.modgain.gain.linearRampToValueAtTime(v3, this.endTime);
      }
    }
  }
}
