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
let texid_font, texid_misc, texid_os1=0, texid_os2=0;
const devidv = [];
let songid;
let gwsid = 0;
let wsconnected = false;

/* Helper interface to egg.draw_tile
 */
 
class TileRenderer {
  constructor() {
    this.limit = 256;
    this.buf = new Uint8Array(6 * this.limit);
    this.vtxc = 0;
    this.srctexid = 0;
  }
  
  begin(texid, tint, alpha) {
    if (this.vtxc) this._flush();
    this.srctexid = texid;
    egg.draw_mode(0, tint, alpha);
  }
  
  end() {
    if (this.vtxc) this._flush();
    egg.draw_mode(0, 0, 0xff);
  }
  
  _flush() {
    egg.draw_tile(1, this.srctexid, this.buf.buffer, this.vtxc);
    this.vtxc = 0;
  }
  
  tile(x, y, tileid, xform) {
    if (this.vtxc >= this.limit) this._flush();
    let p = this.vtxc * 6;
    this.buf[p++] = x; 
    this.buf[p++] = x >> 8;
    this.buf[p++] = y;
    this.buf[p++] = y >> 8;
    this.buf[p++] = tileid;
    this.buf[p++] = xform;
    this.vtxc++;
  }
  
  // Assumes 8 pixels wide. (we could check but no need in this app)
  string(text, x, y) {
    for (let i=0; i<text.length; i++, x+=8) {
      this.tile(x, y, text.charCodeAt(i), 0);
    }
  }
}

const tileRenderer = new TileRenderer();

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

/* Input menus.
 */

function enterInputMenu() {
  menu = {
    parent: menu,
    items: [
      { label: "Show Joystick IDs", cb: () => showJoystickIds() },
      { label: "Joystick Events", cb: () => showJoystickEvents() },
      { label: "Touch", cb: () => showTouchEvents() },
      { label: "Mouse", cb: () => showMouseEvents() },
      { label: "Accelerometer", cb: () => showAccelerometerEvents() },
      { label: "Keyboard", cb: () => showKeyboardEvents() },
    ],
    itemp: 0,
  };
}

function showJoystickIds() {
  const items = [
    { label: "FYI: List won't refresh while open.", cb: () => {} }, // would be doable if we wanted it but extra effort.
  ];
  for (const devid of devidv) {
    const ids = egg.input_device_get_ids(devid);
    if (!ids) continue;
    const name = egg.input_device_get_name(devid);
    items.push({
      label: `${ids.vid}:${ids.pid}:${ids.version} ${name}`,
      cb: () => {},
    });
  }
  menu = {
    parent: menu,
    items,
    itemp: 0,
  };
}

function showJoystickEvents() {
  const events = [];
  menu = {
    parent: menu,
    items: [],
    itemp: 0,
    onJoystickInput: (devid, btnid, value) => {
      if (events.length >= 22) events.splice(0, 1);
      events.push(`${devid}.${btnid}=${value}`);
    },
    renderExtra: () => {
      tileRenderer.begin(texid_font, 0xffff80ff, 0xff);
      let y = 6;
      for (const label of events) {
        tileRenderer.string(label, 12, y);
        y += 8;
      }
      tileRenderer.end();
    },
  };
}

function showTouchEvents() {
  const TOUCH_LIMIT = 400;
  const touches = [];
  let clock = 0;
  const recentTaps = [];
  menu = {
    parent: menu,
    items: [],
    itemp: 0,
    update: (elapsed) => {
      clock += elapsed;
    },
    onTouch: (touchid, state, x, y) => {
      // state: (0,1,2)=(off,on,move)
      if (state === 1) {
        recentTaps.push(clock);
        if (recentTaps.length > 3) recentTaps.splice(0, 1);
        if (recentTaps.length >= 3) {
          const elapsed = recentTaps[2] - recentTaps[0];
          if (elapsed < 1.0) {
            menuExit();
            return;
          }
        }
      }
      if (touches.length) {
        const [pvx, pvy] = touches[touches.length - 1];
        if ((x === pvx) && (y === pvy)) return;
      }
      touches.push([x, y]);
      if (touches.length > TOUCH_LIMIT) touches.splice(0, 1);
    },
    renderExtra: () => {
      tileRenderer.begin(texid_font, 0xc0c0c0ff, 0xff);
      tileRenderer.string("Triple tap to exit", 8, screenh - 8);
      tileRenderer.end();
      // The most recent 14 events get tiles 0..14 from image 3, and all others get tile 15.
      tileRenderer.begin(texid_misc, 0, 0xff);
      for (let i=0; i<touches.length; i++) {
        const [x, y] = touches[i];
        const tileid = Math.min(15, Math.floor((touches.length - i) / 3));
        tileRenderer.tile(x, y, tileid, 0);
      }
      tileRenderer.end();
    },
  };
}

function showMouseEvents() {
  const LIST_LENGTH = (screenh - 8) / 8;
  let pvx=0, pvy=0;
  const items = [];
  menu = {
    parent: menu,
    items,
    itemp: 0,
    onMouseMotion: (x, y) => {
      if ((x === pvx) && (y === pvy)) return;
      pvx = x;
      pvy = y;
      items.push({ label: `MOTION ${x},${y}`, cb: () => {}});
      if (items.length > LIST_LENGTH) items.splice(0, 1);
    },
    onMouseButton: (btnid, value, x, y) => {
      items.push({ label: `BUTTON ${x},${y} #${btnid}=${value}`, cb: () => {}});
      if (items.length > LIST_LENGTH) items.splice(0, 1);
    },
    onMouseWheel: (dx, dy, x, y) => {
      items.push({ label: `WHEEL  ${x},${y} ${dx}=${dy}`, cb: () => {}});
      if (items.length > LIST_LENGTH) items.splice(0, 1);
    },
  };
}

function showAccelerometerEvents() {//TODO
  menu = {
    parent: menu,
    items: [],
    itemp: 0,
  };
}

function showKeyboardEvents() {
  const LIST_LENGTH = (screenh - 8) / 8;
  const items = [];
  menu = {
    parent: menu,
    items,
    itemp: 0,
    onKey: (usage, value) => {
      let k;
      if ((usage >= 0x00070000) && (usage < 0x00080000)) { // should always be the case
        k = (usage & 0xffff).toString(16);
        if (k.length === 1) k = '0' + k;
      } else {
        k = usage;
      }
      items.push({ label: `KEY ${k}=${value}`, cb: () => {}});
      if (items.length > LIST_LENGTH) items.splice(0, 1);
    },
    onText: (codepoint) => {
      items.push({ label: `TEXT U+${codepoint.toString(16)} (${String.fromCharCode(codepoint)})`, cb: () => {}});
      if (items.length > LIST_LENGTH) items.splice(0, 1);
    },
  };
}

/* Audio menus.
 */

function enterAudioMenu() {
  let uiSongId = songid; // Song IDs should be contiguous from 1.
  let songCount = 0;
  const uiSoundIds = []; // Sound IDs are probably sparse
  let uiSoundp = 0;
  for (let p=0; ; p++) {
    const ids = egg.res_id_by_index(p);
    if (!ids || !ids.tid) break;
    if (ids.qual) continue;
    if (ids.tid === 6) { // song
      if (ids.rid > songCount) songCount = ids.rid;
    } else if (ids.tid === 7) { // sound
      uiSoundIds.push(ids.rid);
    }
  }
  const m = {
    parent: menu,
    items: [
      { label: `Play Song Once (${uiSongId})`, cb: () => {
        egg.audio_play_song(0, uiSongId, false, false);
      }},
      { label: `Play Song Repeat (${uiSongId})`, cb: () => {
        egg.audio_play_song(0, uiSongId, false, true);
      }},
      { label: "Restart Song", cb: () => {
        egg.audio_play_song(0, uiSongId, true, true);
      }},
      { label: "Stop Song", cb: () => {
        egg.audio_play_song(0, 0, true, true);
      }},
      { label: `Play Sound (${uiSoundIds[uiSoundp]})`, cb: () => {
        //TODO controls for trim and pan, how?
        egg.audio_play_sound(0, uiSoundIds[uiSoundp], 1.0, 0.0);
      }},
      { label: "Playhead: ", cb: () => {} },
    ],
    itemp: 0,
    onHorz: (dx) => {
      switch (m.itemp) {
        case 0: 
        case 1: {
            uiSongId += dx;
            if (uiSongId < 1) uiSongId = songCount;
            else if (uiSongId > songCount) uiSongId = 1;
            m.items[0].label = `Play Song Once (${uiSongId})`;
            m.items[1].label = `Play Song Repeat (${uiSongId})`;
          } break;
        case 4: {
            uiSoundp += dx;
            if (uiSoundp < 0) uiSoundp = uiSoundIds.length - 1;
            else if (uiSoundp >= uiSoundIds.length) uiSoundp = 0;
            m.items[4].label = `Play Sound (${uiSoundIds[uiSoundp]})`;
          } break;
      }
    },
    update: () => {
      const ph = egg.audio_get_playhead();
      m.items[5].label = `Playhead: ${ph.toFixed(3)}`;
    },
  };
  menu = m;
}

/* Video menus.
 */

function enterVideoMenu() {
  menu = {
    parent: menu,
    items: [
      { label: "Xform Clipping Test", cb: () => xformClippingTest() },
      { label: "Too Many Sprites", cb: () => tooManySprites() },
      { label: "Global Tint and Alpha", cb: () => globalTintAndAlpha() },
      { label: "Offscreen Render", cb: () => offscreenRender() },
    ],
    itemp: 0,
  };
}

function xformClippingTest() {
  const srcx=16, srcy=32, srcw=32, srch=48;
  const speed = 100; // px/s
  let dstx = (screenw >> 1) - (srcw >> 1);
  let dsty = (screenh >> 1) - (srch >> 1);
  let dx=0, dy=0;
  let xform = 0;
  menu = {
    parent: menu,
    items: [],
    itemp: 0,
    onJoystickInput: (devid, btnid, value) => {
      //egg.log(`onJoystickInput ${devid}.${btnid}=${value}`);
      switch (btnid) {
        // This is not even close to OK in real life. Hard-coding against my devices for both evdev and Web.
        case 0x00030010: dx = (value < 0) ? -1 : (value > 0) ? 1 : 0; break;
        case 0x00030011: dy = (value < 0) ? -1 : (value > 0) ? 1 : 0; break;
        case 0x00010122: if (value) { if (++xform > 7) xform = 0; } break;
        case 256: if (value) { if (++xform > 7) xform = 0; } break;
        case 270: if (value) dx = -1; else if (dx < 0) dx = 0; break;
        case 271: if (value) dx = 1; else if (dx > 0) dx = 0; break;
        case 268: if (value) dy = -1; else if (dy < 0) dy = 0; break;
        case 269: if (value) dy = 1; else if (dy > 0) dy = 0; break;
      }
    },
    update: (elapsed) => {
      dstx += dx * speed * elapsed;
      dsty += dy * speed * elapsed;
    },
    renderExtra: () => {
    
      // One decal that you can move around.
      egg.draw_decal(1, texid_misc, Math.round(dstx), Math.round(dsty), srcx, srcy, srcw, srch, xform);
      
      // Eight more, at the corners and edges.
      for (let subx=-1; subx<=1; subx++) {
        for (let suby=-1; suby<=1; suby++) {
          if (!subx && !suby) continue;
          let sdstx = Math.round((subx + 1) * screenw / 2);
          let sdsty = Math.round((suby + 1) * screenh / 2);
          if (xform & 4) {
            sdstx -= srch >> 1;
            sdsty -= srcw >> 1;
          } else {
            sdstx -= srcw >> 1;
            sdsty -= srch >> 1;
          }
          egg.draw_decal(1, texid_misc, sdstx, sdsty, srcx, srcy, srcw, srch, xform);
        }
      }
      
      // Describe the transform in one line near the bottom. Stay off the *very* bottom; we're using that space to observe clipping.
      tileRenderer.begin(2, 0xffffffff, 0xc0);
      let msg = `xform ${xform}:`;
      if (xform & 1) msg += " XREV";
      if (xform & 2) msg += " YREV";
      if (xform & 4) msg += " SWAP";
      switch (xform) {
        case 0: msg += " (none)"; break;
        case 1: msg += " (flop)"; break;
        case 2: msg += " (flip)"; break;
        case 3: msg += " (180)"; break;
        case 4: msg += " (unusual)"; break;
        case 5: msg += " (counterclockwise)"; break;
        case 6: msg += " (clockwise)"; break;
        case 7: msg += " (unusual)"; break;
      }
      tileRenderer.string(msg, 4, screenh-30);
      tileRenderer.end();
    },
  };
}

function tooManySprites() {
  const maxSpeed = 300; // px/s but it's actually half of this
  const SPRITE_LIMIT = 16384;
  const sprites = [];
  const addSprite = () => {
    const sprite = {
      x: Math.floor(Math.random() * screenw),
      y: Math.floor(Math.random() * screenh),
      tileidp: 0,
      xform: (Math.random() * 8) & 7,
      dx: Math.random() * maxSpeed - maxSpeed / 2,
      dy: Math.random() * maxSpeed - maxSpeed / 2,
    };
    switch (Math.floor(Math.random() * 8)) {
      case 0: sprite.tileidv = [0x14, 0x15]; break;
      case 1: sprite.tileidv = [0x16, 0x17, 0x16, 0x18]; break;
      case 2: sprite.tileidv = [0x1a, 0x1a, 0x19]; break;
      case 3: sprite.tileidv = [0x1b]; break;
      case 4: sprite.tileidv = [0x1c]; break;
      case 5: sprite.tileidv = [0x1d]; break;
      case 6: sprite.tileidv = [0x1e]; break;
      default: sprite.tileidv = [0x1f]; break;
    }
    sprites.push(sprite);
  };
  for (let i=32; i-->0; ) addSprite(); // Best to start on a power of two, but really anything goes.
  const animTime = 0.250; // All sprites switch frames at the same time, because it's not important.
  let animClock = animTime;
  menu = {
    parent: menu,
    items: [],
    itemp: 0,
    update: (elapsed) => {
      if ((animClock -= elapsed) <= 0) {
        animClock += animTime;
        for (const sprite of sprites) {
          if (++sprite.tileidp >= sprite.tileidv.length) sprite.tileidp = 0;
        }
      }
      for (const sprite of sprites) {
        sprite.x += sprite.dx * elapsed;
        if (
          ((sprite.x < 0) && (sprite.dx < 0)) ||
          ((sprite.x >= screenw) && (sprite.dx > 0))
        ) sprite.dx = -sprite.dx;
        sprite.y += sprite.dy * elapsed;
        if (
          ((sprite.y < 0) && (sprite.dy < 0)) ||
          ((sprite.y >= screenh ) && (sprite.dy > 0))
        ) sprite.dy = -sprite.dy;
      }
    },
    renderExtra: () => {
      tileRenderer.begin(texid_misc, 0, 0xff);
      for (const sprite of sprites) {
        tileRenderer.tile(Math.round(sprite.x), Math.round(sprite.y), sprite.tileidv[sprite.tileidp], sprite.xform);
      }
      tileRenderer.end();
      tileRenderer.begin(texid_font, 0xffffffff, 0xff);
      tileRenderer.string(`count: ${sprites.length}`, 4, screenh - 4);
      tileRenderer.end();
    },
    onHorz: (dx) => {
      if (dx < 0) {
        if (sprites.length > 1) {
          const nc = Math.max(1, sprites.length >> 1);
          sprites.splice(nc, sprites.length - nc);
        }
      } else if (dx > 0) {
        const nc = Math.min(SPRITE_LIMIT, sprites.length << 1);
        while (sprites.length < nc) addSprite();
      }
    },
  };
}

function globalTintAndAlpha() {
  const colc = 3; // decal, tile, desc
  const rowc = 8; // header, natural, red, green, blue, half-red, 50%, 50%+red
  const colw = Math.floor(screenw / colc);
  const rowh = 20; // Everything we draw is 16 pixels high, and allow some margin between.
  const x0 = (screenw >> 1) - ((colw * colc) >> 1);
  const y0 = (screenh >> 1) - ((rowh * rowc) >> 1);
  const tilebuf = new Uint8Array(6);
  const tileid = 0x19; // Dot wielding her wand.
  const srcx = (tileid & 0x0f) * 16;
  const srcy = (tileid >> 4) * 16;
  menu = {
    parent: menu,
    items: [],
    itemp: 0,
    renderExtra: () => {
      // First a bunch of text labels.
      tileRenderer.begin(texid_font, 0xffffffff, 0xff);
      tileRenderer.string("Decal", x0 + 4, y0 + (rowh >> 1) - 4);
      tileRenderer.string("Tile", x0 + colw + 4, y0 + (rowh >> 1) - 4);
      tileRenderer.string("Natural", x0 + colw * 2 + 4, y0 + rowh * 1 + (rowh >> 1) - 4);
      tileRenderer.string("Red", x0 + colw * 2 + 4, y0 + rowh * 2 + (rowh >> 1) - 4);
      tileRenderer.string("Green", x0 + colw * 2 + 4, y0 + rowh * 3 + (rowh >> 1) - 4);
      tileRenderer.string("Blue", x0 + colw * 2 + 4, y0 + rowh * 4 + (rowh >> 1) - 4);
      tileRenderer.string("Half Red", x0 + colw * 2 + 4, y0 + rowh * 5 + (rowh >> 1) - 4);
      tileRenderer.string("50%", x0 + colw * 2 + 4, y0 + rowh * 6 + (rowh >> 1) - 4);
      tileRenderer.string("50%+red", x0 + colw * 2 + 4, y0 + rowh * 7 + (rowh >> 1) - 4);
      tileRenderer.end();
      // Then draw the content row-major: Only need to set the globals once for both decal and tile.
      // To avoid muddying the waters, we will call egg.draw_decal directly instead of using TileRenderer.
      for (const [tint, alpha, row] of [
        [0x00000000, 0xff, 1],
        [0xff0000ff, 0xff, 2],
        [0x00ff00ff, 0xff, 3],
        [0x0000ffff, 0xff, 4],
        [0xff000080, 0xff, 5],
        [0x00000000, 0x80, 6],
        [0xff0000ff, 0x80, 7],
      ]) {
        egg.draw_mode(0, tint, alpha);
        egg.draw_decal(1, texid_misc, x0 + (colw >> 1) - 8, y0 + row * rowh + (rowh >> 1) - 8, srcx, srcy, 16, 16, 0);
        const dstx = x0 + colw + (colw >> 1);
        const dsty = y0 + rowh * row + (rowh >> 1);
        tilebuf[0] = dstx;
        tilebuf[1] = dstx >> 8;
        tilebuf[2] = dsty;
        tilebuf[3] = dsty >> 8;
        tilebuf[4] = tileid;
        tilebuf[5] = 0;
        egg.draw_tile(1, texid_misc, tilebuf.buffer, 1);
      }
      egg.draw_mode(0, 0, 0xff);
    },
  };
}

function offscreenRender() {
  if (texid_os1) {
    egg.texture_clear(texid_os1);
  } else {
    if (!(texid_os1 = egg.texture_new())) throw new Error(`Failed to create texture`);
    if (egg.texture_upload(texid_os1, 96, 64, 96*4, 1, null) < 0) throw new Error(`Failed to create texture`);
  }
  if (texid_os2) {
    egg.texture_clear(texid_os2);
  } else {
    if (!(texid_os2 = egg.texture_new())) throw new Error(`Failed to create texture`);
    if (egg.texture_upload(texid_os2, 96, 64, 96*4, 1, null) < 0) throw new Error(`Failed to create texture`);
  }
  egg.draw_rect(texid_os1, 0, 0, 4, 4, 0x000000ff);
  egg.draw_rect(texid_os1, 92, 0, 4, 4, 0xff0000ff);
  egg.draw_rect(texid_os1, 0, 60, 4, 4, 0x00ff00ff);
  egg.draw_rect(texid_os1, 92, 60, 4, 4, 0x0000ffff);
  egg.draw_decal(texid_os1, texid_misc, 40, 24, 192, 16, 16, 16, 0);
  egg.draw_rect(texid_os2, 0, 0, 4, 4, 0x000000ff);
  egg.draw_rect(texid_os2, 92, 0, 4, 4, 0xff0000ff);
  egg.draw_rect(texid_os2, 0, 60, 4, 4, 0x00ff00ff);
  egg.draw_rect(texid_os2, 92, 60, 4, 4, 0x0000ffff);
  egg.draw_decal(texid_os2, texid_misc, 40, 24, 208, 16, 16, 16, 0);
  menu = {
    parent: menu,
    items: [],
    itemp: 0,
    renderExtra: () => {
      // Main thing is that they appear rightside-up. That was broken when I first got here!
      egg.draw_decal(1, texid_os1, 10, 10, 0, 0, 96, 64, 0);
      egg.draw_decal(1, texid_os2, 120, 10, 0, 0, 96, 64, 0);
    },
  };
}

/* Storage menus.
 */

function enterStorageMenu() {
  menu = {
    parent: menu,
    items: [
      { label: "List Resources", cb: () => listResources() },
      { label: "Strings by Language", cb: () => stringsByLanguage() },
      { label: "Persistence", cb: () => persistence() },
    ],
    itemp: 0,
  };
}

const TID_NAMES = [ // Authority: src/opt/romr/romr.h
  "", // invalid
  "metadata",
  "wasm",
  "js",
  "image",
  "string",
  "song",
  "sound",
  "map",
];

function qualAsLang(qual) {
  if (!qual) return "0";
  const a = (qual >> 8) & 0xff;
  const b = qual & 0xff;
  // Valid language IDs are two lower-case letters, packed big-endianly:
  if ((a >= 0x61) && (a <= 0x7a) && (b >= 0x61) && (b <= 0x7a)) {
    return String.fromCharCode(a) + String.fromCharCode(b);
  }
  return qual;
}

function reprSize(size) {
  const mb = size / (1 << 20);
  if (mb >= 1) return mb.toFixed(1) + " MB";
  const kb = size / (1 << 10);
  if (kb >= 1) return kb.toFixed(1) + " kB";
  return size + " B";
}

function listResources() {
  const pageSize = Math.floor((screenh - 16) / 8);
  let page = -1; // first resource index (ie steps by pageSize)
  let content = []; // strings
  const m = {
    parent: menu,
    items: [
      { label: "Left/Right to page thru", cb: () => {} },
    ],
    itemp: 0,
    renderExtra: () => {
      tileRenderer.begin(texid_font, 0xccffddff, 0xff);
      let y = 22;
      for (const line of content) {
        tileRenderer.string(line, 6, y);
        y += 8;
      }
      tileRenderer.end();
    },
  };
  menu = m;
  m.onHorz = (dx) => {
    if (page < 0) page = 0;
    else page += dx * pageSize;
    if (page < 0) page = 0;
    m.items[0].label = `p=${page}`;
    content = [];
    for (let i=0; i<pageSize; i++) {
      const ids = egg.res_id_by_index(page + i);
      if (!ids) {
        content.push("null! This is an error");
      } else if (!ids.tid && !ids.qual && !ids.rid) {
        // This is the normal way to report end of list. Emit a blank.
        // (Do emit blank instead of breaking -- if there's something non-empty after an empty, we want to know about it!)
        content.push("");
      } else {
        const size = egg.res_get(ids.tid, ids.qual, ids.rid)?.byteLength || 0;
        const tid = TID_NAMES[ids.tid] || ids.tid;
        // (qual) can be a language ID for string, sound, image
        const qual = ((ids.tid === 4) || (ids.tid === 5) || (ids.tid === 7)) ? qualAsLang(ids.qual) : ids.qual;
        const rid = ids.rid;
        content.push(`${tid}:${qual}:${rid}: ${reprSize(size)}`);
      }
    }
  };
}

function getStringResource(qual, rid) {
  // oh my god what jackass came up with this api.... There's no "get string as string", and no obvious way to make that happen.
  // We don't have TextDecoder in QuickJS.
  // Doing this ridiculous thing, which will only work for ASCII:
  const bin = egg.res_get(5, qual, rid);
  if (!bin) return "";
  const u8 = new Uint8Array(bin);
  return Array.from(u8).map(v => String.fromCharCode(v)).join('');
}

function stringsByLanguage() {
  // Egg does not supply a convenient "get all languages of my string resources".
  // But it's not too burdensome to scan the whole set for them.
  const langs = [];
  for (let p=0; ; p++) {
    const ids = egg.res_id_by_index(p);
    if (!ids || !ids.tid) break;
    if (ids.tid !== 5) continue; // string
    if (langs.indexOf(ids.qual) >= 0) continue;
    langs.push(ids.qual);
  }
  let langsp = 0;
  const items = [];
  const m = {
    parent: menu,
    items,
    itemp: 0,
  };
  const rebuildItems = () => {
    items.splice(0, items.length);
    const qual = langs[langsp] || 0;
    for (let rid=1; rid<=4; rid++) {
      const str = getStringResource(qual, rid);
      items.push({ label: str, cb: () => {} });
    }
  };
  m.onHorz = (dx) => {
    langsp += dx;
    if (langsp < 0) langsp = langs.length - 1;
    else if (langsp >= langs.length) langsp = 0;
    rebuildItems();
  };
  rebuildItems();
  menu = m;
}

function persistence() {
  const VEGETABLES = ["asparagus", "beet", "carrot"];
  const FRUITS = ["apple", "banana", "cantaloupe"];
  const CITIES = ["aachen", "boston", "columbus"];
  let vegetable = egg.store_get("vegetable") || VEGETABLES[0];
  let fruit = egg.store_get("fruit") || FRUITS[0];
  let city = egg.store_get("city") || CITIES[0];
  const m = {
    parent: menu,
    items: [
      { label: "Left/Right to change values.", cb: () => {} },
      { label: "Quit and relaunch, they should persist.", cb: () => {} },
      { label: "", cb: () => {} },
      { label: `vegetable: ${vegetable}`, cb: () => {} },
      { label: `fruit: ${fruit}`, cb: () => {} },
      { label: `city: ${city}`, cb: () => {} },
    ],
    itemp: 0,
  };
  const dx1 = (p, dx, options) => {
    const [k, v] = m.items[p].label.split(': ');
    let vi = options.indexOf(v);
    vi += dx;
    if (vi < 0) vi = options.length - 1;
    else if (vi >= options.length) vi = 0;
    const nv = options[vi];
    m.items[p].label = `${k}: ${nv}`;
    const err = egg.store_set(k, nv);
    egg.log(`egg.store_set(${JSON.stringify(k)}, ${JSON.stringify(nv)}): ${err}`);
  };
  m.onHorz = (dx) => {
    switch (m.itemp) {
      case 3: dx1(m.itemp, dx, VEGETABLES); break;
      case 4: dx1(m.itemp, dx, FRUITS); break;
      case 5: dx1(m.itemp, dx, CITIES); break;
    }
  };
  menu = m;
}

/* Network menu.
 */

function enterNetworkMenu() {
  const wsSendInterval = 10.0; // s
  let wsSendClock = wsSendInterval;
  const m = {
    parent: menu,
    items: [
      { label: "Watch log for responses.", cb: () => {} },
      { label: "", cb: () => {} },
      { label: "HTTP Request", cb: () => {
        const url = "http://localhost:8080/index.html";
        const reqid = egg.http_request("GET", url);
        egg.log(`Requested ${url}: reqid=${reqid}`);
      }},
      { label: (gwsid > 0) ? `Websocket: ${wsconnected ? "CONNECTED" : "PENDING"} (${gwsid})` : "Websocket: DISCONNECTED", cb: () => {
        if (gwsid) {
          egg.log(`Disconnecting WebSocket ${gwsid}`);
          egg.ws_disconnect(gwsid);
          gwsid = 0;
          wsconnected = false;
        } else {
          const url = "ws://localhost:8080/ws";
          gwsid = egg.ws_connect(url);
          wsconnected = false;
          if (gwsid > 0) egg.log(`Beginning connection to ${url}, wsid=${gwsid}`);
          else egg.log(`egg.ws_connect(${JSON.stringify(url)}): ${gwsid}`);
        }
        m.items[3].label = (gwsid > 0) ? `Websocket: ${wsconnected ? "CONNECTED" : "PENDING"} (${gwsid})` : "Websocket: DISCONNECTED";
      }},
    ],
    itemp: 0,
    onWsConnect: () => {
      m.items[3].label = (gwsid > 0) ? `Websocket: ${wsconnected ? "CONNECTED" : "PENDING"} (${gwsid})` : "Websocket: DISCONNECTED";
    },
    onWsDisconnect: () => {
      m.items[3].label = (gwsid > 0) ? `Websocket: ${wsconnected ? "CONNECTED" : "PENDING"} (${gwsid})` : "Websocket: DISCONNECTED";
    },
    update: (elapsed) => {
      if ((wsSendClock -= elapsed) <= 0) {
        wsSendClock += wsSendInterval;
        if (wsconnected) {
          const msg = "Hello this is the client.";
          egg.log(`Sending over Websocket: ${msg}`);
          egg.ws_send(wsid, 0, msg);
        }
      }
    },
  };
  menu = m;
}

/* Misc menu.
 */

function enterMiscMenu() {
  menu = {
    parent: menu,
    items: [
      { label: "Log", cb: () => {
        egg.log("Test of log formatting. Next line should say: String Truncate Len c 123 0x7b 123.456");
        egg.log("%s %.8s %.*s %c %d 0x%x %f", "String", "Truncated String", 3, "Length First", 0x63, 123, 123, 123.456);
      }},
      { label: "Time", cb: () => showTime() },
      { label: "Languages", cb: () => showLanguages() },
      { label: `Is Terminable? ${egg.is_terminable()?"TRUE":"FALSE"}`, cb: () => {} },
      { label: "Terminate", cb: () => egg.request_termination() },
    ],
    itemp: 0,
  };
}

function showTime() {
  const realTimeStart = egg.time_real();
  let elapsedGameTime = 0;
  const pad = (v, w) => {
    const s = v.toString();
    return "00000000".substring(0, w - s.length) + s;
  };
  let minUpdate = 999;
  let maxUpdate = 0;
  menu = {
    parent: menu,
    items: [],
    itemp: 0,
    update: (elapsed) => {
      elapsedGameTime += elapsed;
      if (elapsed < minUpdate) minUpdate = elapsed;
      if (elapsed > maxUpdate) maxUpdate = elapsed;
    },
    renderExtra: () => {
      const now = egg.time_real();
      const elapsedRealTime = now - realTimeStart;
      const split = egg.time_get();
      tileRenderer.begin(texid_font, 0xffffffff, 0xff);
      tileRenderer.string(`Game: ${elapsedGameTime.toFixed(3)}`, 8, 8);
      tileRenderer.string(`Real: ${elapsedRealTime.toFixed(3)}`, 8, 16);
      tileRenderer.string(`Date: ${pad(split.year, 4)}-${pad(split.month, 2)}-${pad(split.day, 2)}`, 8, 24);
      tileRenderer.string(`Time: ${pad(split.hour, 2)}:${pad(split.minute, 2)}:${pad(split.second, 2)}.${pad(split.milli, 3)}`, 8, 32);
      tileRenderer.string(`Updates: ${minUpdate.toFixed(3)} .. ${maxUpdate.toFixed(3)}`, 8, 40);
      tileRenderer.string("Game and Real may skew under load.", 8, 56);
      tileRenderer.string("Date and Time are local.", 8, 64);
      tileRenderer.end();
    },
  };
}

function showLanguages() {
  const languageCodes = egg.get_user_languages();
  egg.log(`languageCodes: ${JSON.stringify(languageCodes)}`);
  const languageNames = languageCodes.map(code => qualAsLang(code).toString());
  menu = {
    parent: menu,
    items: [],
    itemp: 0,
    renderExtra: () => {
      tileRenderer.begin(texid_font, 0xffffffff, 0xff);
      tileRenderer.string("User's preferred languages:", 8, 8);
      let y = 16;
      for (const name of languageNames) {
        tileRenderer.string(name, 16, y);
        y += 8;
      }
      tileRenderer.end();
    },
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
  
  const ldtx = (imageid) => {
    const texid = egg.texture_new();
    if (egg.texture_load_image(texid, 0, imageid) < 0) throw new Error(`Failed to load image:0:${imageid}`);
    return texid;
  };
  texid_font = ldtx(2);
  texid_misc = ldtx(3);
  
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
  
  songid = 1;
  //egg.audio_play_song(0, songid, false, true);
  
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
  if (menu.onHorz) {
    if (dx < 0) menu.onHorz(-1);
    else if (dx > 0) menu.onHorz(1);
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

function onConnect(devid, mapping) {
  devidv.push(devid);
  egg.log(`CONNECT devid=${devid} mapping=${mapping}`);
  egg.log(`  name=${egg.input_device_get_name(devid)}`);
  egg.log(`  ids=${JSON.stringify(egg.input_device_get_ids(devid))}`);
}

function onDisconnect(devid) {
  egg.log(`DISCONNECT devid=${devid}`);
  const p = devidv.indexOf(devid);
  if (p >= 0) {
    devidv.splice(p, 1);
  }
}

function onHttpResponse(reqid, status, length) {
  egg.log(`HTTP_RSP reqid=${reqid} status=${status} length=${length}`);
}

function onWsConnect(wsid) {
  egg.log(`WS_CONNECT wsid=${wsid}`);
  if (wsid === gwsid) {
    wsconnected = true;
    if (menu.onWsConnect) menu.onWsConnect();
  }
}

function onWsDisconnect(wsid) {
  egg.log(`WS_DISCONNECT wsid=${wsid}`);
  if (wsid === gwsid) {
    gwsid = 0;
    wsconnected = false;
    if (menu.onWsDisconnect) menu.onWsDisconnect();
  }
}

function onWsMessage(wsid, msgid, length) {
  egg.log(`WS_MESSAGE wsid=${wsid} msgid=${msgid} length=${length}`);
}

function egg_client_update(elapsed) {
  if (menu.update) menu.update(elapsed);
  for (const event of egg.event_next()) switch (event.eventType) {
    case 1: if (menu.onJoystickInput) menu.onJoystickInput(event.v0, event.v1, event.v2); break;
    case 2: onConnect(event.v0, event.v1); break;
    case 3: onDisconnect(event.v0); break;
    case 4: onHttpResponse(event.v0, event.v1, event.v2); break;
    case 5: onWsConnect(event.v0); break;
    case 6: onWsDisconnect(event.v0); break;
    case 7: onWsMessage(event.v0, event.v1, event.v2); break;
    case 8: if (menu.onMouseMotion) menu.onMouseMotion(event.v0, event.v1); break;
    case 9: if (menu.onMouseButton) {
        menu.onMouseButton(event.v0, event.v1, event.v2, event.v3);
      } else if ((event.v0 === 1) && event.v1) {
        menuSelectPoint(event.v2, event.v3);
      } else if ((event.v0 >= 2) && event.v1) {
        menuExit();
      } break;
    case 10: if (menu.onMouseWheel) menu.onMouseWheel(event.v0, event.v1, event.v2, event.v3); break;
    case 11: { // KEY goes to the menu and *also* to the general processor. You can still press Esc to exit the Keyboard menu.
        if (menu.onKey) menu.onKey(event.v0, event.v1);
        if (event.v1) onKeyDown(event.v0);
      } break;
    case 12: if (menu.onText) menu.onText(event.v0); break;
    case 13: {
        if (menu.onTouch) menu.onTouch(event.v0, event.v1, event.v2, event.v3);
        if (event.v1 === 1) menuSelectPoint(event.v2, event.v3); break;//TODO confirm state==1 means Begin
      } break;
    //default: egg.log(`UNKNOWN EVENT ${event.eventType} [${event.v0},${event.v1},${event.v2},${event.v3}]`); break;
  }
}

/* Render.
 */

function egg_client_render() {
  egg.draw_rect(1, 0, 0, screenw, screenh, 0x5060c0ff);
  if (menu) {
    if (menu.renderExtra) menu.renderExtra();
    if ((menu.itemp >= 0) && (menu.itemp < menu.items.length)) {
      egg.draw_rect(1, 4, 8 + menu.itemp * 8 - 1, screenw - 8, 9, 0x000000ff);
    }
    tileRenderer.begin(texid_font, 0xffffffff, 0xff);
    let y = 12;
    for (const { label } of menu.items) {
      tileRenderer.string(label, 12, y);
      y += 8;
    }
    tileRenderer.end();
  }
}

exportModule({
  egg_client_quit,
  egg_client_init,
  egg_client_update,
  egg_client_render,
});
