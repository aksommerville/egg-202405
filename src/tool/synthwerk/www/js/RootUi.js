/* RootUi.js
 * Top of our view controller hierarchy.
 */
 
import { Dom } from "./Dom.js";
import { TextUi } from "./TextUi.js";
import { BoardUi } from "./BoardUi.js";
import { WaveUi } from "./WaveUi.js";

export class RootUi {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.waveUi = null;
    this.textUi = null;
    this.boardUi = null;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.textUi = this.dom.spawnController(this.element, TextUi);
    const right = this.dom.spawn(this.element, "DIV", ["right"]);
    this.boardUi = this.dom.spawnController(right, BoardUi);
    this.waveUi = this.dom.spawnController(right, WaveUi);
  }
}
