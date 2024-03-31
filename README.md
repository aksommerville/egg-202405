# Egg

Games platform designed to work like an emulator: The game is a single file that runs in a controlled environment.

Game code can be in JavaScript or WebAssembly.

Video and audio, we provide a small opinionated API.
Input is more agnostic, tries to give you raw events as much as possible.

## TODO

- [ ] Alternate render implementations.
- - [ ] Web (CanvasRenderingContext2D). Are there browsers that don't support WebGL? ...no important ones.
- - [ ] Native (soft)
- [ ] Lights-on app to manually validate all platform features.
- - [ ] Test each entry point in Javascript.
- - [ ] Test each entry point in WebAssembly.
- - [ ] Test each entry point natively. (Same as the wasm test, just build differently)
- [ ] Additional native platforms.
- - [ ] MacOS.
- - [ ] Windows.
- - [ ] Preferred language for Mac and Windows.
- [ ] Call JS from Wasm and vice-versa.
- [ ] Synthesizer, native.
- - [ ] GM instruments.
- - [ ] GM drums.
- - [x] pcmprint
- [ ] Synthesizer, web.
- - [ ] Generate instrument config from src/opt/synth/synth_builtin.c
- - [ ] fx: detune
- - [ ] fx: FM LFO
- - [ ] Pitch wheel
- - [ ] Readhead should wrap around on song repeats.
- [ ] User-supplied HTTP permissions.
- [ ] User-supplied storage permissions.
- [ ] Ensure that egg_native_export.c gets dropped by tree-shaking when linking native.
- [ ] Accelerometer input.
- [ ] egg_audio_get_playhead(native): Estimate driver's buffer position.
- [ ] x11 app icon broken, after i changed from GIF to ICO
- - [ ] Our ico and bmp decoder are all kinds of wrong.
- [ ] Web: Detect low frame rate and pause. Or at least kill the audio. (eg when browser window goes into background)

Rethink audio. It's getting too complicated.
- Can I do WebAudio-like pluggable nodes on the C side?
- Integrate PCM printing with synth, there's tons of overlap.
- - But then how will it work in web?
- - - Can we run a second AudioContext and capture its output? YES: OfflineAudioContext.
- I don't want to get stuck on audio forever. Left some thoughts in src/opt/synth2, but will proceed with the existing ones.
