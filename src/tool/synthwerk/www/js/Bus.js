/* Bus.js
 * Event dispatcher and global store.
 * Anyone can dispatch anything.
 * Recording them here but it's not gospel.
 *
 * { type:"play" }
 *   Request to play the current sound. From click in WaveVisualUi.
 *
 * { type:"newSound" }
 *   Request to delete the current model and start a fresh one.
 *   Replaces the global model stored here.
 *
 * { type:"showText" }
 *   Request text modal.
 *
 * { type:"showHex" }
 *   Request hexdump modal.
 *
 */
 
import { Sound } from "./Sound.js";
 
export class Bus {
  static getDependencies() {
    return [];
  }
  constructor() {
    this.listeners = [];
    this.nextListenerId = 1;
    
    // Global store.
    this.sound = new Sound(); // Could reinstantiate as needed, but it's easy to keep reusing the same instance.
    this.text = null; // string
    this.bin = null; // Uint8Array
    this.wave = null; // Float32Array
    this.timeRange = 1000; // ms, horizontal range of all envelopes, and effectively the duration limit.
    this.rate = 44100;
  }
  
  listen(cb) {
    const id = this.nextListenerId++;
    this.listeners.push({ cb, id });
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p < 0) return;
    this.listeners.splice(p, 1);
  }
  
  dispatch(event) {
    switch (event.type) {
      case "newSound": this.newSound(); break;
    }
    for (const { cb } of this.listeners) cb(event);
  }
  
  /* Global store.
   *************************************************************/
   
  newSound() {
    this.sound.clear();
    this.text = null;
    this.bin = null;
    this.wave = null;
    this.timeRange = 1000;
  }
  
  soundDirty() {
    this.text = null;
    this.bin = null;
    this.wave = null;
  }
  
  requireText() {
    if (!this.text) this.text = this.sound.encodeText();
    return this.text;
  }
  
  requireBin() {
    if (!this.bin) this.bin = this.sound.encodeBinary();
    return this.bin;
  }
  
  // Beware, can remain null if printing fails.
  requireWave() {
    //if (!this.wave) this.wave = Pcmprint.print(this.requireBin(), this.rate);
    return this.wave;
  }
}

Bus.singleton = true;
