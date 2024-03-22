# Egg

Games platform designed to work like an emulator: The game is a single file that runs in a controlled environment.

Game code can be in JavaScript or WebAssembly.

## TODO

- [x] OpenGL for Javascript.
- - Match webgl exactly, and add a context parameter to egg_client_render for js.
- [x] Try EGL instead of GLX. We seem to not have real ES on the Wasm side. (try point sprites)
- [x] Failing to launch with drmgx (permissions error at drmModeSetCrtc)...?
- [ ] New bespoke render API. GLES2/WebGL is proving too complicated.
- - [x] Native
- - [ ] Web (WebGL)
- - [ ] Web (CanvasRenderingContext2D). Are there browsers that don't support WebGL?
- [ ] Test each entry point in Javascript.
- [ ] Test each entry point in WebAssembly.
- [ ] Test each entry point natively.
- [ ] Web runtime.
- [ ] MacOS drivers.
- [ ] Windows drivers.
- [ ] Preferred language for Mac and Windows.
- [ ] Command line param to override language.
- [ ] Synthesizer.
- [ ] User-supplied HTTP permissions.
- [ ] User-supplied storage permissions.
- [ ] Can we do some kind of "warp cursor" or "trap cursor"? First-person shooters really need that.
- [ ] Validate size of wasm gl parameters, some are probably off.
- [x] glGetVertexAttribPointerv wasm needs rephrased; we can't pass a double-pointer in from the wasm space.
- [ ] Ensure that egg_native_export.c gets dropped by tree-shaking when linking native.
- [x] Pull the WebGL stuff out of egg_native_export.c, just for cleanliness's sake. Way more of it than I expected.
- [ ] QuickJS JS_GetArrayBuffer: Does this read TypedArray, SharedArrayBuffer, and DataView as well?
- [ ] server: Capture make output and deliver to client on errors.
- [ ] server: Non-blocking make.
- [x] Remove JS "import egg", use a global instead. See src/web/index.html
- [ ] egg_native_event.c: I'm thinking RESIZE should not be required. Made it optional in JS API.
- [ ] Terminating game in web, we probably ought to restore all state. Canvas size, page title, favicon, canvas cursor visibility.
- [ ] Accelerometer input.
- [ ] If we're sticking with a renderer-managed main framebuffer, remove RESIZE events and egg_video_get_size().
