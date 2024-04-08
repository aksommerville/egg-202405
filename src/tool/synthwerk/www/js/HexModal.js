/* TextModal.js
 * Show the compiled song as a hex dump.
 */
 
import { Dom } from "./Dom.js";
import { Bus } from "./Bus.js";

export class HexModal {
  static getDependencies() {
    return [HTMLElement, Dom, Bus];
  }
  constructor(element, dom, bus) {
    this.element = element;
    this.dom = dom;
    this.bus = bus;
    
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "PRE", ["hexdump"], this.reprHex(this.bus.requireBin()));
  }
  
  reprHex(src) {
    let dst = "";
    for (let srcp=0; srcp<src.length; ) {
      for (let i=16; (i-->0) && (srcp<src.length); srcp++) {
        dst += "0123456789abcdef"[src[srcp] >> 4];
        dst += "0123456789abcdef"[src[srcp] & 15];
        dst += " ";
      }
      dst += "\n";
    }
    return dst;
  }
}
