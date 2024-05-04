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

- [ ] Windows
- - [ ] Drivers.
- - [ ] Languages.
- [ ] MacOS.
- - [ ] Drivers.
- - [ ] Languages.
- - [ ] Build sensible app bundle.
- [ ] Pi. We'll need the BCM video driver, beyond that it's really just linux.
- [ ] Synthesizer, native.
- - [ ] GM instruments.
- - [ ] egg_audio_get_playhead(native): Estimate driver's buffer position.
- - [ ] egg_audio_get_playhead(native): Loop. Right now it just keeps counting after the song repeats.
- [ ] Synthesizer, web.
- - [ ] fx: detune
- - [ ] Pitch wheel
- - [ ] Readhead should wrap around on song repeats.
- - [ ] Assess SfgPrinter performance, opportunities for optimization here if needed.
- [ ] Ensure that egg_native_export.c gets dropped by tree-shaking when linking native.
- [ ] Runtime.js: See `updatePanicCount`. Do a hard-pause instead of terminate.
- [ ] render and softrender currently both require upload stride to be provided explicitly. Should we loosen that? Maybe allow (stride==0) for "oh come on, you know what i mean".
- [ ] Do I need to forbid rendering from a texture onto itself? My heart says yes, but we're not forbidding it yet.
- [ ] Mouse coords when softrender in play. Will require a new hostio_video hook i think.
- [ ] Res type, event type, etc symbols where Javascript can reach them. I don't want to expose them out of the runtime, tho that is an option.
- [x] Really need a "get string resource" helper for the JS API, that returns strings.
- - eggsamples/ts has this for Typescript, and it works great.
- [x] Web storage: We're dumping keys willy-nilly in localStorage. Wiser to pack them into one object, with a game-specific key.
- - It is likely that we'll have domains serving multiple games, we need some measure of isolation for them.
- [ ] One can freely append garbage to ROM files, and that's by design, for future expansion.
- - Means attackers can arrange an MD5 sum for any given ROM file. (and other hashes? research)
- - Call this out in docs: If you depend on a ROM's hash, you must also check its length.
- - Egg itself doesn't use ROM hashes.
- [ ] Refactor egg_native_export.c and egg_native_rom.c to make the native/bundled/external decision clearer, it's a mess right now.
- [ ] x11fb: Why does scaling 2x or 3x make such a big performance difference? I'm seeing like 22% CPU vs 4%.
- [ ] Fine-grained access controls for HTTP and Storage. Native and Web both.
- [ ] Web: Need some way to prevent game JS from accessing browser APIs.
- - Would it help to run in a Web Worker? That eliminates Window and Document, but we'd still have to prevent eg fetch and setTimeout.
- [ ] If there's more than one 'js' or 'wasm' resource, concatenate them. And at build time, we can allow >1MB.
- [ ] Source maps
- [ ] Web: Input.js: Try to detect availability of mouse, keyboard, touch, accelerometer.
- [ ] Consider adding a "platform" qualifier to joystick ID (along with vid, pid, version). In case you move a saved game from one machine to another or something.
- [ ] ^ or should we allow saving input config in some global space?
- [x] Option to hide cursor while enabling pointer events (new API). eggsamples/ts/input/PointerShaper.ts could use it.
- [x] Server option to provide an egg file at the command line. So client projects can do their own "make serve".
- [ ] See src/tool/server/server_main.c:server_htdocs_path(): We need to validate against the roots, now that external serving is an option.
- [x] New API to expose platform image decoders (see eggsamples/ts/Font)
- [ ] Wasm: (export "memory"). Web requires this but native does not. Decide who's wrong and change them.
- [ ] romr has a vestigial "qual_by_tid" that we aren't using -- remove it. Make romr_get() take a qualifier.
- [ ] bmp needs a decode_header. For now, we decode the whole image just to read the header. (rawimg_encode.c) jpeg too
- [ ] When a function is absent at link we get this, but it doesn't prevent link or launch:
- - [00:02:38:170 - 76B5EDC2BC80]: warning: failed to link import function (env, jcfg_del)
- - [ ] Can we detect this at runtime and abort earlier?
- - [ ] Can we cause link to fail in this case? Will be a trick, because we do have all those deferred-link "egg_" functions.
