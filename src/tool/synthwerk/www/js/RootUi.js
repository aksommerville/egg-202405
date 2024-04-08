/* RootUi.js
 * Top of our view controller hierarchy.
 */
 
import { Dom } from "./Dom.js";
import { Bus } from "./Bus.js";
import { InputUi } from "./InputUi.js";
import { OutputUi } from "./OutputUi.js";
import { TextModal } from "./TextModal.js";
import { HexModal } from "./HexModal.js";
import { PlaybackService } from "./PlaybackService.js";

export class RootUi {
  static getDependencies() {
    return [HTMLElement, Dom, Bus, PlaybackService];
  }
  constructor(element, dom, bus, playbackService) {
    this.element = element;
    this.dom = dom;
    this.bus = bus;
    this.playbackService = playbackService; // We don't use it, but somebody has to inject it for it to exist.
    
    this.inputUi = null;
    this.outputUi = null;
    this.busListener = this.bus.listen(e => this.onBusEvent(e));
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.bus.unlisten(this.busListener);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.inputUi = this.dom.spawnController(this.element, InputUi);
    this.outputUi = this.dom.spawnController(this.element, OutputUi);
  }
  
  onBusEvent(event) {
    switch (event.type) {
      case "showText": this.dom.presentModal(TextModal); break;
      case "showHex": this.dom.presentModal(HexModal); break;
    }
  }
}
