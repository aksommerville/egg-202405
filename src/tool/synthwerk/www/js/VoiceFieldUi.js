/* VoiceFieldUi.js
 * Single row in VoiceUi, corresponding roughly to one line of the text.
 */
 
import { Dom } from "./Dom.js";
import { Bus } from "./Bus.js";
import { Sound, Env } from "./Sound.js";
import { HarmonicsUi } from "./HarmonicsUi.js";
import { EnvUi } from "./EnvUi.js";

export class VoiceFieldUi {
  static getDependencies() {
    return [HTMLElement, Dom, Bus, "discriminator"];
  }
  constructor(element, dom, bus, discriminator) {
    this.element = element;
    this.dom = dom;
    this.bus = bus;
    this.discriminator = discriminator;
    
    this.requestDeleteVoice = (voice) => {};
    this.requestAddOp = (voice) => {};
    this.requestDeleteOp = (voice, op) => {};
    this.requestOpMotion = (voice, op, dp) => {};
    this.dirty = () => {};
    
    this.voice = null;
    this.key = "";
    this.op = null;
    this.lowerPanel = null; // controller, if open
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const upper = this.dom.spawn(this.element, "DIV", ["upper"]);
    if (!this.voice) return;
    
    if (this.key === "global") {
      this.dom.spawn(upper, "INPUT", { type: "button", value: "X", on_click: () => this.requestDeleteVoice(this.voice) });
      this.dom.spawn(upper, "INPUT", { type: "button", value: "+Op", on_click: () => this.requestAddOp(this.voice) });
      return;
    }
    
    if (this.op) {
      this.dom.spawn(upper, "INPUT", { type: "button", value: "X", on_click: () => this.requestDeleteOp(this.voice, this.op) });
      this.dom.spawn(upper, "INPUT", { type: "button", value: "^", on_click: () => this.requestOpMotion(this.voice, this.op, -1) });
      this.dom.spawn(upper, "INPUT", { type: "button", value: "v", on_click: () => this.requestOpMotion(this.voice, this.op, 1) });
    }
    
    this.dom.spawn(upper, "DIV", ["key"], this.key);
    this.dom.spawn(upper, "DIV", ["spacer"]);
    
    switch (this.key) {
      case "shape": this.spawnShapesMenu(upper); break;
      case "harmonics": this.spawnOpenButton(upper); break;
      case "fm": this.spawnFmFields(upper); break;
      case "fmenv": this.spawnOpenButton(upper); break;
      case "rate": this.spawnOpenButton(upper); break;
      case "ratelfo": this.spawnRateLfoFields(upper); break;
      case "level": this.spawnOpenButton(upper); break;
      case "gain": this.spawnScalar(upper, this.op.gain, v => this.op.gain = v); break;
      case "clip": this.spawnScalar(upper, this.op.clip, v => this.op.clip = v); break;
      case "delay": this.spawnDelayFields(upper); break;
      case "bandpass": this.spawnBandFields(upper, (m, w) => { this.op.freq = m; this.op.width = w; }); break;
      case "notch": this.spawnBandFields(upper, (m, w) => { this.op.freq = m; this.op.width = w; }); break;
      case "lopass": this.spawnScalar(upper, this.op.freq, v => this.op.freq = v); break;
      case "hipass": this.spawnScalar(upper, this.op.freq, v => this.op.freq = v); break;
    }
  }
  
  spawnOpenButton(upper) {
    this.dom.spawn(upper, "INPUT", { type: "button", value: "O", on_click: () => this.onToggleLowerPanel() });
  }
  
  spawnScalar(upper, v, cb) {
    const input = this.dom.spawn(upper, "INPUT", { type: "number", value: v });
    if (cb) input.addEventListener("input", () => cb(+input.value));
  }
  
  spawnShapesMenu(upper) {
    const select = this.dom.spawn(upper, "SELECT");
    for (const name of Sound.shapes) {
      this.dom.spawn(select, "OPTION", { value: name }, name);
    }
    select.value = this.voice.shape;
    select.addEventListener("change", () => this.voice.shape = select.value);
  }
  
  spawnFmFields(upper) {
    const r = this.dom.spawn(upper, "INPUT", { type: "number", name: "fmRate", value: this.voice.fmRate });
    const s = this.dom.spawn(upper, "INPUT", { type: "number", name: "fmScale", value: this.voice.fmScale });
    r.addEventListener("input", () => { this.voice.fmRate = +r.value; });
    s.addEventListener("input", () => { this.voice.fmScale = +s.value; });
  }
  
  spawnRateLfoFields(upper) {
    const r = this.dom.spawn(upper, "INPUT", { type: "number", name: "rateLfoRate", value: this.voice.rateLfoRate });
    const d = this.dom.spawn(upper, "INPUT", { type: "number", name: "rateLfoDepth", value: this.voice.rateLfoDepth });
    r.addEventListener("input", () => { this.voice.rateLfoRate = +r.value; });
    d.addEventListener("input", () => { this.voice.rateLfoDepth = +d.value; });
  }
  
  spawnDelayFields(upper) {
    const p = this.dom.spawn(upper, "INPUT", { type: "number", name: "period", value: this.op.period });
    const d = this.dom.spawn(upper, "INPUT", { type: "number", name: "dry", min: 0, max: 1, step: 0.1, value: this.op.dry });
    const w = this.dom.spawn(upper, "INPUT", { type: "number", name: "wet", min: 0, max: 1, step: 0.1, value: this.op.wet });
    const s = this.dom.spawn(upper, "INPUT", { type: "number", name: "sto", min: 0, max: 1, step: 0.1, value: this.op.sto });
    const f = this.dom.spawn(upper, "INPUT", { type: "number", name: "fbk", min: 0, max: 1, step: 0.1, value: this.op.fbk });
    upper.addEventListener("input", () => {
      this.op.period = +p.value;
      this.op.dry = +d.value;
      this.op.wet = +w.value;
      this.op.sto = +s.value;
      this.op.fbk = +f.value;
    });
  }
  
  spawnBandFields(upper, cb) {
    const f = this.dom.spawn(upper, "INPUT", { type: "number", name: "freq", value: this.op.freq });
    const w = this.dom.spawn(upper, "INPUT", { type: "number", name: "width", value: this.op.width });
    if (cb) upper.addEventListener("input", () => cb(+f.value, +w.value));
  }
  
  setupGlobal(voice) {
    this.voice = voice;
    this.key = "global";
    this.op = null;
    this.buildUi();
  }
  
  setupHeader(voice, key) {
    this.voice = voice;
    this.key = key;
    this.op = null;
    this.buildUi();
  }
  
  setupOp(voice, op) {
    this.voice = voice;
    this.key = op.cmd;
    this.op = op;
    this.buildUi();
  }
  
  onToggleLowerPanel() {
    if (this.lowerPanel) {
      this.lowerPanel.element?.remove();
      this.lowerPanel = null;
    } else switch (this.key) {
    
      case "harmonics": {
          this.lowerPanel = this.dom.spawnController(this.element, HarmonicsUi);
          this.lowerPanel.setup(this.voice.harmonics);
          this.lowerPanel.dirty = (harmonics) => {
            this.voice.harmonics = harmonics;
            this.dirty();
          };
        } break;
        
      case "fmenv": {
          if (!this.voice.fmEnv) this.voice.fmEnv = new Env([1]);
          this.lowerPanel = this.dom.spawnController(this.element, EnvUi);
          this.lowerPanel.setup(this.voice.fmEnv, 1, false);
          this.lowerPanel.onChanged = () => this.dirty();
        } break;
        
      case "rate": {
          if (!this.voice.rate) this.voice.rate = new Env([440]);
          this.lowerPanel = this.dom.spawnController(this.element, EnvUi);
          this.lowerPanel.setup(this.voice.rate, 20000, true); // Scale should really be 65535, but it's Hz, anything above 20k or so is useless
          this.lowerPanel.onChanged = () => this.dirty();
        } break;
        
      case "level": {
          this.lowerPanel = this.dom.spawnController(this.element, EnvUi);
          this.lowerPanel.setup(this.op.env, 1, false);
          this.lowerPanel.onChanged = () => this.dirty();
        } break;
    }
  }
}
