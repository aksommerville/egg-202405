/* PlaybackService.js
 */
 
export class PlaybackService {
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
    
    this.recentWave = null;
    
    this.context = null;
    this.rate = 44100; // TODO Configurable?
  }
  
  /* Don't create the AudioContext at construction.
   * It will work and everything, but we get an annoying warning about user hasn't interacted with the page yet.
   */
  requireContext() {
    if (this.context) return true;
    if (!this.window.AudioContext) return false;
    this.context = new this.window.AudioContext({
      sampleRate: this.rate,
      latencyHint: "interactive",
    });
    if (this.context.state === "suspended") {
      this.context.resume();
    }
    return true;
  }
  
  // Float32Array or Int16Array
  playWave(wave) {
    if (!wave) wave = this.recentWave;
    if (!wave) return;
    this.recentWave = wave;
    if (!this.requireContext()) return;
    
    let wave32;
    if (wave instanceof Float32Array) {
      wave32 = wave;
    } else if (wave instanceof Int16Array) {
      wave32 = new Float32Array(wave.length);
      for (let i=wave.length; i-->0; ) wave32[i] = wave[i] / 32768.0;
    } else {
      throw new Error(`Expected Float32Array or Int16Array`);
    }
    
    const audioBuffer = new AudioBuffer({
      length: wave32.length,
      numberOfChannels: 1,
      sampleRate: this.rate,
      channelCount: 1,
    });
    audioBuffer.copyToChannel(wave32, 0);
    const node = new AudioBufferSourceNode(this.context, {
      buffer: audioBuffer,
      channelCount: 1,
    });
    node.connect(this.context.destination);
    node.start(0);
  }
}

PlaybackService.singleton = true;
