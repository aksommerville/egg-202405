/* egg/index.d.ts
 * Declarations for Egg's Javascript API.
 */
 
/* You must call this once at global scope to register your entry points with Egg's runtime.
 * At least one of (update, render) must be implemented.
 */
declare function exportModule(module: {
  egg_client_quit?: () => void,
  egg_client_init?: () => number,
  egg_client_update?: (elapsed: number) => void,
  egg_client_render?: () => void,
});

/* Aside from exportModule, everything else lives in a global object called "egg".
 */
declare namespace egg {

/* Input.
 ***********************************************************************/

// EGG_EVENT_ in the C API.
const enum EventType {
  INPUT         =  1, /* [devid,btnid,value,_] */
  CONNECT       =  2, /* [devid,mapping,_,_] */
  DISCONNECT    =  3, /* [devid,_,_,_] */
  HTTP_RSP      =  4, /* [reqid,status,length,_] */
  WS_CONNECT    =  5, /* [wsid,_,_,_] */
  WS_DISCONNECT =  6, /* [wsid,_,_,_] */
  WS_MESSAGE    =  7, /* [wsid,msgid,length,_] */
  MMOTION       =  8, /* [x,y,_,_] */
  MBUTTON       =  9, /* [btnid,value,x,y] (1,2,3)=(left,right,middle) */
  MWHEEL        = 10, /* [dx,dy,x,y] */
  KEY           = 11, /* [hidusage,value,_,_] */
  TEXT          = 12, /* [codepoint,_,_,_] */
  TOUCH         = 13, /* [id,state(0,1,2),x,y] (0,1,2)=(release,press,move) */
  ACCELEROMETER = 14, /* [x,y,z,_] m/s**2 s16.16 */
}

const enum GamepadMapping {
  RAW = 0,
  STANDARD = 1, /* https://w3c.github.io/gamepad/#remapping */
}

interface Event {
  eventType: EventType,
  v0: number,
  v1: number,
  v2: number,
  v3: number,
}

function event_next(): Event[];

/**
 * Ask for one bit of the event mask, or change one.
 * In your request:
 *   QUERY: Don't change anything, just report the current state.
 *   DISABLED: Turn off.
 *   ENABLED: Turn on if available, and do not take any extraordinary measures.
 *   REQUIRED: Turn on if available, or fake it if possible.
 * We respond:
 *   IMPOSSIBLE: Feature is not available. (If you asked ENABLED, REQUIRED might work).
 *   DISABLED: Feature is disabled.
 *   ENABLED: Feature is enabled.
 *   REQUIRED: Feature is enabled and can not be disabled.
 * Enabling any of (MMOTION,MBUTTON,MWHEEL) should make the cursor visible.
 */
function event_enable(eventType: EventType, eventState: EventState): EventState;

// EGG_EVTSTATE_* in the C API.
const enum EventState {
  QUERY = 0,
  IMPOSSIBLE = 1,
  DISABLED = 2,
  ENABLED = 3,
  REQUIRED = 4,
}

/* You'll want to do these after EGG_EVENT_CONNECT to examine each new device.
 */
function input_device_get_name(devid: number): string;
function input_device_get_ids(devid: number): { vid: number, pid: number, version: number };
function input_device_get_button(devid: number, index: number): { btnid: number, hidusage: number, lo: number, hi: number, value: number } | null;

function input_device_disconnect(devid: number): void;

/**
 * Cursor is always hidden when MMOTION, MBUTTON, and MWHEEL are disabled.
 * By default, we show the system cursor when any of those is enabled, if there is a system cursor.
 * Send false here to keep it hidden always.
 */
function show_cursor(show: boolean): void;

/* Video.
 ****************************************************************/

const enum TextureFormat {
  UNSPEC = 0,
  RGBA = 1,
  A8 = 2,
  A1 = 3,
  Y8 = 4,
  Y1 = 5,
}

const enum XferMode {
  ALPHA = 0,
  OPAQUE = 1,
}

const enum Xform {
  NONE = 0,
  XREV = 1, // flop
  YREV = 2, // flip
  XREV_YREV = 3, // 180
  SWAP = 4,
  XREV_SWAP = 5, // counterclockwise
  YREV_SWAP = 6, // clockwise
  XREV_YREV_SWAP = 7,
}

/* Decode image resources the same way texture_load_image() would.
 * You don't need this in normal cases, only if you're doing software rendering client-side or something.
 * It's split into two functions to match the C API, though on the JS side it could have been just one.
 * The image's stride is the ArrayBuffer's length divided by height, which will always divide exactly.
 */
function image_get_header(qual: number, imageid: number): { w: number; h: number; fmt: TextureFormat };
function image_decode(qual: number, imageid: number): ArrayBuffer | null;

/* Create or delete a texture.
 * Platform will have some internal limit, but it's not publically defined and may vary across hosts.
 * (texid) is a positive integer or zero for errors (never negative).
 * Texid One is the main output. It always exists and can't be resized.
 */
function texture_del(texid: number): void;
function texture_new(): number;

/* Populate texture from an image resource or raw client-side pixels.
 * You may upload with empty data to create the texture with all pixels zeroed.
 * Otherwise, (srcc==stride*h).
 * These return <0 on errors.
 */
function texture_load_image(texid: number, qual: number, imageid: number): number;
function texture_upload(
  texid: number,
  w: number,
  h: number,
  stride: number,
  fmt: TextureFormat,
  src: ArrayBuffer | Uint8Array | null
);

/* Get size and format of a texture.
 */
function texture_get_header(texid: number): {
  w: number;
  h: number;
  fmt: TextureFormat;
};

/* Clear all pixels to zero.
 */
function texture_clear(texid: number): void;

/* Global state impacting egg_draw_decal and egg_draw_tile.
 * This resets to (0,0,0xff) at the start of each render cycle.
 * Use ALPHA, the default, for normal copying of images with transparency.
 * Use OPAQUE if you want to force the input side to not use an alpha channel.
 * REPLACE and REPLACE_R use the image's alpha but replace the color of every pixel.
 * Useful for text, and odd cases like highlighting an injured sprite.
 * The alpha channel of (replacement) is ignored.
 * Global alpha is multiplied against input after color replacement. 0..0xff.
 */
function draw_mode(xfermode: XferMode, tint: number, alpha: number): void;

/* Draw a flat rectangle.
 * The global alpha does apply. xfermode and replacement do not.
 */
function draw_rect(texid: number, x: number, y: number, w: number, h: number, pixel: number): void;

/* Copy a portion from (srctexid) to (dsttexid).
 * In (src), the bounds are always (srcx,srcy,w,h) regardless of (xform).
 * In (dst), output always begins at (dstx,dsty), and size is (w,h) or (h,w) depending on EGG_XFORM_SWAP.
 * XREV and YREV always refer to the source X and Y axes.
 * So, XREV changes your nose direction and YREV your hat direction, regardless of SWAP.
 */
function draw_decal(
  dsttexid: number,
  srctexid: number,
  dstx: number,
  dsty: number,
  srcx: number,
  srcy: number,
  w: number,
  h: number,
  xform: Xform
): void;

/* Copy multiple tiles cut from a 16x16-tile sheet.
 * (x,y) are the center of the tile's output.
 * (tileid) reads LRTB: 0x00 is top-left tile, 0x0f top-right, 0xf0 bottom-left.
 * This is preferred over egg_draw_decal where possible; it's much more efficient.
 * (v) is 6 bytes per tile: [x lsb, x msb, y lsb, y msb, tileid, xform]
 * (c) is the count of tiles.
 */
function draw_tile(
  dsttexid: number,
  srctexid: number,
  v: ArrayBuffer,
  c: number
): void;

/* Audio.
 * The host provides an opinionated synthesizer.
 * Games interact with it only at a very high level.
 *************************************************************/

/* Begin playing a song resource, and end the current one.
 * If it's already playing, do nothing. Or if (force) restart it.
 * (repeat) is usually nonzero. Zero to stop music at the end of the song.
 * (songid) zero is legal, to play no music, and that's the default.
 */
function audio_play_song(qual: number, songid: number, force: boolean, repeat: boolean): void;

/* Play a one-off sound effect.
 * (trim) in 0..1, (pan) in -1..1.
 */
function audio_play_sound(qual: number, soundid: number, trim: number, pan: number): void;

/* Estimate the current song position in fractional beats from song start.
 * <0 if no song is playing.
 */
function audio_get_playhead(): number;

/* Storage.
 ***************************************************************/
 
const enum ResType { // Canonical list: src/opt/romr/romr.h
  metadata = 1,
  wasm = 2,
  js = 3,
  image = 4,
  string = 5,
  song = 6,
  sound = 7,
  map = 8,
}

/* Copy a resource from the ROM file.
 * May return >dsta if your buffer isn't long enough, and does not populate (dst) in that case.
 * Managing qualifiers is up to you. We'll provide client-side helpers for that.
 */
 //XXX This actually returns Uint8Array, and that is better -- otherwise we would have to copy each resource internally.
function res_get(tid: number, qual: number, rid: number): ArrayBuffer | null;

/* Examine the resource TOC.
 * You shouldn't need this -- they're your resources.
 */
function res_id_by_index(index: number): { tid: number, qual: number, rid: number };

/* We provide a persistent key=value store, at the user's discretion.
 * Keys should be C identifiers plus dot, and values should be UTF-8.
 * User may impose arbitrary limits, outside the game's control.
 * If the platform knows a field won't persist, we won't fake it, it will fail at set.
 * Set an empty value to delete a field.
 */
function store_set(k: string, v: string): -1|0;
function store_get(k: string): string;
function store_key_by_index(index: number): string;

/* Network.
 ******************************************************************/

/* Initiate an HTTP request.
 * You are not allowed to set request headers, by design.
 * If the request queues successfully, we return >0 (reqid).
 * You'll receive EGG_EVENT_HTTP_RSP when the request completes.
 * Failure at this point is rare, and probably means that HTTP access is disabled by the user.
 */
function http_request(method: string, url: string, body?: string | ArrayBuffer): number;

/* <0 if invalid or failed at transport level.
 * 0 if pending.
 * Otherwise the HTTP status code, eg 200.
 * Normally you don't need this; EGG_EVENT_HTTP_RSP includes the status.
 */
function http_get_status(reqid: number): number;

function http_get_header(reqid: number, k: string): string;
function http_get_body(reqid: number): ArrayBuffer | null;

/* Establish a WebSocket connection.
 * Returns >0 (wsid) on success; this means we've accepted the request, not necessarily actually connected.
 * You will receive EGG_EVENT_WS_CONNECT/DISCONNECT/MESSAGE later with that wsid.
 */
function ws_connect(url: string): number;

/* Drop a WebSocket connection.
 * You should not receive EGG_EVENT_WS_DISCONNECT in this case.
 */
function ws_disconnect(wsid: number): void;

/* Retrieve a message payload, after being notified by EGG_EVENT_WS_MESSAGE.
 */
function ws_get_message(wsid: number, msgid: number): string;

/* Queue a message for delivery.
 * No I/O during this call, and you won't be notified of the status.
 * (opcode) is ignored. TODO: Adjust this API, have it follow the same rules as WebSocket.send().
 */
function ws_send(wsid: number, opcode: number, v: string): void;

/* System utilities.
 *******************************************************************/

/* Dump text to the console if supported.
 * Do not use this to communicate with the user; real users will probably have it disabled.
 * Do not include a newline in (fmt).
 * (fmt) is phrased like standard printf, though we don't implement all the bells and whistles. (TODO document this exactly)
 */
function log(fmt: string, ...args: any[]): void;

/* Current real time in seconds since the host's epoch (which is not knowable to you).
 * Note that the intervals reported to egg_client_update() are controlled, and will not always match real time.
 */
function time_real(): number;

/* Current real local time, split out.
 */
function time_get(): { year: number, month: number, day: number, hour: number, minute: number, second: number, milli: number };

/* Return languages the user prefers, in order of her preference.
 * Big-endian ISO 639 codes, eg English "en" = 0x656e.
 * Be prepared for an empty response, we don't necessarily know this on every platform.
 */
function get_user_languages(): number[];

function request_termination(): void;

function is_terminable(): boolean;

}
