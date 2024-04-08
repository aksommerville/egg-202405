/* VoiceUi.js
 * Card with controls for one voice.
 */
 
import { Dom } from "./Dom.js";
import { Bus } from "./Bus.js";
import { Sound, Op } from "./Sound.js";
import { VoiceFieldUi } from "./VoiceFieldUi.js";

export class VoiceUi {
  static getDependencies() {
    return [HTMLElement, Dom, Bus, "discriminator"];
  }
  constructor(element, dom, bus, discriminator) {
    this.element = element;
    this.dom = dom;
    this.bus = bus;
    this.discriminator = discriminator;
    
    this.voice = null;
    
    this.buildUi();
    
    this.element.addEventListener("change", e => this.onChange(e));
  }
  
  onRemoveFromDom() {
  }
  
  setVoice(voice) {
    this.voice = voice;
    this.populateUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
  }
  
  populateUi() {
    this.element.innerHTML = "";
    if (!this.voice) return;
    
    const gfield = this.dom.spawnController(this.element, VoiceFieldUi);
    gfield.setupGlobal(this.voice);
    gfield.requestDeleteVoice = voice => this.onRequestDeleteVoice(voice);
    gfield.requestAddOp = voice => this.onRequestAddOp(voice);
    
    for (const k of ["shape", "harmonics", "fm", "fmenv", "rate", "ratelfo"]) {
      const field = this.dom.spawnController(this.element, VoiceFieldUi);
      field.setupHeader(this.voice, k);
      field.dirty = () => this.bus.soundDirty(this);
    }
    
    for (const op of this.voice.ops) {
      const field = this.dom.spawnController(this.element, VoiceFieldUi);
      field.setupOp(this.voice, op);
      field.requestDeleteOp = (voice, op) => this.onRequestDeleteOp(voice, op);
      field.requestOpMotion = (voice, op, dp) => this.onRequestOpMotion(voice, op, dp);
      field.dirty = () => this.bus.soundDirty(this);
    }
  }
  
  onRequestDeleteVoice(voice) {
    const p = this.bus.sound.voices.indexOf(voice);
    if (p >= 0) {
      this.bus.sound.voices.splice(p, 1);
      this.bus.soundDirty(this);
    }
    this.element.remove();
  }
  
  onRequestAddOp(voice) {
    this.dom.prompt(`op (level, gain, clip, delay, bandpass, notch, lopass, hipass):`).then(cmd => {
      if (!cmd) return;
      const op = new Op([cmd]);
      voice.ops.push(op);
      
      const field = this.dom.spawnController(this.element, VoiceFieldUi);
      field.setupOp(this.voice, op);
      field.requestDeleteOp = (voice, op) => this.onRequestDeleteOp(voice, op);
      field.requestOpMotion = (voice, op, dp) => this.onRequestOpMotion(voice, op, dp);
      field.dirty = () => this.bus.soundDirty(this);
      
      this.bus.soundDirty(this);
    }).catch(() => {});
  }
  
  onRequestDeleteOp(voice, op) {
    const p = voice.ops.indexOf(op);
    if (p >= 0) {
      voice.ops.splice(p, 1);
      this.bus.soundDirty(this);
    }
    for (const element of this.element.querySelectorAll(".VoiceFieldUi")) {
      if (element.__controller?.op === op) {
        element.remove();
        break;
      }
    }
  }
  
  onRequestOpMotion(voice, op, dp) {
    const vfromp = voice.ops.indexOf(op);
    if (vfromp < 0) return;
    if (dp < 0) {
      if (vfromp > 0) {
        voice.ops.splice(vfromp, 1);
        voice.ops.splice(vfromp - 1, 0, op);
      } else {
        return;
      }
    } else {
      if (vfromp < voice.ops.length - 1) {
        voice.ops.splice(vfromp, 1);
        voice.ops.splice(vfromp + 1, 0, op);
      } else {
        return;
      }
    }
    this.bus.soundDirty(this);
    
    // Ui:
    const children = Array.from(this.element.children);
    const fromp = children.findIndex(e => e.__controller?.op === op);
    if (fromp < 0) return;
    const element = children[fromp];
    let top = fromp + dp;
    if ((top < 0) || (fromp === top) || (top >= children.length)) return;
    if (top > fromp) top++;
    this.dom.ignoreNextRemoval(element);
    this.element.insertBefore(element, children[top]);
  }
  
  onChange(event) {
    this.bus.soundDirty(this);
  }
}
