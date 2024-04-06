/* VoiceUi.js
 * One voice section in the board.
 * Should communicate only with its parent BoardUi.
 */
 
import { Dom } from "./Dom.js";
import { OscillatorUi } from "./OscillatorUi.js";
import { OpUi } from "./OpUi.js";

export class VoiceUi {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.onChanged = () => {};
    
    this.buildUi();
  }
  
  buildUi() {
    this.innerHTML = "";
    this.oscillatorUi = this.dom.spawnController(this.element, OscillatorUi);
    this.oscillatorUi.onChanged = () => this.onChanged();
    this.dom.spawn(this.element, "DIV", ["ops"]);
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "+ Op", on_click: () => this.addOp([]) });
  }
  
  addTextLine(src) {
    switch (src[0]) {
      case "wave":
      case "fmrange":
      case "fmlfo":
      case "ratelfo":
      case "rate": {
          this.oscillatorUi.addCommand(src);
        } break;
      default: {
          this.addOp(src);
        }
    }
  }
  
  ready() {
    this.oscillatorUi.ready();
    for (const elem of this.element.querySelectorAll(".OpUi")) {
      const controller = elem.__controller;
      if (!(controller instanceof OpUi)) continue;
      controller.ready();
    }
  }
  
  addOp(src) {
    const list = this.element.querySelector(".ops");
    const controller = this.dom.spawnController(list, OpUi);
    controller.setup(src);
    controller.onChanged = () => this.onChanged();
    controller.requestMotion = (opui, dp) => this.onMoveOp(opui, dp);
    controller.requestDeletion = (opui) => this.onDeleteOp(opui);
  }
  
  encode() {
    let dst = this.oscillatorUi.encode();
    for (const elem of this.element.querySelectorAll(".OpUi")) {
      const controller = elem.__controller;
      if (!(controller instanceof OpUi)) continue;
      dst += controller.encode();
    }
    return dst;
  }
  
  onMoveOp(opui, dp) {
    const parent = this.element.querySelector(".ops");
    let op = -1;
    for (let i=parent.children.length; i-->0; ) {
      console.log(i, parent.children[i], opui.element, parent.children[i] === opui.element);
      if (parent.children[i] === opui.element) {
        op = i;
        break;
      }
    }
    if (op < 0) return;
    let np = op + dp;
    if (np < 0) np = 0;
    else if (np >= parent.children.length) np = parent.children.length - 1;
    if (op === np) return;
    this.dom.ignoreNextRemoval(opui.element);
    if (np > op) parent.insertBefore(opui.element, parent.children[np + 1]);
    else parent.insertBefore(opui.element, parent.children[np]);
  }
  
  onDeleteOp(opui) {
    opui.element.remove();
    this.onChanged();
  }
}
