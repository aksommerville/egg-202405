/* Javascript Lights-On test game.
 * Exposes all functionality of the Egg runtime.
 * There's a companion program, lowasm, that does basically the same stuff but written in C.
 * lojs and lowasm should behave the same, that will be part of my general testing.
 * 
 * Please note that this is NOT a great example of how to write Egg games:
 *  - We're writing Javascript directly as a data resource. That means it won't be minified or transpiled.
 *  - We're coding directly against Egg APIs. I'm going to provide higher-level abstractions that will hopefully make a lot more sense.
 *  - And of course, games ought to be fun to play, and this is very not :P
 */

let screenw, screenh;
const VTXBUFA = 256;
const vtxbuf = new Uint8Array(6 * VTXBUFA);
let texid_font;

/* Generic menu interface.
 */
 
let menu = {
  parent: null, // If not null, ESC takes you back to parent.
  items: [
    { label: "Input", cb: () => enterInputMenu() },
    { label: "Audio", cb: () => enterAudioMenu() },
    { label: "Video", cb: () => enterVideoMenu() },
    { label: "Storage", cb: () => enterStorageMenu() },
    { label: "Network", cb: () => enterNetworkMenu() },
    { label: "Misc", cb: () => enterMiscMenu() },
  ],
  itemp: 0,
};

function enterInputMenu() {
  menu = {
    parent: menu,
    items: [
      { label: "Show Joystick IDs", cb: () => "TODO" },
      { label: "Joystick Events", cb: () => "TODO" },
      { label: "Touch", cb: () => "TODO" },
      { label: "Mouse", cb: () => "TODO" },
      { label: "Accelerometer", cb: () => "TODO" },
      { label: "Keyboard", cb: () => "TODO" },
    ],
    itemp: 0,
  };
}

function enterAudioMenu() {
  menu = {
    parent: menu,
    items: [
      { label: "Play Song Once", cb: () => "TODO" },//TODO left/right to pick song id
      { label: "Play Song Repeat", cb: () => "TODO" },
      { label: "Stop Song", cb: () => "TODO" },
      { label: "Play Sound", cb: () => "TODO" },//TODO controls for trim and pan, how?
    ],
    itemp: 0,
  };
}

function enterVideoMenu() {
  menu = {
    parent: menu,
    items: [
      { label: "Xform Clipping Test", cb: () => "TODO" },
      { label: "Too Many Sprites", cb: () => "TODO" },
      { label: "Global Tint and Alpha", cb: () => "TODO" },
      { label: "Offscreen Render", cb: () => "TODO" },
    ],
    itemp: 0,
  };
}

function enterStorageMenu() {
  menu = {
    parent: menu,
    items: [
      { label: "List Resources", cb: () => "TODO" },
      { label: "Strings by Language", cb: () => "TODO" },
      { label: "Persistence", cb: () => "TODO" },
    ],
    itemp: 0,
  };
}

function enterNetworkMenu() {
  menu = {
    parent: menu,
    items: [
      { label: "HTTP Request", cb: () => "TODO" },
      { label: "WebSocket", cb: () => "TODO" },
    ],
    itemp: 0,
  };
}

function enterMiscMenu() {
  menu = {
    parent: menu,
    items: [
      { label: "Log", cb: () => egg.log("%s %.8s %.*s %c %d 0x%x %f", "String", "Truncated String", 3, "Length First", 0x63, 123, 123, 123.456) },
      { label: "Time", cb: () => "TODO" },
      { label: "Languages", cb: () => "TODO" },
      { label: `Is Terminable? ${egg.is_terminable()?"TRUE":"FALSE"}`, cb: () => {} },
      { label: "Terminate", cb: () => egg.request_termination() },
      { label: "Call WASM from JS", cb: () => "TODO" },
    ],
    itemp: 0,
  };
}

/* Cleanup.
 */

function egg_client_quit() {
}

/* Init.
 */

function egg_client_init() {
  
  const fb = egg.texture_get_header(1);
  screenw = fb.w;
  screenh = fb.h;
  if (fb.fmt !== 1) throw new Error(`Expected RGBA framebuffer (fmt 1, got ${fb.fmt})`);
  
  egg.log("new texture...");
  if ((texid_font = egg.texture_new()) < 0) return -1;
  egg.log("load font...");
  if (egg.texture_load_image(texid_font, 0, 2) < 0) return -1;
  egg.log("loaded");
  
  const DISABLE = 2;
  const ENABLE = 3;
  const REQUIRE = 4;
  egg.event_enable(1, ENABLE); // INPUT
  egg.event_enable(2, ENABLE); // CONNECT
  egg.event_enable(3, ENABLE); // DISCONNECT
  egg.event_enable(4, ENABLE); // HTTP_RSP
  egg.event_enable(5, ENABLE); // WS_CONNECT
  egg.event_enable(6, ENABLE); // WS_DISCONNECT
  egg.event_enable(7, ENABLE); // WS_MESSAGE
  egg.event_enable(8, ENABLE); // MMOTION
  egg.event_enable(9, ENABLE); // MBUTTON
  egg.event_enable(10, ENABLE); // MWHEEL
  egg.event_enable(11, ENABLE); // KEY
  egg.event_enable(12, ENABLE); // TEXT
  egg.event_enable(13, ENABLE); // TOUCH
  
  return 0;
}

/* Events.
 */
 
function menuExit() {
  if (menu.parent) {
    menu = menu.parent;
  } else {
    egg.request_termination();
  }
}

function menuEnter() {
  const item = menu.items[menu.itemp];
  if (!item) return;
  if (item.cb) item.cb();
}

function menuMotion(dx, dy) {
  if (dy < 0) {
    menu.itemp--;
    if (menu.itemp < 0) menu.itemp = menu.items.length - 1;
  } else if (dy > 0) {
    menu.itemp++;
    if (menu.itemp >= menu.items.length) menu.itemp = 0;
  }
}

function menuSelectPoint(x, y) {
  const row = Math.floor((y - 8) / 8);
  if ((row < 0) || (row >= menu.items.length)) return;
  menu.items[row].cb();
}

function onKeyDown(usage) {
  switch (usage) {
    case 0x00070029: menuExit(); break; // esc
    case 0x00070028: menuEnter(); break; // enter
    case 0x0007002c: menuEnter(); break; // space
    case 0x0007004f: menuMotion(1, 0); break; // arrows...
    case 0x00070050: menuMotion(-1, 0); break;
    case 0x00070051: menuMotion(0, 1); break;
    case 0x00070052: menuMotion(0, -1); break;
  }
}

function egg_client_update(elapsed) {
  for (const event of egg.event_next()) switch (event.eventType) {
    //case 1: egg.log(`INPUT devid=${event.v0} btnid=${event.v1} value=${event.v2}`); break;
    //case 2: egg.log(`CONNECT devid=${event.v0} mapping=${event.v1}`); break;
    //case 3: egg.log(`DISCONNECT devid=${event.v0}`); break;
    //case 4: egg.log(`HTTP_RSP reqid=${event.v0} status=${event.v1} length=${event.v2}`); break;
    //case 5: egg.log(`WS_CONNECT wsid=${event.v0}`); break;
    //case 6: egg.log(`WS_DISCONNECT wsid=${event.v0}`); break;
    //case 7: egg.log(`WS_MESSAGE wsid=${event.v0} msgid=${event.v1} length=${event.v2}`); break;
    //case 8: egg.log(`MMOTION ${event.v0},${event.v1}`); break;
    case 9: if ((event.v0 === 1) && event.v1) menuSelectPoint(event.v2, event.v3); break;
    //case 10: egg.log(`MWHEEL ${event.v0},${event.v1} @${event.v2},${event.v3}`); break;
    case 11: if (event.v1) onKeyDown(event.v0); break;
    //case 12: egg.log(`TEXT codepoint=${event.v0}`); break;
    case 13: if (event.v1 === 1) menuSelectPoint(event.v2, event.v3); break;//TODO confirm state==1 means Begin
    //default: egg.log(`UNKNOWN EVENT ${event.eventType} [${event.v0},${event.v1},${event.v2},${event.v3}]`); break;
  }
}

/* Render.
 */

function egg_client_render() {
  egg.draw_rect(1, 0, 0, screenw, screenh, 0x2030a0ff);
  if (menu) {
    if ((menu.itemp >= 0) && (menu.itemp < menu.items.length)) {
      egg.draw_rect(1, 4, 8 + menu.itemp * 8 - 1, screenw - 8, 9, 0x000000ff);
    }
    egg.draw_mode(0, 0xffff00ff, 0xff);
    let vtxbufp = 0, vtxc = 0, y = 12;
    for (const { label } of menu.items) {
      let x = 12;
      for (const ch of label) {
        if (vtxc >= VTXBUFA) {
          egg.draw_tile(1, texid_font, vtxbuf, vtxc);
          vtxbufp = 0;
          vtxc = 0;
        }
        const tileid = ch.charCodeAt(0);
        vtxbuf[vtxbufp++] = x;
        vtxbuf[vtxbufp++] = x >> 8;
        vtxbuf[vtxbufp++] = y;
        vtxbuf[vtxbufp++] = y >> 8;
        vtxbuf[vtxbufp++] = tileid;
        vtxbuf[vtxbufp++] = 0;
        vtxc++;
        x += 8;
      }
      y += 8;
    }
    egg.draw_tile(1, texid_font, vtxbuf.buffer, vtxc);
    egg.draw_mode(0, 0, 0xff);
  }
}

exportModule({
  egg_client_quit,
  egg_client_init,
  egg_client_update,
  egg_client_render,
});
