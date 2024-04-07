/* RootUi.js
 * Top of our view controller hierarchy.
 */
 
import { Dom } from "./Dom.js";
import { Bus } from "./Bus.js";
import { InputUi } from "./InputUi.js";
import { OutputUi } from "./OutputUi.js";
import { PlaybackService } from "./PlaybackService.js";//XXX

//XXX TEMP trying out SfgPrinter and SfgCompiler
import { SfgPrinter } from "./SfgPrinter.js";
import { SfgCompiler } from "./SfgCompiler.js";
const multifile = `
sound 35 # B1 Acoustic Bass Drum
  master 0.250
  shape sine
  harmonics 1 0 0.400 0 0.100 0 0.040
  fm 1 1
  fmenv 0 100 1 2000 0
  rate 440 30 220 200 440
  ratelfo 3 100
  level 0 30 1 50 0.200 2000 0
end

sound 36 # C2 Bass Drum 1
  master 0.200
  fm 0.5 3
  fmenv 0 20 1 20 0.5 600 0
  rate 150 150 50
  level 0.000 20 1.000 20 0.500 800 0.000
end

sound 37 # C#2 Side Stick
  master 0.100
  shape noise
  level 0.000 10 0.999 10 0.000
  delay 80 0.5 0.5 0.5 0.8
  level 0.000 1 1.000 400 1.000 1000 0.000
end
`;
const singlefile = `
  master 0.250
  shape sine
  harmonics 1 0 0.400 0 0.100 0 0.040
  fm 1 1
  fmenv 0 100 1 2000 0
  rate 440 30 220 200 440
  ratelfo 3 100
  level 0 30 1 50 0.200 2000 0
`;

export class RootUi {
  static getDependencies() {
    return [HTMLElement, Dom, Bus, PlaybackService];
  }
  constructor(element, dom, bus, playbackService) {
    this.element = element;
    this.dom = dom;
    this.bus = bus;
    this.playbackService = playbackService;
    
    this.inputUi = null;
    this.outputUi = null;
    this.busListener = this.bus.listen(e => this.onBusEvent(e));
    
    this.buildUi();
    
    this.element.addEventListener("click", () => this.XXX_test());
  }
  
  XXX_test() {
    console.log(`sfg test...`);
    try {
      const sounds = new SfgCompiler(multifile, "multifile").compileAll();
      console.log(`compiled multifile`, {...sounds});
      this.XXX_playAndDepleteSounds(sounds);
    } catch (e) {
      console.error(e);
    }
  }
  XXX_playAndDepleteSounds(sounds) {
    const k = Object.keys(sounds)[0];
    if (!k) {
      console.log(`End of playback`);
      return;
    }
    const bin = sounds[k];
    delete sounds[k];
    const wave = new SfgPrinter(bin, this.playbackService.rate).print();
    console.log(`Play sound ${JSON.stringify(k)}`);
    this.playbackService.playWave(wave);
    window.setTimeout(() => {
      this.XXX_playAndDepleteSounds(sounds);
    }, 1000);
  }
  
  onRemoveFromDom() {
    this.bus.unlisten(this.busListener);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.inputUi = this.dom.spawnController(this.element, InputUi);
    this.outputUi = this.dom.spawnController(this.element, OutputUi);
  }
  
  onBusEvent(event) {
    console.log(`RootUi.onBusEvent`, event);
    switch (event.type) {
    }
  }
}
