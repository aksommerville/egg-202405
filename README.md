# Egg

Games platform designed to work like an emulator: The game is a single file that runs in a controlled environment.

Game code can be in JavaScript or WebAssembly.

Video and audio, we provide a small opinionated API.
Input is more agnostic, tries to give you raw events as much as possible.

## TODO

### High Priority

- [ ] Lights-on app to manually validate all platform features.
- - [ ] Test each entry point in Javascript.
- - [ ] Test each entry point in WebAssembly.
- - [ ] Test each entry point natively. (Same as the wasm test, just build differently)
- [ ] Additional native platforms.
- - [ ] MacOS.
- - [ ] Windows.
- - [ ] Preferred language for Mac and Windows.
- [ ] Call JS from Wasm and vice-versa.
- [ ] User-supplied HTTP permissions.
- [ ] User-supplied storage permissions.
- [ ] Accelerometer input.
- [ ] softrender: Update (encfmt) with alpha hint when an image is used as destination, and initialize at decode.
- [ ] softrender: Tint and global alpha. I think would be best to implement as blenders at softrender_draw.c, not sure.
- [ ] web: We don't seem to be getting click events

### Low Priority

- [ ] Assess SfgPrinter performance, opportunities for optimization here if needed.
- [ ] Synthesizer, native.
- - [ ] GM instruments.
- [ ] Synthesizer, web.
- - [ ] fx: detune
- - [ ] Pitch wheel
- - [ ] Readhead should wrap around on song repeats.
- [ ] Ensure that egg_native_export.c gets dropped by tree-shaking when linking native.
- [ ] egg_audio_get_playhead(native): Estimate driver's buffer position.
- [ ] Web: Detect low frame rate and pause. Or at least kill the audio. (eg when browser window goes into background)
- [ ] render and softrender currently both require upload stride to be provided explicitly. Should we loosen that? Maybe allow (stride==0) for "oh come on, you know what i mean".
- [ ] Do I need to forbid rendering from a texture onto itself? My heart says yes, but we're not forbidding it yet.
- [ ] Mouse coords when softrender in play. Will require a new hostio_video hook i think.
- [ ] Mouse and touch coords in web. Must account for scaling (do they already?)
