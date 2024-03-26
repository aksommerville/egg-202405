# Egg

Games platform designed to work like an emulator: The game is a single file that runs in a controlled environment.

Game code can be in JavaScript or WebAssembly.

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
- [ ] Synthesizer, web.
- [ ] User-supplied HTTP permissions.
- [ ] User-supplied storage permissions.
- [ ] Ensure that egg_native_export.c gets dropped by tree-shaking when linking native.
- [x] server: Capture make output and deliver to client on errors.
- [x] server: Non-blocking make.
- [ ] Accelerometer input.
- [ ] egg_audio_get_playhead(native): Estimate driver's buffer position.
- [x] Store tempo in songs, and reshape egg_audio_get_playhead().
- [x] midi file: tracks are going out of sync over time. Evident in 3-toil_and_trouble
- [x] Actually, tempos are haywire all over the place! Compare 1-tangled_vine (fast) to 13-nearer_the_sky
- [ ] x11 app icon broken, after i changed from GIF to ICO
