/* EnvUi.js
 * General envelope editor.
 */
 
import { Bus } from "./Bus.js";
 
const RENDER_TIMEOUT_MS = 10; // Just enough to kick out of this execution frame; 0 would probly be ok too.
const HANDLE_RADIUS = 6;
const LOG_FLOOR = 20;
 
export class EnvUi {
  static getDependencies() {
    return [HTMLCanvasElement, Window, Bus];
  }
  constructor(element, window, bus) {
    this.element = element;
    this.window = window;
    this.bus = bus;
    
    this.onChanged = () => {};
    
    this.env = null;
    this.range = 1;
    this.logscale = false;
    this.loglo = 1;
    this.loghi = 2;
    this.renderTimeout = null;
    this.dragPoint = null; // One of (env.points), "init", or null.
    this.tlo = 0; // while dragging
    this.thi = 0;
    
    this.element.addEventListener("mousedown", e => this.onMouseDown(e));
    this.element.addEventListener("contextmenu", e => e.preventDefault()); // mousedown preventDefault isn't sufficient
    this.mouseListener = null; // For mouseup and mousemove, attached to window.
    
    this.busListener = this.bus.listen(e => this.onBusEvent(e));
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
    }
    this.dropMouseListener();
    this.bus.unlisten(this.busListener);
  }
  
  setup(env, range, logscale) {
    this.env = env;
    this.range = range || 1;
    this.logscale = logscale || false;
    if (this.logscale) {
      this.loglo = Math.log2(LOG_FLOOR);
      this.loghi = Math.log2(this.range);
    }
    this.renderSoon();
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
    
    // Guidelines to show time scale.
    for (const [interval, color] of [
      [62.5, "#080808"],
      [ 250, "#101010"],
      [1000, "#202020"],
    ]) {
      ctx.beginPath();
      for (let t=interval; t<this.bus.timeRange; t+=interval) {
        const x = this.xFromT(t);
        ctx.moveTo(x, 0);
        ctx.lineTo(x, bounds.bottom);
      }
      ctx.strokeStyle = color;
      ctx.stroke();
    }
    
    // When working in log scale, draw guidelines one octave apart.
    if (this.logscale) {
      ctx.beginPath();
      for (let hz=25; ; hz*=2) {
        const y = this.yFromV(hz);
        if (y <= 0) break;
        ctx.moveTo(0, y);
        ctx.lineTo(bounds.width, y);
      }
      ctx.strokeStyle = "#111";
      ctx.stroke();
    }
    
    if (!this.env) return;
    const points = this.env.points.map(({t, v}) => [
      this.xFromT(t),
      this.yFromV(v),
    ]);
    
    ctx.beginPath();
    let lasty;
    ctx.moveTo(0, lasty = this.yFromV(this.env.init));
    for (const [x, y] of points) ctx.lineTo(x, lasty = y);
    ctx.lineTo(bounds.width, lasty);
    ctx.strokeStyle = "#a50";
    ctx.stroke();
    
    ctx.fillStyle = "#ff0";
    for (const [x, y] of points) {
      ctx.beginPath();
      ctx.ellipse(x, y, HANDLE_RADIUS, HANDLE_RADIUS, 0, Math.PI * 2, false);
      ctx.fill();
    }
    ctx.beginPath();
    ctx.ellipse(0, this.yFromV(this.env.init), HANDLE_RADIUS, HANDLE_RADIUS, 0, Math.PI * 2, false);
    ctx.fill();
  }
  
  /* Coordinate transforms.
   **************************************************/
  
  xFromT(t) {
    return (t * this.element.width) / this.bus.timeRange;
  }
  
  tFromX(x) {
    return Math.round((x * this.bus.timeRange) / this.element.width);
  }
  
  yFromV(v) {
    if (this.logscale) {
      if (v <= LOG_FLOOR) return this.element.height;
      const log = Math.log2(v);
      const norm = (log - this.loglo) / (this.loghi - this.loglo);
      return (1 - norm) * this.element.height;
    }
    return this.element.height - (v * this.element.height) / this.range;
  }
  
  vFromY(y) {
    if (this.logscale) {
      const norm = (this.element.height - y) / this.element.height;
      const log = this.loglo + norm * (this.loghi - this.loglo);
      const pow = Math.pow(2, log);
      return pow;
    }
    return ((this.element.height - y) * this.range) / this.element.height;
  }
  
  /* Events.
   *********************************************************************/
  
  dropMouseListener() {
    if (this.mouseListener) {
      this.window.removeEventListener("mouseup", this.mouseListener);
      this.window.removeEventListener("mousedown", this.mouseListener);
      this.mouseListener = null;
    }
    this.dragPoint = null;
  }
  
  onMouseUpOrMove(event) {
    if (event.type === "mouseup") {
      this.dropMouseListener();
      return;
    }
    if (!this.env || !this.dragPoint) return;
    const bounds = this.element.getBoundingClientRect();
    const x = event.clientX - bounds.x;
    const y = event.clientY - bounds.y;
    const v = Math.max(0, Math.min(this.range, this.vFromY(y)));
    if (this.dragPoint === "init") {
      this.env.init = v;
    } else {
      const t = Math.max(this.tlo, Math.min(this.thi, this.tFromX(x)));
      this.dragPoint.t = t;
      this.dragPoint.v = v;
    }
    this.onChanged();
    this.renderSoon();
  }
  
  onMouseDown(event) {
    if (this.mouseListener) return;
    event.preventDefault();
    event.stopPropagation();
    if (!this.env) return;
    
    const bounds = this.element.getBoundingClientRect();
    const x = event.clientX - bounds.x;
    const y = event.clientY - bounds.y;
    
    // First check if we clicked on a handle, in screen coordinates only.
    let dragPoint = null;
    for (const point of this.env.points) {
      const dx = x - this.xFromT(point.t);
      if ((dx < -HANDLE_RADIUS) || (dx > HANDLE_RADIUS)) continue;
      const dy = y - this.yFromV(point.v);
      if ((dy < -HANDLE_RADIUS) || (dy > HANDLE_RADIUS)) continue;
      dragPoint = point;
    }
    
    // Got a handle, and it's a right click? Delete the point and we're done.
    if (dragPoint && (event.button === 2)) {
      const p = this.env.points.indexOf(dragPoint);
      this.env.points.splice(p, 1);
      this.onChanged();
      this.renderSoon();
      return;
    }
    
    // Any other right click is noop.
    if (event.button !== 0) return;
    
    // If no point yet, check the imaginary init point.
    if (!dragPoint) {
      if (x <= HANDLE_RADIUS) {
        const dy = y - this.yFromV(this.env.init);
        if ((dy >= -HANDLE_RADIUS) && (dy <= HANDLE_RADIUS)) {
          dragPoint = "init";
        }
      }
    }
    
    // Still no point? We'll add one.
    if (!dragPoint) {
      const mouset = this.tFromX(x);
      const mousev = this.vFromY(y);
      let insp = this.env.points.length;
      for (let i=0; i<this.env.points.length; i++) {
        if (this.env.points[i].t >= mouset) {
          insp = i;
          break;
        }
      }
      dragPoint = { t: mouset, v: mousev };
      this.env.points.splice(insp, 0, dragPoint);
    }

    this.dragPoint = dragPoint;
    if (this.dragPoint === "init") {
      this.tlo = 0;
      this.thi = 0;
    } else {
      const p = this.env.points.indexOf(this.dragPoint);
      if (p < 0) return;
      if (p > 0) this.tlo = this.env.points[p-1].t + 1;
      else this.tlo = 1;
      if (p < this.env.points.length - 1) this.thi = this.env.points[p+1].t - 1;
      else this.thi = this.bus.timeRange;
      if (this.tlo > this.thi) return;
    }
    
    this.mouseListener = e => this.onMouseUpOrMove(e);
    this.window.addEventListener("mouseup", this.mouseListener);
    this.window.addEventListener("mousemove", this.mouseListener);
    this.onMouseUpOrMove(event);
  }
  
  onBusEvent(event) {
    switch (event.type) {
      case "setTimeRange": this.renderSoon(); break;
    }
  }
}
