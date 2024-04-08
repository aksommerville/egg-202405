/* SliderUi.js
 * Simple vertical slider.
 * Alas <input type="range"> doesn't support this consistently yet.
 */
 
import { Dom } from "./Dom.js";

export class SliderUi {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.mouseListener = null;
    this.value = 1; // 0..1
    this.onchange = v => {}; // updates on mouseup, once per gesture
    this.oninput = v => {}; // updates on motion
    
    this.buildUi();
    
    this.element.addEventListener("mousedown", e => this.onThumbMouseDown(e));
  }
  
  onRemoveFromDom() {
    this.dropMouseListener();
  }
  
  setValue(v, display) {
    this.value = v;
    this.setDisplayValue(display);
    // Kick the UI adjustment out to the next frame; we're usually not fully installed when this gets called.
    this.window.setTimeout(() => {
      const outerBounds = this.element.getBoundingClientRect();
      const thumb = this.element.querySelector(".thumb");
      const thumbBounds = thumb.getBoundingClientRect();
      const top = Math.round((1 - v) * (outerBounds.height - thumbBounds.height));
      thumb.style.top = `${top}px`;
    }, 0);
  }
  
  setDisplayValue(v) {
    if ((typeof(v) !== "string") && (typeof(v) !== "number")) v = "";
    this.element.querySelector(".display").innerText = v;
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["groove"]);
    const thumb = this.dom.spawn(this.element, "DIV", ["thumb"]);
    this.dom.spawn(thumb, "DIV", ["display"]);
  }
  
  dropMouseListener() {
    if (this.mouseListener) {
      this.window.removeEventListener("mousemove", this.mouseListener);
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.mouseListener = null;
    }
  }
  
  onMouseMoveOrUp(event) {
    if (event.type === "mouseup") {
      this.dropMouseListener();
      this.onchange(this.value);
      return;
    }
    const outerBounds = this.element.getBoundingClientRect();
    const thumb = this.element.querySelector(".thumb");
    const thumbBounds = thumb.getBoundingClientRect();
    const normy = Math.max(0, Math.min(1, 1 - (event.clientY - outerBounds.y - (thumbBounds.height / 2)) / (outerBounds.height - thumbBounds.height)));
    this.value = normy;
    const top = Math.round((1 - normy) * (outerBounds.height - thumbBounds.height));
    thumb.style.top = `${top}px`;
    this.oninput(this.value);
  }
  
  onThumbMouseDown(event) {
    this.dropMouseListener();
    this.mouseListener = e => this.onMouseMoveOrUp(e);
    this.window.addEventListener("mousemove", this.mouseListener);
    this.window.addEventListener("mouseup", this.mouseListener);
    this.onMouseMoveOrUp(event); // the mousedown itself triggers an update too
  }
}
