# Egg

Games platform designed to work like an emulator: The game is a single file that runs in a controlled environment.

Game code can be in JavaScript or WebAssembly.

## TODO

- [x] Define API.
- [x] Wire the native side of API.
- [x] egg_request_termination() and egg_is_terminable()
- [x] Events.
- [x] egg_input_device_get_button, need a better strategy
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
