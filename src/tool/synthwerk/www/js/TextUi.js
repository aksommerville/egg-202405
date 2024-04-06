/* TextUi.js
 * Left side of window, shows the raw text and hexdump of binary.
 */
 
import { Dom } from "./Dom.js";
import { Comm } from "./Comm.js";

export class TextUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm];
  }
  constructor(element, dom, comm) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    
    this.commListener = this.comm.listen(e => this.onCommEvent(e));
    
    this.buildUi();
    
    //XXX TEMP: Fill with some text, after allowing a little time for the rest of the app to start up.
    window.setTimeout(() => {
      const elem = this.element.querySelector("textarea");
      elem.value = `
  master 0.200
  #wave 0.900 0.000 0.400 0.000 0.100
  wave fm=0.5,3
  fmrange 0 20 1 20 0.5 600 0
  rate 150 150 50
  level 0.000 20 1.000 20 0.500 800 0.000
      `;
      this.onInput();
    }, 100);
  }
  
  onRemoveFromDom() {
    this.comm.unlisten(this.commListener);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "TEXTAREA", { name: "text", on_input: () => this.onInput() });
    this.dom.spawn(this.element, "PRE", ["binary"]);
  }
  
  onInput() {
    const text = this.element.querySelector("*[name='text']").value;
    this.comm.textChanged(text, this);
  }
  
  setText(text) {
    this.element.querySelector("*[name='text']").value = text;
  }
  
  setReady(bin) {
    const elem = this.element.querySelector(".binary");
    elem.classList.remove("dirty");
    elem.classList.remove("error");
    elem.innerText = this.dumpHex(bin);
  }
  
  dumpHex(bin) {
    let dst = "";
    for (let binp=0; binp<bin.length; ) {
      for (let i=0; (i<16) && (binp<bin.length); i++, binp++) {
        const ch = bin.charCodeAt(binp);
        dst += ' ' + "0123456789abcdef"[ch >> 4] + "0123456789abcdef"[ch & 15];
      }
      dst += "\n";
    }
    return dst;
  }
  
  setDirty() {
    const elem = this.element.querySelector(".binary");
    elem.classList.add("dirty");
    elem.classList.remove("error");
    elem.innerText = "";
  }
  
  setError() {
    // We don't care about the content of the error. WaveUi shows that to the user.
    const elem = this.element.querySelector(".binary");
    elem.classList.remove("dirty");
    elem.classList.add("error");
    elem.innerText = "";
  }
  
  onCommEvent(event) {
    switch (event.type) {
      case "dirty": this.setDirty(); break;
      case "ready": this.setReady(event.bin); break;
      case "error": this.setError(); break;
      case "text": if (event.sender !== this) this.setText(event.text); break;
    }
  }
}
