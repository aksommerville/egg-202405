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

const EGG_EVTSTATE_QUERY      = 0;
const EGG_EVTSTATE_IMPOSSIBLE = 1;
const EGG_EVTSTATE_DISABLED   = 2;
const EGG_EVTSTATE_ENABLED    = 3;
const EGG_EVTSTATE_REQUIRED   = 4;

let glprogram = 0;
let posbuffer = 0;
let colorbuffer = 0;

egg.log("1.js outer");

function egg_client_quit() {
}

function compile_shader(gl, type, src) {
  const shader=gl.createShader(type);
  gl.shaderSource(shader,src);
  gl.compileShader(shader);
  const status = gl.getShaderParameter(shader,gl.COMPILE_STATUS);
  if (!status) {
    egg.log("GLSL COMPILE FAILED (%s)",(type==gl.VERTEX_SHADER)?"VERTEX":"FRAGMENT");
    const infolog = gl.getShaderInfoLog(shader);
    egg.log("%s",infolog.trim());
    gl.deleteShader(shader);
    return -1;
  }
  gl.attachShader(glprogram,shader);
  gl.deleteShader(shader);
  return 0;
}

function egg_client_init() {
  egg.log("1.js egg_client_init");

  // Mouse and text events start off disabled. Uncomment to enable:
  // egg.event_enable(EGG_EVENT_MMOTION, EGG_EVTSTATE_ENABLED);
  // egg.event_enable(EGG_EVENT_MBUTTON, EGG_EVTSTATE_ENABLED);
  // egg.event_enable(EGG_EVENT_MWHEEL, EGG_EVTSTATE_ENABLED);
  // egg.event_enable(EGG_EVENT_TEXT, EGG_EVTSTATE_ENABLED);
  
  const gl = egg.video_get_context();
  if (!(glprogram=gl.createProgram())) return -1;
  if (compile_shader(gl, gl.VERTEX_SHADER,
    `precision mediump float;
    attribute vec2 aposition;
    attribute vec4 acolor;
    varying vec4 vcolor;
    void main() {
      gl_Position=vec4(aposition,0.0,1.0);
      vcolor=acolor;
      gl_PointSize=32.0;
    }
  `)<0) return -1;
  if (compile_shader(gl, gl.FRAGMENT_SHADER,
    `precision mediump float;
    varying vec4 vcolor;
    void main() {
      //vec2 normpos=(gl_PointCoord-0.5)*2.0;
      //float a=1.0-sqrt(normpos.x*normpos.x+normpos.y*normpos.y);
      //gl_FragColor=vec4(vcolor.rgb,a);
      gl_FragColor=vcolor;
    }
  `)<0) return -1;
  gl.linkProgram(glprogram);
  const status = gl.getProgramParameter(glprogram,gl.LINK_STATUS);
  if (!status) {
    egg.log("GLSL LINK FAILED");
    const infolog = gl.getProgramInfoLog(glprogram);
    egg.log("%s",infolog.trim());
    return -1;
  }
  gl.bindAttribLocation(glprogram,0,"aposition");
  gl.bindAttribLocation(glprogram,1,"acolor");
  if (!(posbuffer = gl.createBuffer())) return -1;
  if (!(colorbuffer = gl.createBuffer())) return -1;
  
  return 0;
}

function onKey(keycode, value) {
  //egg.log("KEY %08x=%d", keycode, value);
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

function onInputConnect(devid) {
  egg.log("CONNECT %d", devid);
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
      case EGG_EVENT_CONNECT: onInputConnect(event.v0); break;
      case EGG_EVENT_DISCONNECT: egg.log("DISCONNECT %d", event.v0); break;
      case EGG_EVENT_HTTP_RSP: {
          egg.log("HTTP_RSP reqid=%d status=%d length=%d zero=%d", event.v0, event.v1, event.v2, event.v3);
          const body = egg.http_get_body(event.v0);
          egg.log(">> %s", body);
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
      default: egg.log("UNKNOWN EVENT: %s", JSON.stringify(event));
    }
  }
}

function egg_client_render(gl) {
  const [screenw, screenh] = egg.video_get_size();
  gl.viewport(0,0,screenw,screenh);
  gl.clearColor(0.5,0.25,0.0,1.0);
  gl.clear(gl.COLOR_BUFFER_BIT);
  
  {
    gl.useProgram(glprogram);
    gl.enable(gl.BLEND);
    gl.blendFuncSeparate(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA,gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA);
    gl.enableVertexAttribArray(0);
    gl.enableVertexAttribArray(1);
    gl.bindBuffer(gl.ARRAY_BUFFER, posbuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([0.0,0.5,-0.5,-0.5,0.5,-0.5]).buffer, gl.STREAM_DRAW);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
    gl.bindBuffer(gl.ARRAY_BUFFER, colorbuffer);
    gl.bufferData(gl.ARRAY_BUFFER, new Uint8Array([0xff,0x00,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0x00,0xff,0xff]).buffer, gl.STREAM_DRAW);
    gl.vertexAttribPointer(1, 4, gl.UNSIGNED_BYTE, true, 0, 0);
    gl.drawArrays(gl.TRIANGLES,0,3);
    gl.disableVertexAttribArray(0);
    gl.disableVertexAttribArray(1);
    gl.useProgram(null);
  }
}

exportModule({
  egg_client_init,
  egg_client_quit,
  egg_client_update,
  egg_client_render,
});
