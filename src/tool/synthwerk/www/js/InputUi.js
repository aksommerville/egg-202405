/* InputUi.js
 * Top half of the root view.
 * Contains the full logical model.
 */
 
import { Dom } from "./Dom.js";
import { Bus } from "./Bus.js";
import { SliderUi } from "./SliderUi.js";
import { VoiceUi } from "./VoiceUi.js";

// Technically should be 65535, but that's a hell of a long sound effect.
// Keep it short enough to not waste most of the range slider.
const TIME_RANGE_LIMIT = 5000;

export class InputUi {
  static getDependencies() {
    return [HTMLElement, Dom, Bus];
  }
  constructor(element, dom, bus) {
    this.element = element;
    this.dom = dom;
    this.bus = bus;
    
    this.timeRangeUi = null; // SliderUi
    this.masterUi = null; // SliderUi
    
    this.busListener = this.bus.listen(e => this.onBusEvent(e));
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.bus.unlisten(this.busListener);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    const controls = this.dom.spawn(this.element, "DIV", ["controls"]);
    this.timeRangeUi = this.dom.spawnController(controls, SliderUi);
    this.timeRangeUi.oninput = v => this.onTimeRangeChange(v);
    this.timeRangeUi.setValue(this.sliderValueForTimeRange(this.bus.timeRange), this.bus.timeRange);
    this.masterUi = this.dom.spawnController(controls, SliderUi);
    this.masterUi.oninput = v => this.onMasterChange(v);
    this.masterUi.setValue(this.bus.sound.master, this.sliderDisplayForMaster(this.bus.sound.master));
    
    const voicesContainer = this.dom.spawn(this.element, "DIV", ["voicesContainer"]);
    for (const voice of this.bus.sound.voices) {
      const controller = this.dom.spawnController(voicesContainer, VoiceUi);
      controller.setVoice(voice);
    }
  }
  
  sliderValueForTimeRange(timeRange) {
    const lo = Math.log2(100);
    const hi = Math.log2(65535);
    const ranged = Math.log2(timeRange);
    const norm = (ranged - lo) / (hi - lo);
    return norm;
  }
  
  onTimeRangeChange(norm) {
    const lo = Math.log2(100);
    const hi = Math.log2(65535);
    const ranged = lo + (norm * (hi - lo));
    const timeRange = Math.round(Math.pow(2, ranged));
    this.timeRangeUi.setDisplayValue(timeRange);
    this.bus.dispatch({ type: "setTimeRange", timeRange });
  }
  
  sliderDisplayForMaster(master) {
    return Math.round(master * 100);
  }
  
  onMasterChange(v) {
    this.masterUi.setDisplayValue(this.sliderDisplayForMaster(v));
    this.bus.dispatch({ type: "setMaster", master: v });
  }
  
  onBusEvent(event) {
    switch (event.type) {
      case "soundDirty": {
          if (event.sender === this) return;
          if (event.sender instanceof VoiceUi) return;
          this.buildUi();
        } break;
      case "newSound": this.buildUi(); break;
    }
  }
}
