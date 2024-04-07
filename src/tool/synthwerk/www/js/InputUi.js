/* InputUi.js
 * Top half of the root view.
 * Contains the full logical model.
 */
 
import { Dom } from "./Dom.js";
import { Bus } from "./Bus.js";

export class InputUi {
  static getDependencies() {
    return [HTMLElement, Dom, Bus];
  }
  constructor(element, dom, bus) {
    this.element = element;
    this.dom = dom;
    this.bus = bus;
    
    this.busListener = this.bus.listen(e => this.onBusEvent(e));
  }
  
  onRemoveFromDom() {
    this.bus.unlisten(this.busListener);
  }
  
  onBusEvent(event) {
    switch (event.type) {
    }
  }
}
