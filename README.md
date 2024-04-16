# Egg

Games platform designed to work like an emulator: The game is a single file that runs in a controlled environment.

Game code can be in JavaScript or WebAssembly.
Build games to a portable Egg file, or bundle with Egg runtime for a fake-native executable.
Games written in C can also be built true-native.

Video and audio, we provide a small opinionated API.
Input is more agnostic, tries to give you raw events as much as possible.

## TODO

### High Priority

- [ ] Additional native platforms.
- - [ ] MacOS.
- - [ ] Windows.
- - [ ] Preferred language for Mac and Windows.
- [ ] User-supplied HTTP permissions.
- [ ] User-supplied storage permissions.
- [ ] Accelerometer input.
- - [ ] Update lojs
- - [ ] Update lowasm
- [ ] softrender: Update (encfmt) with alpha hint when an image is used as destination, and initialize at decode.
- [ ] softrender: Tint and global alpha. I think would be best to implement as blenders at softrender_draw.c, not sure.
- [ ] http_request failing sometimes, JS Native.
- [ ] Serve WebSocket from the dev server, just an echo service or something, for testing.
- [ ] Native: OpenSSL for HTTP and WebSocket.
- [ ] Web is allowing unreasonably long updates. (Run lojs Time test, and pause via dev tools)
- [x] Need realloc et al for web.
- - Either we supply these via the platform, or do not supply in the native platform, and force devs to bring their own stdlib.
- - I think "bring your own stdlib" is a pretty stupid policy. ...but it's not really so: We would provide that lib too, just it's baked into the ROM.
- - Figure out what all is being provided in native, and ensure web provides the same.
- - *** It's possible to build without `-nostdlib`, if we implement a stub `wasi_snapshot_preview1` in Wasm.js.
- - But then native fails: out/rom/lowasm.egg:wasm_runtime_load: WASM module load failed: unsupported opcode fc 0a
- - - Per https://pengowray.github.io/wasm-ops/: fc 0a is a proposed memcpy opcode.
- - - Updated wasm-micro-runtime and now it works :D
- [ ] Cleaner solution for native builds. Get everything linux-specific out of etc/make/demos.mk

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
- [ ] egg_audio_get_playhead(native): Loop. Right now it just keeps counting after the song repeats.
- [ ] Web: Detect low frame rate and pause. Or at least kill the audio. (eg when browser window goes into background)
- [ ] render and softrender currently both require upload stride to be provided explicitly. Should we loosen that? Maybe allow (stride==0) for "oh come on, you know what i mean".
- [ ] Do I need to forbid rendering from a texture onto itself? My heart says yes, but we're not forbidding it yet.
- [ ] Mouse coords when softrender in play. Will require a new hostio_video hook i think.
- [ ] Res type, event type, etc symbols where Javascript can reach them. I don't want to expose them out of the runtime, tho that is an option.
- [ ] `eggrom -t` does it too, make it skip them.
- [ ] Really need a "get string resource" helper for the JS API, that returns strings.
- [ ] Write test plan for new runtime ports. When I make the Mac and Windows ports, I want a checklist to run down.
- [ ] Web storage: We're dumping keys willy-nilly in localStorage. Wiser to pack them into one object, with a game-specific key.
- - It is likely that we'll have domains serving multiple games, we need some measure of isolation for them.
- [ ] One can freely append garbage to ROM files, and that's by design, for future expansion.
- - Means attackers can arrange an MD5 sum for any given ROM file. (and other hashes? research)
- - Call this out in docs: If you depend on a ROM's hash, you must also check its length.
- - Egg itself doesn't use ROM hashes.
- [ ] Refactor egg_native_export.c and egg_native_rom.c to make the native/bundled/external decision clearer, it's a mess right now.
