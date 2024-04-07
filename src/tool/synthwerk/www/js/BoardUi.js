/* BoardUi.js
 * Upper-right side of window, the main graphical workspace.
 */
 
import { Dom } from "./Dom.js";

export class BoardUi {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const global = this.dom.spawn(this.element, "DIV", ["global"]);
    this.dom.spawn(global, "INPUT", { type: "range", min: 0, max: 1, step: 0.001, name: "master", value: 1, on_input: () => this.onMasterChanged() });
    this.dom.spawn(global, "BUTTON", { on_click: () => this.onAddVoice() }, "+ Vc");
  }
  
  onMasterChanged() {
  }
  
  onAddVoice() {
  }
}
