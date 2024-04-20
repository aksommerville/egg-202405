# Egg

Games platform designed to work like an emulator: The game is a single file that runs in a controlled environment.

Game code can be in JavaScript or WebAssembly.
Build games to a portable Egg file, or bundle with Egg runtime for a fake-native executable.
Games written in C can also be built true-native.

Video and audio, we provide a small opinionated API.
Input is more agnostic, tries to give you raw events as much as possible.

## Prereqs

- *Mandatory*:
- QuickJS [https://github.com/bellard/quickjs]
- wasm-micro-runtime [https://github.com/bytecodealliance/wasm-micro-runtime]
- libcurl [https://github.com/curl/curl].
- - Must build from source, with `-DENABLE_WEBSOCKETS=ON -DBUILD_STATIC_LIBS=ON`
- - I'm using a preview of version 8.8.0
- *For build-time tooling*:
- zlib [https://zlib.net]
- - You should already have this, and we don't do anything exotic with it.
- wasi-sdk [https://github.com/WebAssembly/wasi-sdk]
- - I'm on version 16.0.

There are more optional and platform-specific prereqs; see makefiles.

After cloning, run `make` and you'll be prompted to update the new config file `etc/config.mk`.
Once you've done that, `make` should build the whole project.

## TODO

### High Priority

- [ ] Additional native platforms.
- - [ ] MacOS.
- - [ ] Windows.
- - [ ] Preferred language for Mac and Windows.
- [x] Accelerometer input.
- - xxx Update lojs. ...made a new demo "mobile" instead
- - xxx Update lowasm
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
- [ ] Runtime.js: See `updatePanicCount`. Do a hard-pause instead of terminate.
- [ ] render and softrender currently both require upload stride to be provided explicitly. Should we loosen that? Maybe allow (stride==0) for "oh come on, you know what i mean".
- [ ] Do I need to forbid rendering from a texture onto itself? My heart says yes, but we're not forbidding it yet.
- [ ] Mouse coords when softrender in play. Will require a new hostio_video hook i think.
- [ ] Res type, event type, etc symbols where Javascript can reach them. I don't want to expose them out of the runtime, tho that is an option.
- [ ] Really need a "get string resource" helper for the JS API, that returns strings.
- [ ] Web storage: We're dumping keys willy-nilly in localStorage. Wiser to pack them into one object, with a game-specific key.
- - It is likely that we'll have domains serving multiple games, we need some measure of isolation for them.
- [ ] One can freely append garbage to ROM files, and that's by design, for future expansion.
- - Means attackers can arrange an MD5 sum for any given ROM file. (and other hashes? research)
- - Call this out in docs: If you depend on a ROM's hash, you must also check its length.
- - Egg itself doesn't use ROM hashes.
- [ ] Refactor egg_native_export.c and egg_native_rom.c to make the native/bundled/external decision clearer, it's a mess right now.
- [ ] x11fb: Why does scaling 2x or 3x make such a big performance difference? I'm seeing like 22% CPU vs 4%.
- [ ] Fine-grained access controls for HTTP and Storage. Native and Web both.
