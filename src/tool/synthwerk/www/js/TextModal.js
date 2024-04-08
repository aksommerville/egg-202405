/* TextModal.js
 * Show the song in text format, allows copy or pasting.
 */
 
import { Dom } from "./Dom.js";
import { Bus } from "./Bus.js";

const SAVE_TIMEOUT = 1000;

export class TextModal {
  static getDependencies() {
    return [HTMLElement, Dom, Bus, Window];
  }
  constructor(element, dom, bus, window) {
    this.element = element;
    this.dom = dom;
    this.bus = bus;
    this.window = window;
    
    this.saveTimeout = null;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    if (this.saveTimeout) {
      this.window.clearTimeout(this.saveTimeout);
      this.saveTimeout = null;
      this.saveNow();
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const textarea = this.dom.spawn(this.element, "TEXTAREA", { autofocus: "autofocus", on_input: () => this.onInput() });
    textarea.value = this.bus.requireText();
  }
  
  onInput() {
    this.saveSoon();
  }
  
  saveSoon() {
    if (this.saveTimeout) {
      this.window.clearTimeout(this.saveTimeout);
    } else {
      this.element.classList.add("dirty");
    }
    this.saveTimeout = this.window.setTimeout(() => {
      this.saveTimeout = null;
      this.element.classList.remove("dirty");
      this.saveNow();
    }, SAVE_TIMEOUT);
  }
  
  saveNow() {
    const text = this.element.querySelector("textarea").value;
    this.bus.replaceText(text, this);
  }
}
