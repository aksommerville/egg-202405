/* WaveVisualUi.js
 * Lower-right side of window, shows the printed wave, and user can click to play it.
 */
 
import { Dom } from "./Dom.js";
import { Bus } from "./Bus.js";

const RENDER_TIMEOUT = 500;

export class WaveVisualUi {
  static getDependencies() {
    return [HTMLElement, Dom, Bus, Window];
  }
  constructor(element, dom, bus, window) {
    this.element = element;
    this.dom = dom;
    this.bus = bus;
    this.window = window;
    
    this.renderTimeout = null;
    
    this.element.addEventListener("click", () => this.onClick());
    this.busListener = this.bus.listen(e => this.onBusEvent(e));
    
    this.buildUi();
    this.renderSoon();
  }
  
  onRemoveFromDom() {
    this.bus.unlisten(this.busListener);
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "CANVAS", ["visual"]);
  }
  
  tracePeaks(ctx, wave, w, h) {
    ctx.beginPath();
    if (wave.length <= w) return; // Averages will draw the exact signal.
    for (let x=0, p=0; x<w; x++) {
      const np = Math.max(p+1, Math.round(((x+1)*wave.length)/w));
      let hi=wave[p], lo=wave[p];
      for (let q=p; q<np; q++) {
        if (wave[q]>hi) hi=wave[q];
        else if (wave[q]<lo) lo=wave[q];
      }
      const yhi = Math.round(h - ((hi+1) * h) / 2) + 0.5;
      const ylo = Math.round(h - ((lo+1) * h) / 2) + 1.5;
      ctx.moveTo(x+0.5, yhi);
      ctx.lineTo(x+0.5, ylo);
      p = np;
    }
  }
  
  traceAverages(ctx, wave, w, h) {
    ctx.beginPath();
    if (wave.length <= 0) return;
    ctx.moveTo(0, h >> 1);
    if (wave.length <= w) { // Draw exact line for very short signal.
      for (let p=0; p<wave.length; p++) {
        const x = ((p+1) * w) / wave.length;
        const y = h - ((wave[p]+1) * h) / 2;
        ctx.lineTo(x, y);
      }
    } else { // Trace the average, one point per column.
      for (let x=0, p=0; x<w; x++) {
        const np = Math.max(p+1, Math.round(((x+1)*wave.length)/w));
        let sum = 0;
        for (let q=p; q<np; q++) sum += wave[q];
        const avg = sum / (np - p);
        const y = Math.round(h - ((avg+1) * h) / 2) + 0.5;
        ctx.lineTo(x+0.5, y);
        p = np;
      }
    }
  }
  
  renderWave(wave) {
    const canvas = this.element.querySelector(".visual");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#001";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    this.tracePeaks(ctx, wave, bounds.width, bounds.height);
    ctx.strokeStyle = "#00a";
    ctx.stroke();
    this.traceAverages(ctx, wave, bounds.width, bounds.height);
    ctx.strokeStyle = "#08f";
    ctx.stroke();
  }
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderWave(this.bus.requireWave());
    }, RENDER_TIMEOUT);
  }
  
  onClick() {
    this.bus.dispatch({ type: "play" });
  }
  
  onBusEvent(event) {
    switch (event.type) {
      case "newSound":
      case "soundDirty": 
      case "setMaster": this.renderSoon(); break;
    }
  }
}
