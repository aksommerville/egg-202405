/* OutputUi.js
 * Bottom half of the root view.
 * Contains a visualization of the wave, and buttons to view or replace text and hexdump.
 */
 
import { Dom } from "./Dom.js";
import { Bus } from "./Bus.js";
import { WaveVisualUi } from "./WaveVisualUi.js";

export class OutputUi {
  static getDependencies() {
    return [HTMLElement, Dom, Bus];
  }
  constructor(element, dom, bus) {
    this.element = element;
    this.dom = dom;
    this.bus = bus;
    
    this.waveVisualUi = null;
    
    this.busListener = this.bus.listen(e => this.onBusEvent(e));
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.bus.unlisten(this.busListener);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const buttons = this.dom.spawn(this.element, "DIV", ["buttons"]);
    this.dom.spawn(buttons, "INPUT", { type: "button", value: "Text", on_click: () => this.bus.dispatch({ type: "showText" }) });
    this.dom.spawn(buttons, "INPUT", { type: "button", value: "Hex", on_click: () => this.bus.dispatch({ type: "showHex" }) });
    this.dom.spawn(buttons, "INPUT", { type: "button", value: "New", on_click: () => this.bus.dispatch({ type: "newSound" }) });
    this.waveVisualUi = this.dom.spawnController(this.element, WaveVisualUi);
  }
  
  onBusEvent(event) {
    switch (event.type) {
    }
  }
}
