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
- [x] Web runtime.
- [ ] Additional native platforms.
- - [ ] MacOS.
- - [ ] Windows.
- - [ ] Preferred language for Mac and Windows.
- [ ] Call JS from Wasm and vice-versa.
- [x] Command line param to override language.
- [ ] Synthesizer.
- [ ] User-supplied HTTP permissions.
- [ ] User-supplied storage permissions.
- [ ] Ensure that egg_native_export.c gets dropped by tree-shaking when linking native.
- [x] QuickJS JS_GetArrayBuffer: Does this read TypedArray, SharedArrayBuffer, and DataView as well? ...NO
- [ ] server: Capture make output and deliver to client on errors.
- [ ] server: Non-blocking make.
- [x] egg_native_event.c: I'm thinking RESIZE should not be required. Made it optional in JS API. ...actually, I'm deleting that event.
- [x] Terminating game in web, we probably ought to restore all state. Canvas size, page title, favicon, canvas cursor visibility.
- [ ] Accelerometer input.
- [x] If we're sticking with a renderer-managed main framebuffer, remove RESIZE events and egg_video_get_size().
- [x] eggrom: Check for 1-bit images encoded at a higher depth, output as 1-bit.
