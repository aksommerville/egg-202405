/* EnvUi.js
 * General envelope editor.
 */
 
const RENDER_TIMEOUT_MS = 10; // Just enough to kick out of this execution frame; 0 would probly be ok too.
const RIGHT_SLOP_WIDTH_MS = 100; // Dead space on the right side, so you can drag and expand the range.
 
export class EnvUi {
  static getDependencies() {
    return [HTMLCanvasElement, Window];
  }
  constructor(element, window) {
    this.element = element;
    this.window = window;
    
    this.range = 1; // Owner may replace.
    this.valueIsHertz = false; // True to treat value as hz and display on a more useful log scale.
    this.onChanged = () => {};
    
    this.points = []; // {t:ms,v:0..1} Times are absolute, values are always normalized.
    this.renderTimeout = null;
    this.dragIndex = -1;
    
    this.element.addEventListener("mousedown", e => this.onMouseDown(e));
    this.element.addEventListener("contextmenu", e => e.preventDefault()); // mousedown preventDefault isn't sufficient
    this.mouseListener = null; // For mouseup and mousemove, attached to window.
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
    }
    this.dropMouseListener();
  }
  
  setup(words) {
    this.points = [];
    this.points.push({ t:0, v: (+words[0] || 0) / this.range });
    for (let p=1, t=0; p<words.length; ) {
      const delay = +words[p++] || 0;
      const v = (+words[p++] / this.range) || 0;
      t += delay;
      if (t < 1) t = 1; // t==0 is reserved for the first point, important, we won't allow t to change if zero.
      this.points.push({ t, v });
    }
    this.renderSoon();
  }
  
  encode() {
    if (!this.points.length) return ""; // Definitely unused if we don't have the dummy zero point (setup was never called).
    let encv;
    if (this.range >= 65535) { // Values end up 16-bit. If range is this high, output integers only.
      encv = v => Math.min(65535, Math.floor(v * this.range));
    } else { // Floating-point values in text.
      encv = v => v * this.range;
    }
    let dst = encv(this.points[0].v);
    for (let i=1, t=0; i<this.points.length; i++) {
      const pt = this.points[i];
      dst += ` ${pt.t-t} ${encv(pt.v)}`;
      t = pt.t;
    }
    return dst;
  }
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, RENDER_TIMEOUT_MS);
  }
  
  renderNow() {
    const bounds = this.element.getBoundingClientRect();
    this.element.width = bounds.width;
    this.element.height = bounds.height;
    const ctx = this.element.getContext("2d");
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    
    // If we're log-scaled, draw guidelines an octave apart. The phase of these lines isn't important.
    if (this.valueIsHertz) {
      ctx.beginPath();
      for (let f=20; f<22000; f*=2) {
        const y = this.yForV(f / 65536, bounds.height);
        ctx.moveTo(0, y);
        ctx.lineTo(bounds.width, y);
      }
      ctx.strokeStyle = "#111";
      ctx.stroke();
    }
    
    if (this.points.length > 1) {
    
      // Trace the envelope.
      const trange = this.points[this.points.length - 1].t + RIGHT_SLOP_WIDTH_MS;
      ctx.beginPath();
      ctx.moveTo(0, this.yForV(this.points[0].v, bounds.height));
      for (const { t, v } of this.points) {
        const x = (t * bounds.width) / trange;
        const y = this.yForV(v, bounds.height);
        ctx.lineTo(x, y);
      }
      ctx.strokeStyle = "#aaa";
      ctx.stroke();
      
      // Draw a big handle on each point.
      const radius = 4;
      ctx.fillStyle = "#f80";
      for (const { t, v } of this.points) {
        const x = (t * bounds.width) / trange;
        const y = this.yForV(v, bounds.height);
        ctx.beginPath();
        ctx.ellipse(x, y, radius, radius, 0, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }
  
  yForV(v, h) {
    if (this.valueIsHertz) {
      // We'll try to display only values in 20..22k, and on a base-2 log scale so octaves have a uniform height.
      v *= 65536;
      if (v > 22000) v = 22000;
      else if (v < 20) v = 20;
      v = (Math.log2(v) - 4.3) / (14.5 - 4.3);
      return h - v * h;
    } else {
      return h - v * h;
    }
  }
  
  vForY(y, h) {
    y = (h - y - 1) / h;
    if (this.valueIsHertz) {
      const hz = Math.min(65535, Math.max(1, Math.pow(2, (y * (14.5 - 4.3)) + 4.3)));
      return hz / 65536.0;
    }
    return y;
  }
  
  dropMouseListener() {
    if (!this.mouseListener) return;
    this.window.removeEventListener("mouseup", this.mouseListener);
    this.window.removeEventListener("mousemove", this.mouseListener);
    this.mouseListener = null;
    this.dragIndex = -1;
  }
  
  onMouseMoveOrUp(event) {
    if ((event.type === "mouseup") || (this.dragIndex < 0) || (this.dragIndex >= this.points.length)) {
      this.dropMouseListener();
      this.onChanged();
      return;
    }
    const bounds = this.element.getBoundingClientRect();
    const x = event.x - bounds.x;
    const y = event.y - bounds.y;
    const point = this.points[this.dragIndex];
    if (point.t) {
      const trange = this.points[this.points.length - 1].t + RIGHT_SLOP_WIDTH_MS;
      point.t = Math.max(1, Math.round((x * trange) / bounds.width));
      if ((this.dragIndex > 0) && (point.t <= this.points[this.dragIndex - 1].t)) {
        point.t = this.points[this.dragIndex - 1].t + 1;
      } else if ((this.dragIndex < this.points.length - 1) && (point.t >= this.points[this.dragIndex + 1].t)) {
        point.t = this.points[this.dragIndex + 1].t - 1;
      }
    }
    point.v = Math.max(0, Math.min(1, this.vForY(y, bounds.height)));
    this.renderSoon();
  }
  
  onMouseDown(event) {
    event.stopPropagation();
    event.preventDefault();
    const bounds = this.element.getBoundingClientRect();
    const x = event.x - bounds.x;
    const y = event.y - bounds.y;
    
    // Convert our logical points into canvas coords like (x,y), and maintain order.
    const radius = 4;
    const trange = this.points[this.points.length - 1].t + RIGHT_SLOP_WIDTH_MS;
    const points = this.points.map(pt => [
      (pt.t * bounds.width) / trange,
      this.yForV(pt.v, bounds.height),
    ]);
    
    // If we're inside a point, begin dragging it. (or delete it, if it was a right-click)
    this.dragIndex = -1;
    for (let i=points.length; i-->0; ) {
      const point = points[i];
      const dx = point[0] - x;
      if ((dx < -radius) || (dx > radius)) continue;
      const dy = point[1] - y;
      if ((dy < -radius) || (dy > radius)) continue;
      
      if (event.button === 2) {
        if (!i) return; // Can't delete point zero; it's not a real point.
        this.points.splice(i, 1);
        this.renderSoon();
        this.onChanged();
        return;
      }
      this.dragIndex = i;
      break;
    }
    
    // If we didn't find a point, add one.
    if (this.dragIndex < 0) {
      const t = Math.max(1, Math.round((x * trange) / bounds.width));
      for (let i=1; i<this.points.length; i++) {
        if (t <= this.points[i].t) {
          this.dragIndex = i;
          this.points.splice(i, 0, {
            t,
            v: this.vForY(y, bounds.height),
          });
          break;
        }
      }
      if (this.dragIndex < 0) {
        this.dragIndex = this.points.length;
        this.points.push({
          t,
          v: this.vForY(y, bounds.height),
        });
      }
    }
    
    this.mouseListener = e => this.onMouseMoveOrUp(e);
    this.window.addEventListener("mouseup", this.mouseListener);
    this.window.addEventListener("mousemove", this.mouseListener);
    this.onMouseMoveOrUp(event);
  }
}
