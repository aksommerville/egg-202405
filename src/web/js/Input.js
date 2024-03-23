/* Input.js
 * Manages input collection and exposes the relevant public API hooks.
 */
 
export class Input {
  constructor(window, canvas) {
    this.window = window;
    this.canvas = canvas;
    
    this.evtq = [];
    this.evtmask = 
      (1<<Input.EVENT_INPUT)|
      (1<<Input.EVENT_CONNECT)|
      (1<<Input.EVENT_DISCONNECT)|
      (1<<Input.EVENT_HTTP_RSP)|
      (1<<Input.EVENT_WS_CONNECT)|
      (1<<Input.EVENT_WS_DISCONNECT)|
      (1<<Input.EVENT_WS_MESSAGE)|
      // MMOTION, MBUTTON, MWHEEL, TEXT: off by default
      (1<<Input.EVENT_KEY)|
    0;
    
    this.cursorVisible = false;
    this.mouseEventListener = null;
    this.mouseButtonsDown = new Set();
    
    this.keyListener = e => this.onKey(e);
    this.window.addEventListener("keydown", this.keyListener);
    this.window.addEventListener("keyup", this.keyListener);
    
    this.gamepads = []; // sparse
    this.gamepadListener = e => this.onGamepadConnection(e);
    this.window.addEventListener("gamepadconnected", this.gamepadListener);
    this.window.addEventListener("gamepaddisconnected", this.gamepadListener);
    
    this.touchListener = e => this.onTouch(e);
    this.canvas.addEventListener("touchstart", this.touchListener);
    this.canvas.addEventListener("touchend", this.touchListener);
    this.canvas.addEventListener("touchcancel", this.touchListener);
    this.canvas.addEventListener("touchmove", this.touchListener);
    
    // TODO Accelerometer. Looks easy to support, just need to define it in the public API.
    // Need to switch on and off when event mask changes, like cursor.
    // https://developer.mozilla.org/en-US/docs/Web/API/Accelerometer
  }
  
  detach() {
    this._unlistenMouse();
    if (this.keyListener) {
      this.window.removeEventListener("keydown", this.keyListener);
      this.window.removeEventListener("keyup", this.keyListener);
      this.keyListener = null;
    }
    if (this.gamepadListener) {
      this.window.removeEventListener("gamepadconnected", this.gamepadListener);
      this.window.removeEventListener("gamepaddisconnected", this.gamepadListener);
      this.gamepadListener = null;
    }
    if (this.touchListener) {
      this.canvas.removeEventListener("touchstart", this.touchListener);
      this.canvas.removeEventListener("touchend", this.touchListener);
      this.canvas.removeEventListener("touchcancel", this.touchListener);
      this.canvas.removeEventListener("touchmove", this.touchListener);
      this.touchListener = null;
    }
  }
  
  update() {
    this._updateGamepads();
  }
  
  pushEvent(eventType, v0=0, v1=0, v2=0, v3=0) {
    if (!(this.evtmask & (1 << eventType))) return;
    this.evtq.push({ eventType, v0, v1, v2, v3 });
  }
  
  /* Nonzero (IMPOSSIBLE or REQUIRED) if the given event can't be changed.
   * If subject to user change, we return zero (QUERY).
   */
  getEventHardState(type) {
    switch (type) {
      case Input.EVENT_INPUT: return 0;
      case Input.EVENT_CONNECT: return 0;
      case Input.EVENT_DISCONNECT: return 0;
      case Input.EVENT_HTTP_RSP: return Input.EVTSTATE_REQUIRED;
      case Input.EVENT_WS_CONNECT: return 0;
      case Input.EVENT_WS_DISCONNECT: return 0;
      case Input.EVENT_WS_MESSAGE: return Input.EVTSTATE_REQUIRED;
      case Input.EVENT_MMOTION: return 0;
      case Input.EVENT_MBUTTON: return 0;
      case Input.EVENT_MWHEEL: return 0;
      case Input.EVENT_KEY: return 0;
      case Input.EVENT_TEXT: return 0;
      case Input.EVENT_TOUCH: return 0;
    }
    return Input.EVTSTATE_IMPOSSIBLE;
  }
  
  reset() {
    this.evtq = [];
    const initialMask =
      (1<<Input.EVENT_INPUT)|
      (1<<Input.EVENT_CONNECT)|
      (1<<Input.EVENT_DISCONNECT)|
      (1<<Input.EVENT_HTTP_RSP)|
      (1<<Input.EVENT_WS_CONNECT)|
      (1<<Input.EVENT_WS_DISCONNECT)|
      (1<<Input.EVENT_WS_MESSAGE)|
      (1<<Input.EVENT_KEY)|
    0;
    if (initialMask !== this.evtmask) {
      for (let i=0; i<30; i++) {
        if ((initialMask & (1 << i)) !== (this.evtmask & (1 << i))) {
          this.event_enable(i, (initialMask & (1 << i)) ? 3 : 2);
        }
      }
    }
  }
  
  /* Touch.
   ********************************************************************/
   
  onTouch(e) {
    //console.log(`onTouch`, e);
    if (!e.changedTouches) return;
    const bounds = this.canvas.getBoundingClientRect();
    let state;
    switch (e.type) {
      case "touchstart": state = 1; break;
      case "touchend":
      case "touchcancel": state = 0; break;
      case "touchmove": state = 2; break;
    }
    for (const touch of e.changedTouches) {
      const x = touch.clientX - bounds.x;
      const y = touch.clientY - bounds.y;
      this.pushEvent(Input.EVENT_TOUCH, touch.identifier, state, x, y);
    }
  }
  
  /* Gamepad.
   * We will use (gamepad.index+1) as (devid) for reporting to the client.
   * (btnid) will be 0..255 for axes and 256.. for buttons, adding the index.
   ******************************************************************************/
   
  _updateGamepads() {
    if (!this.window.navigator.getGamepads) return;
    for (const gamepad of this.window.navigator.getGamepads()) {
      if (!gamepad) continue;
      const local = this.gamepads[gamepad.index];
      if (!local) continue;
      
      for (let i=local.axes.length; i-->0; ) {
        const pv = local.axes[i];
        const nx = gamepad.axes[i];
        if (pv === nx) continue;
        local.axes[i] = nx;
        this.pushEvent(Input.EVENT_INPUT, local.devid, i, nx);
      }
      
      for (let i=local.buttons.length; i-->0; ) {
        const pv = local.buttons[i];
        const nx = gamepad.buttons[i].value;
        if (pv === nx) continue;
        local.buttons[i] = nx;
        this.pushEvent(Input.EVENT_INPUT, local.devid, 0x100 + i, nx);
      }
    }
  }
  
  onGamepadConnection(e) {
    switch (e.type) {
    
      case "gamepadconnected": {
          //console.log(`Game Pad Connected`, e);
          this.gamepads[e.gamepad.index] = {
            devid: e.gamepad.index + 1,
            index: e.gamepad.index,
            id: e.gamepad.id,
            axes: (e.gamepad.axes || []).map(v => v),
            buttons: (e.gamepad.buttons || []).map(v => 0),
            mapping: e.gamepad.mapping,
          };
          let mapping = 0; // RAW
          switch (e.gamepad.mapping) {
            case "standard": mapping = 1; break;
          }
          this.pushEvent(Input.EVENT_CONNECT, e.gamepad.index + 1, mapping);
        } break;
        
      case "gamepaddisconnected": {
          const local = this.gamepads[e.gamepad.index];
          if (local) {
            delete this.gamepads[e.gamepad.index];
            this.pushEvent(Input.EVENT_DISCONNECT, local.devid);
          }
        } break;
    }
  }
  
  /* Mouse.
   ********************************************************************************/
  
  _checkCursorVisibility(show) {
    show = !!show;
    if (show === this.cursorVisible) return;
    this.cursorVisible = show;
    if (this.canvas) {
      if (show) {
        this.canvas.style.cursor = "pointer";
        this._listenMouse();
      } else {
        this.canvas.style.cursor = "none";
        this._unlistenMouse();
      }
    }
  }
  
  _listenMouse() {
    if (this.mouseEventListener) return;
    this.mouseEventListener = e => this.onMouseEvent(e);
    this.window.addEventListener("mousewheel", this.mouseEventListener);
    this.window.addEventListener("mousemove", this.mouseEventListener);
    this.window.addEventListener("mouseup", this.mouseEventListener);
    this.canvas.addEventListener("mousedown", this.mouseEventListener);
    this.canvas.addEventListener("contextmenu", this.mouseEventListener);
  }
  
  _unlistenMouse() {
    if (this.mouseEventListener) {
      this.window.removeEventListener("mousewheel", this.mouseEventListener);
      this.window.removeEventListener("mousemove", this.mouseEventListener);
      this.window.removeEventListener("mouseup", this.mouseEventListener);
      this.canvas.removeEventListener("mousedown", this.mouseEventListener);
      this.canvas.removeEventListener("contextmenu", this.mouseEventListener);
      this.mouseEventListener = null;
    }
  }
  
  onMouseEvent(e) {
    const bounds = this.canvas.getBoundingClientRect();
    const x = e.x - bounds.x;
    const y = e.y - bounds.y;
    switch (e.type) {
      case "mousemove": this.pushEvent(Input.EVENT_MMOTION, x, y); break;
      case "mousedown": {
          if (e.target !== this.canvas) return;
          if (this.mouseButtonsDown.has(e.button)) return;
          this.mouseButtonsDown.add(e.button);
          e.preventDefault();
          this.pushEvent(Input.EVENT_MBUTTON, e.button, 1, x, y);
        } break;
      case "mouseup": {
          if (!this.mouseButtonsDown.has(e.button)) return;
          this.mouseButtonsDown.delete(e.button);
          this.pushEvent(Input.EVENT_MBUTTON, e.button, 0, x, y);
        } break;
      case "contextmenu": e.preventDefault(); break;
      case "mousewheel": {
          if ((x < 0) || (y < 0) || (x >= bounds.width) || (y >= bounds.height)) return;
          let dx = e.deltaX, dy = e.deltaY;
          if (e.wheelDeltaX) dx /= Math.abs(e.wheelDeltaX);
          if (e.wheelDeltaY) dy /= Math.abs(e.wheelDeltaY);
          if (e.wheelDelta && !e.wheelDeltaX && !e.wheelDeltaY) {
            dx /= Math.abs(e.wheelDelta);
            dy /= Math.abs(e.wheelDelta);
          }
          if (e.shiftKey) { // I like Shift+Wheel to mean X instead of Y.
            let tmp = dx;
            dx = dy;
            dy = tmp;
          }
          if (dx || dy) {
            this.pushEvent(Input.EVENT_MWHEEL, dx, dy, x, y);
          }
          //event.preventDefault(); // It's installed as passive... why is that? (Chrome Linux)
        } break;
    }
  }
  
  /* Keyboard.
   **************************************************************************/
  
  onKey(e) {
    
    // Ignore all keyboard events when Alt or Ctrl is held.
    if (e.ctrlKey || e.altKey) {
      return;
    }
    
    // TODO We might be too heavy-handed with event suppression. Bear in mind that we are listening on Window.
    
    // If we recognize the key and user wants key events, pass it on and suppress it in browser.
    if (this.evtmask & (1 << Input.EVENT_KEY)) {
      const usage = this.hidUsageByKeyCode(e.code);
      if (usage) {
        const v = (e.type === "keyup") ? 0 : e.repeat ? 2 : 1;
        this.pushEvent(Input.EVENT_KEY, usage, v);
        e.preventDefault();
        e.stopPropagation();
      }
    }
    
    // Likewise, if user wants text and it looks like text.
    if (this.evtmask & (1 << Input.EVENT_TEXT)) {
      if (e.key?.length === 1) {
        this.pushEvent(Input.EVENT_TEXT, e.key.charCodeAt(0));
        e.preventDefault();
        e.stopPropagation();
      }
    }
  }
  
  hidUsageByKeyCode(code) {
    if (!code) return 0;
  
    // "KeyA".."KeyZ" => 0x04..0x1d
    if ((code.length === 4) && code.startsWith("Key")) {
      const ch = code.charCodeAt(3);
      if ((ch >= 0x41) && (ch <= 0x5a)) return 0x00070004 + ch - 0x41;
    }
    
    // "Digit1".."Digit9" => 0x1e..0x25, some jackass put "0" on the right side... why...
    if ((code.length === 6) && code.startsWith("Digit")) {
      const ch = code.charCodeAt(5);
      if ((ch >= 0x31) && (ch <= 0x39)) return 0x0007001e + ch - 0x31;
      if (ch === 0x30) return 0x00070027; // zero
    }
    
    // "F1".."F12" => 0x3a..0x45
    // "F13".."F24" => 0x68..0x73
    if (((code.length === 2) || (code.length === 3)) && (code[0] === 'F')) {
      const v = +code.substring(1);
      if ((v >= 1) && (v <= 12)) return 0x0007003a + v - 1;
      if ((v >= 13) && (v <= 24)) return 0x00070068 + v - 13;
    }
    
    // "Numpad1".."Numpad9" => 0x59..0x61, again with zero on top because Jesus hates me.
    if ((code.length === 7) && code.startsWith("Numpad")) {
      const v = +code[7];
      if ((v >= 1) && (v <= 9)) return 0x00070059 + v - 1;
      if (v === 0) return 0x00070062;
    }
    
    // And finally a not-too-crazy set of one-off names.
    switch (code) {
      case "Enter":          return 0x00070028;
      case "Escape":         return 0x00070029;
      case "Backspace":      return 0x0007002a;
      case "Tab":            return 0x0007002b;
      case "Space":          return 0x0007002c;
      case "Minus":          return 0x0007002d;
      case "Equal":          return 0x0007002e;
      case "BracketLeft":    return 0x0007002f;
      case "BracketRight":   return 0x00070039;
      case "Backslash":      return 0x00070031;
      case "Semicolon":      return 0x00070033;
      case "Quote":          return 0x00070034;
      case "Backquote":      return 0x00070035;
      case "Comma":          return 0x00070036;
      case "Period":         return 0x00070037;
      case "Slash":          return 0x00070038;
      case "CapsLock":       return 0x00070039;
      case "Pause":          return 0x00070048;
      case "Insert":         return 0x00070049;
      case "Home":           return 0x0007004a;
      case "PageUp":         return 0x0007004b;
      case "Delete":         return 0x0007004c;
      case "PageDown":       return 0x0007004e;
      case "ArrowRight":     return 0x0007004f;
      case "ArrowLeft":      return 0x00070050;
      case "ArrowDown":      return 0x00070051;
      case "ArrowUp":        return 0x00070052;
      case "NumLock":        return 0x00070053;
      case "NumpadDivide":   return 0x00070054;
      case "NumpadMultiply": return 0x00070055;
      case "NumpadSubtract": return 0x00070056;
      case "NumpadAdd":      return 0x00070057;
      case "NumpadEnter":    return 0x00070058;
      case "NumpadDecimal":  return 0x00070063;
      case "ContextMenu":    return 0x00070076;
      case "ShiftLeft":      return 0x000700e1;
      case "ShiftRight":     return 0x000700e5;
      case "ControlLeft":    return 0x000700e0;
      case "ControlRight":   return 0x000700e4;
      case "AltLeft":        return 0x000700e2;
      case "AltRight":       return 0x000700e6;
    }
    return 0;
  }
  
  /* Public API.
   *******************************************************************************/
  
  event_next() {
    // This only happens for JS clients. Wasm, Runtime reads from this.evtq directly.
    const events = this.evtq;
    this.evtq = [];
    return events;
  }
  
  event_enable(type, state) {
    const hard = this.getEventHardState(type);
    if (hard) return hard;
    if (state === Input.EVTSTATE_QUERY) {
      return (this.evtmask & (1 << type)) ? Input.EVTSTATE_ENABLED : Input.EVTSTATE_DISABLED;
    }
    if ((state === Input.EVTSTATE_ENABLED) || (state === Input.EVTSTATE_REQUIRED)) {
      this.evtmask |= 1 << type;
    } else if (state === Input.EVTSTATE_DISABLED) {
      this.evtmask &= ~(1 << type);
    }
    const mouseEvents = (1 << Input.EVENT_MMOTION) | (1 << Input.EVENT_MBUTTON) | (1 << Input.EVENT_MWHEEL);
    this._checkCursorVisibility(this.evtmask & mouseEvents);
    return (this.evtmask & (1 << type)) ? Input.EVTSTATE_ENABLED : Input.EVTSTATE_DISABLED;
  }
  
  input_device_get_name(devid) {
    const local = this.gamepads[devid - 1];
    if (!local || (local.devid !== devid)) return "";
    const name = local.id.split('(')[0].trim();
    return name || local.id;
  }
  
  input_device_get_ids(devid) {
    const local = this.gamepads[devid - 1];
    if (!local || (local.devid !== devid)) return {vid:0,pid:0,version:0};
    // Linux Chrome: Microsoft X-Box 360 pad (STANDARD GAMEPAD Vendor: 045e Product: 028e)
    // Not at all sure how standard that formatting is, but we don't have much else to go on...
    let vid=0, pid=0, version=0;
    let match;
    if (match = local.id.match(/Vendor: ([0-9a-fA-F]{4})/)) {
      vid = parseInt(match[1], 16);
    }
    if (match = local.id.match(/Product: ([0-9a-fA-F]{4})/)) {
      pid = parseInt(match[1], 16);
    }
    if (match = local.id.match(/Version: ([0-9a-fA-F]{4})/)) {
      // This one doesn't exist for me. And not sure whether they'd break it out as MAJOR.MINOR.REVISION.
      // 0xf000=MAJOR, 0x0f00=MINOR, 0x00ff=REVISION
      version = parseInt(match[1], 16);
    }
    return { vid, pid, version };
  }
  
  input_device_get_button(devid, p) {
    if (p < 0) return null;
    const local = this.gamepads[devid - 1];
    if (!local || (local.devid !== devid)) return null;
    
    if (p < local.axes.length) {
      let hidusage = 0;
      if (local.mapping === "standard") {
        switch (p) {
          case 0: hidusage = 0x00010030; break; // lx
          case 1: hidusage = 0x00010031; break; // ly
          case 2: hidusage = 0x00010033; break; // rx
          case 3: hidusage = 0x00010034; break; // ry
        }
      }
      return {
        btnid: p,
        hidusage,
        lo: -1,
        hi: 1,
        value: local.axes[p],
      };
    }
    p -= local.axes.length;
    
    if (p < local.buttons.length) {
      let hidusage = 0x00090000 + p;
      if (local.mapping === "standard") {
        switch (p) {
          case  0: hidusage=0x00050037; break; // south
          case  1: hidusage=0x00050037; break; // east
          case  2: hidusage=0x00050037; break; // west
          case  3: hidusage=0x00050037; break; // north
          case  4: hidusage=0x00050039; break; // l1
          case  5: hidusage=0x00050039; break; // r1
          case  6: hidusage=0x00050039; break; // l2
          case  7: hidusage=0x00050039; break; // r2
          case  8: hidusage=0x0001003e; break; // select
          case  9: hidusage=0x0001003d; break; // start
          case 10: hidusage=0x00090000; break; // lp
          case 11: hidusage=0x00090001; break; // rp
          case 12: hidusage=0x00010090; break; // dup
          case 13: hidusage=0x00010091; break; // ddown
          case 14: hidusage=0x00010093; break; // dleft
          case 15: hidusage=0x00010092; break; // dright
          case 16: hidusage=0x00010085; break; // heart -- "System Main Menu", debatable.
        }
      }
      return {
        btnid: 0x100 + p,
        hidusage,
        lo: 0,
        hi: 1,
        value: local.buttons[p],
      };
    }
    return null;
  }
  
  input_device_disconnect(devid) {
    const local = this.gamepads[devid - 1];
    if (!local || (local.devid !== devid)) return;
    delete this.gamepads[devid - 1];
  }
}

Input.EVENT_INPUT         =  1; /* [devid,btnid,value,_] */
Input.EVENT_CONNECT       =  2; /* [devid,_,_,_] */
Input.EVENT_DISCONNECT    =  3; /* [devid,_,_,_] */
Input.EVENT_HTTP_RSP      =  4; /* [reqid,status,length,_] */
Input.EVENT_WS_CONNECT    =  5; /* [wsid,_,_,_] */
Input.EVENT_WS_DISCONNECT =  6; /* [wsid,_,_,_] */
Input.EVENT_WS_MESSAGE    =  7; /* [wsid,msgid,length,_] */
Input.EVENT_MMOTION       =  8; /* [x,y,_,_] */
Input.EVENT_MBUTTON       =  9; /* [btnid,value,x,y] */
Input.EVENT_MWHEEL        = 10; /* [dx,dy,x,y] */
Input.EVENT_KEY           = 11; /* [hidusage,value,_,_] */
Input.EVENT_TEXT          = 12; /* [codepoint,_,_,_] */
Input.EVENT_TOUCH         = 13; /* [id,state,x,y] */
  
Input.EVTSTATE_QUERY = 0;
Input.EVTSTATE_IMPOSSIBLE = 1;
Input.EVTSTATE_DISABLED = 2;
Input.EVTSTATE_ENABLED = 3;
Input.EVTSTATE_REQUIRED = 4;
