/* egg.js
 * Egg web runner, bootstrap.
 * If you're copying this for distribution, be sure to take everything under "./js/" too.
 */
 
import { Rom } from "./js/Rom.js";
import { Runtime } from "./js/Runtime.js";

let runningRuntime = null;
let haveEmbeddedRom = false;

function startEgg(serial, fileName, canvasId) {
  const canvas = document.getElementById(canvasId);
  if (!canvas || (canvas.tagName !== "CANVAS")) {
    throw new Error(`eggCanvasId ${JSON.stringify(canvasId)} is not a <canvas>`);
  }
  if (runningRuntime) {
    runningRuntime.terminate();
    runningRuntime = null;
  }
  const rom = new Rom(serial, fileName);
  const rt = new Runtime(rom, canvas, window);
  runningRuntime = rt;
  window.egg = rt.getClientExports();
  rt.run().then(() => {
  }).catch(e => {
    console.error(`Failed to initialize platform.`, e);
    runningRuntime = null;
  });
}

function decodeBase64(src) {
  let dst = new Uint8Array(65536);
  let dstc = 0;
  const intake = []; // (0..63) up to 4
  for (let i=0; i<src.length; i++) {
    const srcch = src.charCodeAt(i);
    if (srcch <= 0x20) continue;
    if (srcch === 0x3d) continue; // =
    if ((srcch >= 0x41) && (srcch <= 0x5a)) intake.push(srcch - 0x41);
    else if ((srcch >= 0x61) && (srcch <= 0x7a)) intake.push(srcch - 0x61 + 26);
    else if ((srcch >= 0x30) && (srcch <= 0x39)) intake.push(srcch - 0x30 + 52);
    else if (srcch === 0x2b) intake.push(62); // +
    else if (srcch === 0x2f) intake.push(63); // /
    else throw new Error(`Unexpected byte ${srcch} in base64 rom`);
    if (intake.length >= 4) {
      if (dstc >= dst.length - 3) {
        const ndst = new Uint8Array(dst.length << 1);
        ndst.set(dst);
        dst = ndst;
      }
      dst[dstc++] = (intake[0] << 2) | (intake[1] >> 4);
      dst[dstc++] = (intake[1] << 4) | (intake[2] >> 2);
      dst[dstc++] = (intake[2] << 6) | intake[3];
      intake.splice(0, 4);
    }
  }
  if (intake.length > 0) {
    if (dstc >= dst.length - 2) {
      const ndst = new Uint8Array(dst.length + 2);
      ndst.set(dst);
      dst = ndst;
    }
    switch (intake.length) {
      case 1: dst[dstc++] = intake[0] << 2; break;
      case 2: dst[dstc++] = (intake[0] << 2) | (intake[1] >> 4); break;
      case 3: dst[dstc++] = (intake[0] << 2) | (intake[1] >> 4); dst[dstc++] = (intake[1] << 4) | (intake[2] >> 2); break;
    }
  }
  if (dstc < dst.length) {
    const ndst = new Uint8Array(dstc);
    const srcview = new Uint8Array(dst.buffer, 0, dstc);
    ndst.set(srcview);
    dst = ndst;
  }
  return dst.buffer;
}

window.addEventListener("load", () => {
  const romElement = document.getElementById("eggEmbeddedRom");
  if (romElement) {
    haveEmbeddedRom = true;
    const serial = decodeBase64(romElement.innerText);
    startEgg(serial, "embedded", "eggCanvas");
  }
});

window.addEventListener("message", e => {
  if (haveEmbeddedRom) return;
  if (!e.data?.eggRunSerial) return;
  startEgg(e.data.eggRunSerial, e.data.eggFileName, e.data.eggCanvasId);
});
