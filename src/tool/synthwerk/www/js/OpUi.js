/* OpUi.js
 * Container for pipeline operations UI.
 */
 
import { Dom } from "./Dom.js";
import { EnvUi } from "./EnvUi.js";

export class OpUi {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.onChanged = () => {};
    this.requestMotion = (opui, dp) => {};
    this.requestDeletion = (opui) => {};
    
    this.envUi = null;
    this.command = "";
    
    this.buildUi();
  }
  
  setup(words) {
    const container = this.element.querySelector(".payload");
    this.command = words[0] || "";
    this.element.querySelector("select[name='command']").value = this.command;
    container.innerHTML = "";
    if (!words[0]) return;
    switch (words[0]) {
    
      case "level": {
          this.envUi = this.dom.spawnController(container, EnvUi);
          this.envUi.setup(words.slice(1));
          this.envUi.onChanged = () => this.onChanged();
        } break;
        
      case "gain": {
          this.dom.spawn(container, "INPUT", { type: "number", name: "gain", min: 0, max: 256, step: 0.1 }, words[1] || "1");
        } break;
        
      case "clip": {
          this.dom.spawn(container, "INPUT", { type: "number", name: "clip", min: 0, max: 1, step: 0.001 }, words[1] || "1");
        } break;
        
      case "delay": {
          this.dom.spawn(container, "INPUT", { type: "number", name: "period", min: 0, max: 65535, step: 1 }, words[1] || "250");
          this.dom.spawn(container, "INPUT", { type: "number", name: "dry", min: 0, max: 1, step: 0.001 }, words[2] || "0.5");
          this.dom.spawn(container, "INPUT", { type: "number", name: "wet", min: 0, max: 1, step: 0.001 }, words[3] || "0.5");
          this.dom.spawn(container, "INPUT", { type: "number", name: "sto", min: 0, max: 1, step: 0.001 }, words[4] || "0.5");
          this.dom.spawn(container, "INPUT", { type: "number", name: "fbk", min: 0, max: 1, step: 0.001 }, words[5] || "0.5");
        } break;
        
      case "bandpass": 
      case "notch": {
          this.dom.spawn(container, "INPUT", { type: "number", name: "mid", min: 0, max: 65535, step: 1 }, words[1] || "440");
          this.dom.spawn(container, "INPUT", { type: "number", name: "wid", min: 0, max: 65535, step: 1 }, words[1] || "100");
        } break;
        
      case "lopass": 
      case "hipass": {
          this.dom.spawn(container, "INPUT", { type: "number", name: "freq", min: 0, max: 65535, step: 1 }, words[1] || "440");
        } break;
        
      default: {
          console.error(`Unknown op command: ${JSON.stringify(words)}`);
        }
    }
  }
  
  ready() {
  }
  
  encode() {
    const v = k => this.element.querySelector(`*[name='${k}']`)?.value;
    switch (this.command) {
      case "level": return `level ${this.envUi.encode()}\n`;
      case "gain": return `gain ${v("gain")}\n`;
      case "clip": return `clip ${v("clip")}\n`;
      case "delay": return `delay ${v("period")} ${v("dry")} ${v("wet")} ${v("sto")} ${v("fbk")}\n`;
      case "bandpass": return `bandpass ${v("mid")} ${v("wid")}\n`;
      case "notch": return `notch ${v("mid")} ${v("wid")}\n`;
      case "lopass": return `lopass ${v("freq")}\n`;
      case "hipass": return `hipass ${v("freq")}\n`;
    }
    return "";
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const controls = this.dom.spawn(this.element, "DIV", ["controls"]);
    this.dom.spawn(controls, "INPUT", { type: "button", value: "^", on_click: () => this.requestMotion(this, -1) });
    this.dom.spawn(controls, "INPUT", { type: "button", value: "X", on_click: () => this.requestDeletion(this) });
    this.dom.spawn(controls, "INPUT", { type: "button", value: "v", on_click: () => this.requestMotion(this, 1) });
    
    const right = this.dom.spawn(this.element, "DIV", ["right"]);
    const commandMenu = this.dom.spawn(right, "SELECT", { name: "command", on_change: () => this.onCommandChanged() });
    this.dom.spawn(commandMenu, "OPTION", { value: "" }, "");
    this.dom.spawn(commandMenu, "OPTION", { value: "level" }, "level");
    this.dom.spawn(commandMenu, "OPTION", { value: "gain" }, "gain");
    this.dom.spawn(commandMenu, "OPTION", { value: "clip" }, "clip");
    this.dom.spawn(commandMenu, "OPTION", { value: "delay" }, "delay");
    this.dom.spawn(commandMenu, "OPTION", { value: "bandpass" }, "bandpass");
    this.dom.spawn(commandMenu, "OPTION", { value: "notch" }, "notch");
    this.dom.spawn(commandMenu, "OPTION", { value: "lopass" }, "lopass");
    this.dom.spawn(commandMenu, "OPTION", { value: "hipass" }, "hipass");
    
    this.dom.spawn(right, "DIV", ["payload"], { on_change: () => this.onChanged() });
  }
  
  onCommandChanged() {
    const command = this.element.querySelector("select[name='command']").value;
    this.setup([command]);
    this.onChanged();
  }
}
