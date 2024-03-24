/* Audio.js
 * Implements our synthesizer and exposes the public API.
 */
 
export class Audio {
  constructor(window, rom) {
    this.window = window;
    this.rom = rom;
  }
  
  update() {
    //TODO
  }
  
  stop() {
    //TODO
  }
  
  /* Public API.
   *****************************************************************/
  
  audio_play_song(qual, songid, force, repeat) {
    console.log(`TODO Audio.audio_play_song(${qual},${songid},${force},${repeat})`);
  }
  
  audio_play_sound(qual, soundid, trim, pan) {
    console.log(`TODO Audio.audio_play_sound(${qual},${soundid},${trim},${pan})`);
  }
  
  audio_get_playhead() {
    return -1;//TODO
  }
}
