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
- [ ] GUI for pcmprint. Build as a web app, in synthwerk, at least for now.
- [ ] Assess SfgPrinter performance, opportunities for optimization here if needed.
- [ ] Synthesizer, native.
- - [ ] GM instruments.
- - [x] GM drums.
- [ ] Synthesizer, web.
- - [ ] fx: detune
- - [ ] Pitch wheel
- - [ ] Readhead should wrap around on song repeats.
- [ ] Timing in MIDI conversion is still fucked up. See nearer-the-sky, they go out of sync toward the end.
- [ ] User-supplied HTTP permissions.
- [ ] User-supplied storage permissions.
- [ ] Ensure that egg_native_export.c gets dropped by tree-shaking when linking native.
- [ ] Accelerometer input.
- [ ] egg_audio_get_playhead(native): Estimate driver's buffer position.
- [ ] x11 app icon broken, after i changed from GIF to ICO
- - [ ] Our ico and bmp decoder are all kinds of wrong.
- [ ] Web: Detect low frame rate and pause. Or at least kill the audio. (eg when browser window goes into background)

