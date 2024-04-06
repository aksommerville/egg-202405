/* WaveUi.js
 * Lower-right side of window, shows the printed wave and provides play and stop buttons.
 * Or hey, just click on it to play or stop, that should be easier.
 */
 
import { Dom } from "./Dom.js";
import { Comm } from "./Comm.js";
import { PlaybackService } from "./PlaybackService.js";

export class WaveUi {
  static getDependencies() {
    return [HTMLElement, Dom, Comm, PlaybackService];
  }
  constructor(element, dom, comm, playbackService) {
    this.element = element;
    this.dom = dom;
    this.comm = comm;
    this.playbackService = playbackService;
    
    this.state = "init"; // "init", "ready", "dirty", "error"
    
    this.commListener = this.comm.listen(e => this.onCommEvent(e));
    
    this.element.addEventListener("click", () => this.onClick());
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.comm.unlisten(this.commListener);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "CANVAS", ["visual"]);
    const dirtyOverlay = this.dom.spawn(this.element, "DIV", ["dirty", "hidden", "overlay"]);
    this.dom.spawn(dirtyOverlay, "DIV", ["message"], "Waiting for compiler...");
    const errorOverlay = this.dom.spawn(this.element, "DIV", ["error", "hidden", "overlay"]);
    this.dom.spawn(errorOverlay, "DIV", ["message"], "");
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
      const yhi = Math.round(h - ((hi+32768) * h) / 65536) + 0.5;
      const ylo = Math.round(h - ((lo+32768) * h) / 65536) + 1.5;
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
        const y = h - ((wave[p]+32768) * h) / 65536;
        ctx.lineTo(x, y);
      }
    } else { // Trace the average, one point per column.
      for (let x=0, p=0; x<w; x++) {
        const np = Math.max(p+1, Math.round(((x+1)*wave.length)/w));
        let sum = 0;
        for (let q=p; q<np; q++) sum += wave[q];
        const avg = sum / (np - p);
        const y = Math.round(h - ((avg+32768) * h) / 65536) + 0.5;
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
  
  setReady(wave) {
    this.element.querySelector(".dirty").classList.add("hidden");
    this.element.querySelector(".error").classList.add("hidden");
    this.state = "ready";
    this.renderWave(wave);
  }
  
  setDirty() {
    this.element.querySelector(".dirty").classList.remove("hidden");
    this.element.querySelector(".error").classList.add("hidden");
    this.state = "dirty";
  }
  
  setError(error) {
    this.element.querySelector(".dirty").classList.add("hidden");
    const eelem = this.element.querySelector(".error");
    eelem.querySelector(".message").innerText = error?.message || error?.statusText || "Failed to compile.";
    eelem.classList.remove("hidden");
    this.state = "error";
  }
  
  onCommEvent(event) {
    switch (event.type) {
      case "dirty": this.setDirty(); break;
      case "ready": this.setReady(event.wave); break;
      case "error": this.setError(event.error); break;
    }
  }
  
  onClick() {
    if (this.state !== "ready") return;
    this.playbackService.playWave(null);
  }
}
