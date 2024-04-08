/* HarmonicsUi.js
 */
 
// The model stores up to 255 of them, but I can't see using that many.
const BAR_COUNT = 16;
 
export class HarmonicsUi {
  static getDependencies() {
    return [HTMLCanvasElement, Window];
  }
  constructor(element, window) {
    this.element = element;
    this.window = window;
    
    this.dirty = (harmonics) => {};
    
    this.harmonics = [];
    
    this.element.addEventListener("click", e => this.onClick(e, true));
    this.element.addEventListener("contextmenu", e => this.onClick(e, false));
    
    // Give UI a chance to settle before rendering.
    this.window.setTimeout(() => {
      this.render();
    }, 1);
  }
  
  setup(harmonics) {
    if (harmonics) this.harmonics = [...harmonics];
    else this.harmonics = [];
  }
  
  render() {
    const bounds = this.element.getBoundingClientRect();
    this.element.width = bounds.width;
    this.element.height = bounds.height;
    const ctx = this.element.getContext('2d');
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    const barw = Math.ceil(bounds.width / BAR_COUNT);
    for (let i=0; i<BAR_COUNT; i++) {
      const x = i * barw;
      if (i & 1) { // background stripe for the odd columns
        ctx.fillStyle = "#111";
        ctx.fillRect(x, 0, barw, bounds.height);
      }
      const v = this.harmonics[i] || 0;
      if (v <= 0) continue;
      const subh = Math.ceil(v * bounds.height);
      ctx.fillStyle = "#f00";
      ctx.fillRect(x+1, bounds.height - subh, barw-2, subh);
    }
  }
  
  onClick(event, positive) {
    event.stopPropagation();
    event.preventDefault();
    const bounds = this.element.getBoundingClientRect();
    const x = event.clientX - bounds.x;
    const y = event.clientY - bounds.y;
    const barw = Math.ceil(bounds.width / BAR_COUNT);
    const p = Math.floor(x / barw);
    if ((p < 0) || (p >= BAR_COUNT)) return;
    while (this.harmonics.length <= p) this.harmonics.push(0);
    if (positive) {
      const v = 1 - y / bounds.height;
      this.harmonics[p] = v;
    } else {
      this.harmonics[p] = 0;
    }
    this.render();
    this.dirty(this.harmonics);
  }
}
