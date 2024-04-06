/* OscillatorUi.js
 * Top of the voice section.
 * Manages oscillator commands: wave, fmrange, fmlfo, ratelfo, rate
 */
 
import { Dom } from "./Dom.js";
import { EnvUi } from "./EnvUi.js";

export class OscillatorUi {
  static getDependencies() {
    return [HTMLElement, Dom, "discriminator"];
  }
  constructor(element, dom, discriminator) {
    this.element = element;
    this.dom = dom;
    this.discriminator = discriminator;
    
    this.fmrangeEnvUi = null;
    this.onChanged = () => {};
    
    this.element.addEventListener("change", () => this.onChanged());
    
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    const modeSelect = this.dom.spawn(this.element, "SELECT", { name: "mode", on_change: () => this.onModeChanged() });
    this.dom.spawn(modeSelect, "OPTION", { value: "silence" }, "Silence (delete)");
    this.dom.spawn(modeSelect, "OPTION", { value: "noise" }, "Noise");
    this.dom.spawn(modeSelect, "OPTION", { value: "sine" }, "Sine");
    this.dom.spawn(modeSelect, "OPTION", { value: "square" }, "Square");
    this.dom.spawn(modeSelect, "OPTION", { value: "sawdown" }, "Saw Down");
    this.dom.spawn(modeSelect, "OPTION", { value: "sawup" }, "Saw Up");
    this.dom.spawn(modeSelect, "OPTION", { value: "triangle" }, "Triangle");
    this.dom.spawn(modeSelect, "OPTION", { value: "harmonics" }, "Harmonics");
    this.dom.spawn(modeSelect, "OPTION", { value: "fm" }, "FM");
    
    // Additional parameters to "wave".
    const fmUi = this.dom.spawn(this.element, "DIV", ["fm", "hidden"]);
    this.spawnRow(fmUi, "Rel Rate", "fmrate", 0, 256, 0.1);
    this.spawnRow(fmUi, "Scale", "fmscale", 0, 256, 0.1);
    this.dom.spawn(this.element, "INPUT", ["hidden", "harmonics"], { name: "harmonics", type: "text" });//TODO Use a bar chart instead.
    
    this.fmrangeEnvUi = this.dom.spawnController(this.element, EnvUi);
    this.fmrangeEnvUi.element.classList.add("fmrange");
    this.fmrangeEnvUi.element.classList.add("hidden");
    this.fmrangeEnvUi.range = 1;
    this.fmrangeEnvUi.onChanged = () => this.onChanged();
    
    const fmlfoUi = this.dom.spawn(this.element, "DIV", ["fmlfo", "hidden"]);
    this.spawnRow(fmlfoUi, "Hz", "fmlfo_rate", 0, 256, 0.1);
    this.spawnRow(fmlfoUi, "Mlt", "fmlfo_depth", 0, 256, 0.1);
    
    this.rateEnvUi = this.dom.spawnController(this.element, EnvUi);
    this.rateEnvUi.element.classList.add("rate");
    this.rateEnvUi.range = 65535;
    this.rateEnvUi.valueIsHertz = true;
    this.rateEnvUi.onChanged = () => this.onChanged();
    
    const ratelfoUi = this.dom.spawn(this.element, "DIV", ["ratelfo"]);
    this.spawnRow(ratelfoUi, "Hz", "ratelfo_rate", 0, 256, 0.1);
    this.spawnRow(ratelfoUi, "Cents", "ratelfo_depth", 0, 65535, 1);
  }
  
  spawnRow(parent, label, key, min, max, step) {
    const id = `OscillatorUi_${this.discriminator}_${key}`;
    const row = this.dom.spawn(parent, "DIV", ["row"]);
    this.dom.spawn(row, "LABEL", { for: id }, label);
    return this.dom.spawn(row, "INPUT", { type: "number", id, name: key, min, max, step });
  }
  
  onModeChanged() {
    const mode = this.element.querySelector("*[name='mode']").value;
    switch (mode) {
      case "fm": this.showFeatures("fm", "fmrange", "fmlfo", "ratelfo", "rate"); break;
      case "harmonics": this.showFeatures("harmonics", "ratelfo", "rate"); break;
      case "noise": this.showFeatures(); break;
      default: this.showFeatures("ratelfo", "rate"); break;
    }
  }
  
  showFeatures(...features) {
    for (const clsname of ["fm", "harmonics", "fmrange", "fmlfo", "ratelfo", "rate"]) {
      const elem = this.element.querySelector(`.${clsname}`);
      if (!elem) continue;
      if (features.indexOf(clsname) >= 0) elem.classList.remove("hidden");
      else elem.classList.add("hidden");
    }
  }
  
  addCommand(src) {
    switch (src[0]) {
    
      case "wave": {
          if (!src[1] || (src[1] === "silence")) {
            this.element.querySelector("select[name='mode']").value = "silence";
          } else if (src[1].startsWith("fm=")) {
            this.element.querySelector("select[name='mode']").value = "fm";
            const [rate, range] = src[1].split('=')[1].split(',');
            this.element.querySelector("input[name='fmrate']").value = rate;
            this.element.querySelector("input[name='fmscale']").value = range;
          } else if (src[1].match(/^\d/)) {
            this.element.querySelector("select[name='mode']").value = "harmonics";
            this.element.querySelector("input[name='harmonics']").value = src[1];
          } else {
            this.element.querySelector("select[name='mode']").value = src[1];
          }
          this.onModeChanged();
        } break;
        
      case "fmrange": {
          this.fmrangeEnvUi.setup(src.slice(1));
        } break;
        
      case "fmlfo": {
          console.log(`TODO OscillatorUi set fmlfo: ${JSON.stringify(src)}`);
          this.element.querySelector("input[name='fmlfo_rate']").value = src[1];
          this.element.querySelector("input[name='fmlfo_depth']").value = src[2];
        } break;
        
      case "ratelfo": {
          console.log(`TODO OscillatorUi set ratelfo: ${JSON.stringify(src)}`);
          this.element.querySelector("input[name='ratelfo_rate']").value = src[1];
          this.element.querySelector("input[name='ratelfo_depth']").value = src[2];
        } break;
        
      case "rate": {
          this.rateEnvUi.setup(src.slice(1));
        } break;
    }
  }
  
  ready() {
    if (!this.rateEnvUi.points.length) {
      this.rateEnvUi.setup(["440"]);
    }
  }
  
  encode() {
    let dst = "";
    const mode = this.element.querySelector("select[name='mode']").value;
    switch (mode) {
    
      case "fm": {
          const rate = this.element.querySelector("input[name='fmrate']").value;
          const range = this.element.querySelector("input[name='fmscale']").value;
          dst += `wave fm=${rate},${range}\n`;
          dst += this.encodeFmrange();
          dst += this.encodeFmlfo();
          dst += this.encodeRatelfo();
          dst += this.encodeRate();
        } break;
        
      case "harmonics": {
          dst += "wave " + this.element.querySelector("input[name='harmonics']").value + "\n";
          dst += this.encodeRatelfo();
          dst += this.encodeRate();
        } break;
        
      case "noise": {
          dst += "wave noise\n";
        } break;
        
      default: {
          dst += `wave ${mode}\n`;
          dst += this.encodeRatelfo();
          dst += this.encodeRate();
        } break;
    }
    return dst;
  }
  
  encodeFmrange() {
    const serial = this.fmrangeEnvUi.encode();
    if (!serial) return "";
    return `fmrange ${serial}\n`;
  }
  
  encodeFmlfo() {
    const rate = this.element.querySelector("input[name='fmlfo_rate']").value;
    const depth = this.element.querySelector("input[name='fmlfo_depth']").value;
    if (!rate && !depth) return "";
    return `fmlfo ${rate} ${depth}\n`;
  }
  
  encodeRatelfo() {
    const rate = this.element.querySelector("input[name='ratelfo_rate']").value;
    const depth = this.element.querySelector("input[name='ratelfo_depth']").value;
    if (!rate && !depth) return "";
    return `ratelfo ${rate} ${depth}\n`;
  }
  
  encodeRate() {
    const serial = this.rateEnvUi.encode();
    return `rate ${serial}\n`;
  }
}
