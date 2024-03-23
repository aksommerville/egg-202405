//import * as egg from "egg";

const EGG_EVENT_INPUT         =  1; /* [devid,btnid,value,_] */
const EGG_EVENT_CONNECT       =  2; /* [devid,_,_,_] */
const EGG_EVENT_DISCONNECT    =  3; /* [devid,_,_,_] */
const EGG_EVENT_HTTP_RSP      =  4; /* [reqid,status,length,_] */
const EGG_EVENT_WS_CONNECT    =  5; /* [wsid,_,_,_] */
const EGG_EVENT_WS_DISCONNECT =  6; /* [wsid,_,_,_] */
const EGG_EVENT_WS_MESSAGE    =  7; /* [wsid,msgid,opcode,length] */
const EGG_EVENT_MMOTION       =  8; /* [x,y,_,_] */
const EGG_EVENT_MBUTTON       =  9; /* [btnid,value,x,y] */
const EGG_EVENT_MWHEEL        = 10; /* [dx,dy,x,y] */
const EGG_EVENT_KEY           = 11; /* [hidusage,value,_,_] */
const EGG_EVENT_TEXT          = 12; /* [codepoint,_,_,_] */
const EGG_EVENT_RESIZE        = 13; /* [w,h,_,_] */
const EGG_EVENT_TOUCH         = 14; /* [id,state,x,y] */

const EGG_EVTSTATE_QUERY      = 0;
const EGG_EVTSTATE_IMPOSSIBLE = 1;
const EGG_EVTSTATE_DISABLED   = 2;
const EGG_EVTSTATE_ENABLED    = 3;
const EGG_EVTSTATE_REQUIRED   = 4;

const EGG_TEX_FMT_UNSPEC = 0;
const EGG_TEX_FMT_RGBA = 1;
const EGG_TEX_FMT_A8 = 2;
const EGG_TEX_FMT_A1 = 3;
const EGG_TEX_FMT_Y8 = 4;
const EGG_TEX_FMT_Y1 = 5;

const EGG_XFERMODE_ALPHA = 0;
const EGG_XFERMODE_OPAQUE = 1;

const EGG_XFORM_XREV = 1;
const EGG_XFORM_YREV = 2;
const EGG_XFORM_SWAP = 4;

let screenw=0,screenh=0;

let texid_dot=0;
let texid_tilesheet=0;
let texid_title=0;
let texid_rotate=0;

const SPRITEA = 500;
const spritev = []; // {x,y,dx,dy,tileid,xform}

const tilebuf = new ArrayBuffer(Math.max(SPRITEA,100) * 6);
const tilebuf8 = new Uint8Array(tilebuf);

egg.log("1.js outer");

function egg_client_quit() {
}

function egg_client_init() {
  egg.log("1.js egg_client_init");

  // Mouse and text events start off disabled. Uncomment to enable:
  // egg.event_enable(EGG_EVENT_MMOTION, EGG_EVTSTATE_ENABLED);
  // egg.event_enable(EGG_EVENT_MBUTTON, EGG_EVTSTATE_ENABLED);
  // egg.event_enable(EGG_EVENT_MWHEEL, EGG_EVTSTATE_ENABLED);
  // egg.event_enable(EGG_EVENT_TEXT, EGG_EVTSTATE_ENABLED);
  egg.event_enable(EGG_EVENT_TOUCH, EGG_EVTSTATE_ENABLED);
  
  //const reqid = egg.http_request("GET", "http://localhost:8081/ws_client.html");
  //const wsid = egg.ws_connect("ws://localhost:8081/ws");
  //console.log(`reqid=${reqid} wsid=${wsid}`);
  
  const t1hdr = egg.texture_get_header(1);
  screenw = t1hdr.w;
  screenh = t1hdr.h;
  
  if (!(texid_dot=egg.texture_new())) return -1;
  if (!(texid_tilesheet=egg.texture_new())) return -1;
  if (!(texid_title=egg.texture_new())) return -1;
  if (!(texid_rotate=egg.texture_new())) return -1;
  if (egg.texture_load_image(texid_dot,0,1)<0) {
    egg.log("failed to load image:1");
    return -1;
  }
  if (egg.texture_load_image(texid_tilesheet,0,2)<0) {
    egg.log("failed to load image:2");
    return -1;
  }
  if (egg.texture_load_image(texid_title,0,3)<0) {
    egg.log("failed to load image:3");
    return -1;
  }
  if (egg.texture_load_image(texid_rotate,0,4)<0) {
    egg.log("failed to load image:4");
    return -1;
  }
  
  {
    const max_speed=3;
    const speeds_count=(max_speed<<1)+1;
    while (spritev.length < SPRITEA) {
      const x=Math.floor(Math.random()*screenw);
      const y=Math.floor(Math.random()*screenh);
      let tileid;
      switch (Math.floor(Math.random()*3)) {
        case 0: tileid=0x00; break;
        case 1: tileid=0x01; break;
        case 2: tileid=0xff; break;
      }
      const xform=Math.floor(Math.random()*8);
      let dx,dy;
      do {
        dx=Math.floor(Math.random()*speeds_count)-max_speed;
        dy=Math.floor(Math.random()*speeds_count)-max_speed;
      } while (!dx||!dy);
      spritev.push({ x, y, tileid, xform, dx, dy });
    }
  }
  
  return 0;
}

function onKey(keycode, value) {
  egg.log("KEY %08x=%d", keycode, value);
  // (keycode) should be USB-HID page 7.
  // Platforms are expected to translate to that space if their underlying system provides something else.
  if (value) switch (keycode) {
    case 0x00070029: egg.request_termination(); break; // ESC: quit
    case 0x0007003a: { // F1: Toggle mouse.
        if (egg.event_enable(EGG_EVENT_MMOTION, EGG_EVTSTATE_QUERY) === EGG_EVTSTATE_ENABLED) {
          egg.event_enable(EGG_EVENT_MMOTION, EGG_EVTSTATE_DISABLED);
          egg.event_enable(EGG_EVENT_MBUTTON, EGG_EVTSTATE_DISABLED);
          egg.event_enable(EGG_EVENT_MWHEEL, EGG_EVTSTATE_DISABLED);
        } else {
          egg.event_enable(EGG_EVENT_MMOTION, EGG_EVTSTATE_ENABLED);
          egg.event_enable(EGG_EVENT_MBUTTON, EGG_EVTSTATE_ENABLED);
          egg.event_enable(EGG_EVENT_MWHEEL, EGG_EVTSTATE_ENABLED);
        }
      } break;
    case 0x0007003b: { // F2: Toggle text.
        if (egg.event_enable(EGG_EVENT_TEXT, EGG_EVTSTATE_QUERY) === EGG_EVTSTATE_ENABLED) {
          egg.event_enable(EGG_EVENT_TEXT, EGG_EVTSTATE_DISABLED);
        } else {
          egg.event_enable(EGG_EVENT_TEXT, EGG_EVTSTATE_ENABLED);
        }
      } break;
  }
}

function onInputConnect(devid, mapping) {
  egg.log("CONNECT %d, %s mapping", devid, (mapping === 1) ? "STANDARD" : "RAW");
  const name = egg.input_device_get_name(devid);
  const ids = egg.input_device_get_ids(devid);
  egg.log(`name=${JSON.stringify(name)} vid=${ids?.vid} pid=${ids?.pid} version=${ids?.version}`);
  for (let btnix=0; ; btnix++) {
    const button = egg.input_device_get_button(devid, btnix);
    if (!button) break;
    egg.log("  %d 0x%08x %d..%d =%d", button.btnid, button.hidusage, button.lo, button.hi, button.value);
  }
}

let i=0;

function egg_client_update(elapsed) {
  for (const event of egg.event_next()) {
    switch (event.eventType) {
      case EGG_EVENT_INPUT: egg.log("INPUT %d.%d=%d", event.v0, event.v1, event.v2); break;
      case EGG_EVENT_CONNECT: onInputConnect(event.v0, event.v1); break;
      case EGG_EVENT_DISCONNECT: egg.log("DISCONNECT %d", event.v0); break;
      case EGG_EVENT_HTTP_RSP: {
          egg.log("HTTP_RSP reqid=%d status=%d length=%d zero=%d", event.v0, event.v1, event.v2, event.v3);
          const body = egg.http_get_body(event.v0);
          egg.log(">> %s...", body.substring(0,100));
        } break;
      case EGG_EVENT_WS_CONNECT: egg.log("WS_CONNECT %d", event.v0); break;
      case EGG_EVENT_WS_DISCONNECT: egg.log("WS_DISCONNECT %d", event.v0); break;
      case EGG_EVENT_WS_MESSAGE: {
          egg.log("WS_MESSAGE wsid=%d msgid=%d opcode=%d len=%d", event.v0, event.v1, event.v2, event.v3);
          const msg = egg.ws_get_message(event.v0, event.v1);
          egg.log(">> %s", msg);
        } break;
      case EGG_EVENT_MMOTION: egg.log("MMOTION %d,%d", event.v0, event.v1); break;
      case EGG_EVENT_MBUTTON: egg.log("MBUTTON %d=%d @%d,%d", event.v0, event.v1, event.v2, event.v3); break;
      case EGG_EVENT_MWHEEL: egg.log("MWHEEL +%d,%d @%d,%d", event.v0, event.v1, event.v2, event.v3); break;
      case EGG_EVENT_KEY: onKey(event.v0, event.v1); break;
      case EGG_EVENT_TEXT: egg.log("TEXT U+%x", event.v0); break;
      case EGG_EVENT_RESIZE: egg.log("RESIZE %d,%d", event.v0, event.v1); break;
      case EGG_EVENT_TOUCH: egg.log("TOUCH #%d =%d @%d,%d", event.v0, event.v1, event.v2, event.v3); break;
      default: egg.log("UNKNOWN EVENT: %s", JSON.stringify(event));
    }
  }
  
  for (const sprite of spritev) {
    sprite.x+=sprite.dx;
    sprite.y+=sprite.dy;
    if ((sprite.x<0)&&(sprite.dx<0)) sprite.dx=-sprite.dx;
    else if ((sprite.x>=screenw)&&(sprite.dx>0)) sprite.dx=-sprite.dx;
    if ((sprite.y<0)&&(sprite.dy<0)) sprite.dy=-sprite.dy;
    else if ((sprite.y>=screenh)&&(sprite.dy>0)) sprite.dy=-sprite.dy;
  }
}

let tr=0,tg=0,tb=0,ta=0,ga=0;
let dtr=1,dtg=5,dtb=3,dta=2,dga=1;

function egg_client_render() {
  egg.draw_rect(1,0,0,screenw,screenh,0x008000ff);
  egg.draw_rect(1,10,10,screenw-20,screenh-20,0xff0000ff);
  egg.draw_rect(1,11,11,screenw-22,screenh-22,0x0000ffff);
  egg.draw_rect(1,78,0,screenw,screenh,0xffffff80);
  egg.draw_rect(1,0,0,40,30,0x00000080);
  
  { // Lots of sprites with random xform and motion.
    let bufp=0;
    for (const sprite of spritev) {
      tilebuf8[bufp++] = sprite.x;
      tilebuf8[bufp++] = sprite.x >> 8;
      tilebuf8[bufp++] = sprite.y;
      tilebuf8[bufp++] = sprite.y >> 8;
      tilebuf8[bufp++] = sprite.tileid;
      tilebuf8[bufp++] = sprite.xform;
    }
    egg.draw_tile(1,texid_tilesheet,tilebuf,spritev.length);
  }
  
  // Javascript needs the C preprocessor. Write your congressman.
  tr+=dtr; if (tr<0) { tr=0; dtr=-dtr; } else if (tr>0xff) { tr=0xff; dtr=-dtr; }
  tg+=dtg; if (tg<0) { tg=0; dtg=-dtg; } else if (tg>0xff) { tg=0xff; dtg=-dtg; }
  tb+=dtb; if (tb<0) { tb=0; dtb=-dtb; } else if (tb>0xff) { tb=0xff; dtb=-dtb; }
  ta+=dta; if (ta<0) { ta=0; dta=-dta; } else if (ta>0xff) { ta=0xff; dta=-dta; }
  ga+=dga; if (ga<0) { ga=0; dga=-dga; } else if (ga>0xff) { ga=0xff; dga=-dga; }
  
  // Game's title, fading in and out via global alpha.
  egg.draw_mode(EGG_XFERMODE_ALPHA,0,ga);
  const titleinfo = egg.texture_get_header(texid_title);
  egg.draw_decal(1,texid_title,0,0,0,0,titleinfo.w,titleinfo.h,0);
  
  // Tiles and a decal with animated tint.
  egg.draw_mode(EGG_XFERMODE_ALPHA,(tr<<24)|(tg<<16)|(tb<<8)|ta,0xff);
  {
    let bufp=0;
    const addvtx = (x,y,tileid,xform) => {
      tilebuf8[bufp++] = x;
      tilebuf8[bufp++] = x >> 8;
      tilebuf8[bufp++] = y;
      tilebuf8[bufp++] = y >> 8;
      tilebuf8[bufp++] = tileid;
      tilebuf8[bufp++] = xform;
    };
    addvtx(100,100,0x00,0);
    addvtx(120,100,0x01,0);
    addvtx(140,100,0xff,0);
    addvtx(160,100,0x02,0);
    egg.draw_tile(1,texid_tilesheet,tilebuf,4);
  }
  egg.draw_decal(1,texid_dot,70,92,0,0,16,16,0);
  egg.draw_mode(EGG_XFERMODE_ALPHA,0,0xff);
  
  { // Transforms reference, and transformed tiles.
    let bufp=0,tilec=0;
    const addvtx = (x,y,tileid,xform) => {
      tilebuf8[bufp++] = x;
      tilebuf8[bufp++] = x >> 8;
      tilebuf8[bufp++] = y;
      tilebuf8[bufp++] = y >> 8;
      tilebuf8[bufp++] = tileid;
      tilebuf8[bufp++] = xform;
      tilec++;
    };
    addvtx( 60,120,0x10,0); // reference...
    addvtx( 90,120,0x11,0);
    addvtx(120,120,0x12,0);
    addvtx(150,120,0x13,0);
    addvtx(180,120,0x14,0);
    addvtx(210,120,0x15,0);
    addvtx(240,120,0x16,0);
    addvtx(270,120,0x17,0); // ...reference
    addvtx( 60,140,0x10,0); // xform...
    addvtx( 90,140,0x10,1);
    addvtx(120,140,0x10,2);
    addvtx(150,140,0x10,3);
    addvtx(180,140,0x10,4);
    addvtx(210,140,0x10,5);
    addvtx(240,140,0x10,6);
    addvtx(270,140,0x10,7); // ...xform
    egg.draw_tile(1,texid_tilesheet,tilebuf,tilec);
  }
  { // Transformed decals.
    egg.draw_decal(1,texid_rotate, 48,150,8,8,24,16,0); // none
    egg.draw_decal(1,texid_rotate, 78,150,8,8,24,16,1); // XREV
    egg.draw_decal(1,texid_rotate,108,150,8,8,24,16,2); // YREV
    egg.draw_decal(1,texid_rotate,138,150,8,8,24,16,3); // XREV|YREV
    egg.draw_decal(1,texid_rotate,168,150,8,8,24,16,4); // SWAP
    egg.draw_decal(1,texid_rotate,198,150,8,8,24,16,5); // SWAP|XREV
    egg.draw_decal(1,texid_rotate,228,150,8,8,24,16,6); // SWAP|YREV
    egg.draw_decal(1,texid_rotate,258,150,8,8,24,16,7); // SWAP|XREV|YREV
  }
  
}

exportModule({
  egg_client_init,
  egg_client_quit,
  egg_client_update,
  egg_client_render,
});
