/* egg.js
 * Egg web runner, bootstrap.
 * If you're copying this for distribution, be sure to take everything under "./js/" too.
 */
 
import { Rom } from "./js/Rom.js";
import { Runtime } from "./js/Runtime.js";
 
window.addEventListener("message", e => {
  if (!e.data?.eggRunSerial) return;
  const canvas = document.getElementById(e.data.eggCanvasId);
  if (!canvas || (canvas.tagName !== "CANVAS")) {
    throw new Error(`eggCanvasId ${JSON.stringify(e.data.eggCanvasId)} is not a <canvas>`);
  }
  const rom = new Rom(e.data.eggRunSerial, e.data.eggFileName);
  const rt = new Runtime(rom, canvas, window);
  rt.run();
});
