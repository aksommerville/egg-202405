/* BoardUi.js
 * Upper-right side of window, the main graphical workspace.
 */
 
import { Dom } from "./Dom.js";
import { Comm } from "./Comm.js";
import { VoiceUi } from "./VoiceUi.js";

const REPORT_TIMEOUT_MS = 50;

export class BoardUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, Window];
  }
  constructor(element, dom, comm, window) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.window = window;
    
    this.commListener = this.comm.listen(e => this.onCommEvent(e));
    this.reportTimeout = null;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.comm.unlisten(this.commListener);
    if (this.reportTimeout) {
      this.window.clearTimeout(this.reportTimeout);
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const global = this.dom.spawn(this.element, "DIV", ["global"]);
    this.dom.spawn(global, "INPUT", { type: "range", min: 0, max: 1, step: 0.001, name: "master", value: 1, on_input: () => this.onMasterChanged() });
    this.dom.spawn(global, "BUTTON", { on_click: () => this.onAddVoice() }, "+ Vc");
  }
  
  rebuildFromText(text) {
    for (const elem of this.element.querySelectorAll(".VoiceUi")) elem.remove();
    let voiceUi = null;
    for (const line of text.split('\n')) {
      const words = line.split('#')[0].split(/\s+/).filter(v => v);
      if (words.length < 1) continue;
      switch (words[0]) {
        case "master": this.element.querySelector("input[name='master']").value = words[1]; break;
        case "voice": {
            if (voiceUi) voiceUi.ready();
            voiceUi = null;
          } break;
        default: {
            if (!voiceUi) {
              voiceUi = this.dom.spawnController(this.element, VoiceUi);
              voiceUi.onChanged = () => this.reportSoon();
            }
            voiceUi.addTextLine(words);
          }
      }
    }
    if (voiceUi) voiceUi.ready();
  }
  
  generateText() {
    let dst = "";
    dst += `master ${this.element.querySelector("input[name='master']").value}\n`;
    let first = true;
    for (const velem of this.element.querySelectorAll(".VoiceUi")) {
      const controller = velem.__controller;
      if (!(controller instanceof VoiceUi)) continue;
      if (first) first = false;
      else dst += "voice\n";
      dst += controller.encode();
    }
    return dst;
  }
  
  reportNow() {
    const text = this.generateText();
    this.comm.textChanged(text, this);
  }
  
  reportSoon() {
    if (this.reportTimeout) this.window.clearTimeout(this.reportTimeout);
    this.reportTimeout = this.window.setTimeout(() => {
      this.reportTimeout = null;
      this.reportNow();
    }, REPORT_TIMEOUT_MS);
  }
  
  onCommEvent(event) {
    switch (event.type) {
      case "text": if (event.sender !== this) this.rebuildFromText(event.text); break;
    }
  }
  
  onMasterChanged() {
    this.reportSoon();
  }
  
  onAddVoice() {
    const controller = this.dom.spawnController(this.element, VoiceUi);
    controller.ready();
    controller.onChanged = () => this.reportSoon();
  }
}
