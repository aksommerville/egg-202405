import * as egg from "egg";

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
}

function egg_client_render() {
  const [screenw, screenh] = egg.video_get_size();
}

exportModule({
  egg_client_init,
  egg_client_quit,
  egg_client_update,
  egg_client_render,
});
