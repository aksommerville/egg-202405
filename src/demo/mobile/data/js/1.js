/* mobile/1.js
 * An Egg demo to show off touch and accelerometer inputs.
 * The Lights-On demos kind of assume that you have a keyboard, and I don't feel like reworking them.
 */

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

/* And on with the demo...
 */
 
const TOUCH_LIMIT = 100;
 
let screenw, screenh;
let texid_hand;
let texid_font;
const touches = []; // [x,y]
const accel = [0, 0, 0, 0, 0, 0, 0, 0, 0]; // lox, loy, loz, curx, cury, curz, hix, hiy, hiz

function egg_client_init() {

  const fb = egg.texture_get_header(1);
  screenw = fb.w;
  screenh = fb.h;
  
  texid_hand = egg.texture_new();
  if (egg.texture_load_image(texid_hand, 0, 1) < 0) return -1;
  texid_font = egg.texture_new();
  if (egg.texture_load_image(texid_font, 0, 2) < 0) return -1;
  
  egg.event_enable(13, 3); // TOUCH
  egg.event_enable(14, 3); // ACCELEROMETER
}

function egg_client_update(elapsed) {
  for (const event of egg.event_next()) switch (event.eventType) {
  
    case 13: { // TOUCH [id,state,x,y]
        touches.push([event.v2, event.v3]);
        if (touches.length > TOUCH_LIMIT) {
          touches.splice(0, touches.length - TOUCH_LIMIT);
        }
        if (event.v1 === 1) { // each press, reset the accelerometer's sticky report
          accel[0] = accel[6] = accel[3];
          accel[1] = accel[7] = accel[4];
          accel[2] = accel[8] = accel[5];
        }
      } break;
      
    case 14: { // ACCELEROMETER [x,y,z,_] s16.16
        const x = event.v0 / 65536.0;
        const y = event.v1 / 65536.0;
        const z = event.v2 / 65536.0;
        accel[3] = x;
        accel[4] = y;
        accel[5] = z;
             if (x < accel[0]) accel[0] = x;
        else if (x > accel[6]) accel[6] = x;
             if (y < accel[1]) accel[0] = y;
        else if (y > accel[7]) accel[6] = y;
             if (z < accel[2]) accel[0] = z;
        else if (z > accel[8]) accel[6] = z;
      } break;
  }
}

function reprAccel1(v) {
  if (v < 0) return v.toFixed(3);
  return "+" + v.toFixed(3);
}

function reprAccel(p0) {
  return reprAccel1(accel[p0]) + " " + reprAccel1(accel[p0+1]) + " " + reprAccel1(accel[p0+2]);
}

function egg_client_render() {
  egg.draw_rect(1, 0, 0, screenw, screenh, 0x104020ff);
  for (const [x, y] of touches) {
    egg.draw_decal(1, texid_hand, x - 16, y - 16, 0, 0, 32, 32, 0);
  }
  tileRenderer.begin(texid_font, 0xffffffff, 0x80);
  tileRenderer.string(reprAccel(0), 8, 8);
  tileRenderer.end();
  tileRenderer.begin(texid_font, 0xffffffff, 0xff);
  tileRenderer.string(reprAccel(3), 8, 16);
  tileRenderer.end();
  tileRenderer.begin(texid_font, 0xffffffff, 0x80);
  tileRenderer.string(reprAccel(6), 8, 24);
  tileRenderer.end();
}
 
exportModule({
  egg_client_init,
  egg_client_update,
  egg_client_render,
});
