/* OutputUi.js
 * Bottom half of the root view.
 * Contains a visualization of the wave, and buttons to view or replace text and hexdump.
 */
 
import { Dom } from "./Dom.js";
import { Bus } from "./Bus.js";
import { WaveVisualUi } from "./WaveVisualUi.js";
import { Voice } from "./Sound.js";

export class OutputUi {
  static getDependencies() {
    return [HTMLElement, Dom, Bus, "discriminator"];
  }
  constructor(element, dom, bus, discriminator) {
    this.element = element;
    this.dom = dom;
    this.bus = bus;
    this.discriminator = discriminator;
    
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
    this.dom.spawn(buttons, "INPUT", { type: "button", value: "+Voice", on_click: () => this.onAddVoice() });
    this.dom.spawn(buttons, "INPUT", { type: "button", value: "Text", on_click: () => this.bus.dispatch({ type: "showText" }) });
    this.dom.spawn(buttons, "INPUT", { type: "button", value: "Hex", on_click: () => this.bus.dispatch({ type: "showHex" }) });
    this.dom.spawn(buttons, "INPUT", { type: "button", value: "New", on_click: () => this.bus.dispatch({ type: "newSound" }) });
    const autoPlayId = `OutputUi-${this.discriminator}-autoPlay`;
    this.dom.spawn(
      this.dom.spawn(buttons, "LABEL", { for: autoPlayId }, "Auto play"),
      "INPUT", { type: "checkbox", name: "autoPlay", id: autoPlayId, on_change: () => this.onAutoPlayChange() }
    );
    if (this.bus.autoPlay) this.element.querySelector("input[name='autoPlay']").checked = true;
    this.waveVisualUi = this.dom.spawnController(this.element, WaveVisualUi);
  }
  
  onAutoPlayChange() {
    const checked = this.element.querySelector("input[name='autoPlay']").checked;
    this.bus.setAutoPlay(checked);
  }
  
  onBusEvent(event) {
    switch (event.type) {
    }
  }
  
  onAddVoice() {
    this.bus.sound.voices.push(Voice.newWithDefaults());
    this.bus.soundDirty(this);
  }
}
