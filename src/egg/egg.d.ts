/* egg.d.ts
 * The Egg Public API, Javascript version.
 */
 
/* Client hooks.
 * Game should implement these in js:0:1 by calling exportModule({...}).
 ************************************************************************/
 
export interface ClientModule {
  egg_client_quit: () => void,
  egg_client_init: () => -1|0,
  egg_client_update: (elapsed: number) => void,
  egg_client_render: () => void,
}

export declare function exportModule(mod: ClientModule): void;

/* Input.
 ***********************************************************************/

// EGG_EVENT_ in the C API.
export enum EventType {
  INPUT         =  1, /* [devid,btnid,value,_] */
  CONNECT       =  2, /* [devid,mapping,_,_] */
  DISCONNECT    =  3, /* [devid,_,_,_] */
  HTTP_RSP      =  4, /* [reqid,status,length,_] */
  WS_CONNECT    =  5, /* [wsid,_,_,_] */
  WS_DISCONNECT =  6, /* [wsid,_,_,_] */
  WS_MESSAGE    =  7, /* [wsid,msgid,length,_] */
  MMOTION       =  8, /* [x,y,_,_] */
  MBUTTON       =  9, /* [btnid,value,x,y] */
  MWHEEL        = 10, /* [dx,dy,x,y] */
  KEY           = 11, /* [hidusage,value,_,_] */
  TEXT          = 12, /* [codepoint,_,_,_] */
  RESIZE        = 13, /* [w,h,_,_] */
  TOUCH         = 14, /* [id,state(0,1,2),x,y] */
};

export enum GamepadMapping {
  RAW = 0,
  STANDARD = 1, /* https://w3c.github.io/gamepad/#remapping */
};

//TODO Should I make a union type, one with each eventType?
export interface Event {
  eventType: EventType,
  v0: number,
  v1: number,
  v2: number,
  v3: number,
};

export declare function event_next(): Event[];

/* Ask for one bit of the event mask, or change one.
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
export declare function event_enable(eventType: EventType, eventState: EventState): EventState;

// EGG_EVTSTATE_* in the C API.
export enum EventState {
  QUERY = 0,
  IMPOSSIBLE = 1,
  DISABLED = 2,
  ENABLED = 3,
  REQUIRED = 4,
};

/* You'll want to do these after EGG_EVENT_CONNECT to examine each new device.
 */
export declare function input_device_get_name(devid: number): string;
export declare function input_device_get_ids(devid: number): { vid: number, pid: number, version: number };
export declare function input_device_get_button(devid: number, index: number): { btnid: number, hidusage: number, lo: number, hi: number, value: number } | null;

export declare function input_device_disconnect(devid: number): void;

/* Video.
 ****************************************************************/

/* Game can not control the main video size, and it can change at any time.
 * You'll get a RESIZE event when it does. Or you can poll this as needed.
 */
export declare function video_get_size(): [number, number];

/* Audio.
 * The host provides an opinionated synthesizer.
 * Games interact with it only at a very high level.
 *************************************************************/

/* Begin playing a song resource, and end the current one.
 * If it's already playing, do nothing. Or if (force) restart it.
 * (repeat) is usually nonzero. Zero to stop music at the end of the song.
 * (songid) zero is legal, to play no music, and that's the default.
 */
export declare function audio_play_song(songid: number, force: boolean, repeat: boolean): void;

/* Play a one-off sound effect.
 * (trim) in 0..1, (pan) in -1..1.
 */
export declare function audio_play_sound(soundid: number, trim: number, pan: number): void;

/* Estimate the current song position in milliseconds from song start.
 * Our songs do not provide any tempo information, so digesting this is up to you.
 * <0 if no song is playing.
 */
export declare function audio_get_playhead(): number;

/* Storage.
 ***************************************************************/

/* Copy a resource from the ROM file.
 * May return >dsta if your buffer isn't long enough, and does not populate (dst) in that case.
 * Managing qualifiers is up to you. We'll provide client-side helpers for that.
 */
export declare function res_get(tid: number, qual: number, rid: number): ArrayBuffer | null;

/* Examine the resource TOC.
 * You shouldn't need this -- they're your resources.
 */
export declare function res_id_by_index(index: number): { tid: number, qual: number, rid: number };

/* We provide a persistent key=value store, at the user's discretion.
 * Keys should be C identifiers plus dot, and values should be UTF-8.
 * User may impose arbitrary limits, outside the game's control.
 * If the platform knows a field won't persist, we won't fake it, it will fail at set.
 * Set an empty value to delete a field.
 */
export declare function store_set(k: string, v: string): -1|0;
export declare function store_get(k: string): string;
export declare function store_key_by_index(index: number): string;

/* Network.
 ******************************************************************/

/* Initiate an HTTP request.
 * You are not allowed to set request headers, by design.
 * If the request queues successfully, we return >0 (reqid).
 * You'll receive EGG_EVENT_HTTP_RSP when the request completes.
 * Failure at this point is rare, and probably means that HTTP access is disabled by the user.
 */
export declare function http_request(method: string, url: string, body?: string | ArrayBuffer): number;

/* <0 if invalid or failed at transport level.
 * 0 if pending.
 * Otherwise the HTTP status code, eg 200.
 * Normally you don't need this; EGG_EVENT_HTTP_RSP includes the status.
 */
export declare function http_get_status(reqid: number): number;

export declare function http_get_header(reqid: number, k: string): string;
export declare function http_get_body(reqid: number): ArrayBuffer | null;

/* Establish a WebSocket connection.
 * Returns >0 (wsid) on success; this means we've accepted the request, not necessarily actually connected.
 * You will receive EGG_EVENT_WS_CONNECT/DISCONNECT/MESSAGE later with that wsid.
 */
export declare function ws_connect(url: string): number;

/* Drop a WebSocket connection.
 * You should not receive EGG_EVENT_WS_DISCONNECT in this case.
 */
export declare function ws_disconnect(wsid: number): void;

/* Retrieve a message payload, after being notified by EGG_EVENT_WS_MESSAGE.
 */
export declare function ws_get_message(wsid: number, msgid: number): string;

/* Queue a message for delivery.
 * No I/O during this call, and you won't be notified of the status.
 */
export declare function ws_send(wsid: number, opcode: number, v: string): void;

/* System utilities.
 *******************************************************************/

/* Dump text to the console if supported.
 * Do not use this to communicate with the user; real users will probably have it disabled.
 * Do not include a newline in (fmt).
 * (fmt) is phrased like standard printf, though we don't implement all the bells and whistles. (TODO document this exactly)
 */
export declare function log(fmt: string, ...args: any[]): void;

/* Current real time in seconds since the host's epoch (which is not knowable to you).
 * Note that the intervals reported to egg_client_update() are controlled, and will not always match real time.
 */
export declare function time_real(): number;

/* Current real local time, split out.
 * OK to pass null for fields you don't need.
 */
export declare function time_get(): { year: number, month: number, day: number, hour: number, minute: number, second: number, milli: number };

/* Fill (dst) with languages the user prefers, in order of her preference.
 * Big-endian ISO 631 codes, eg English "en" = 0x656e.
 * Be prepared for an empty response, we don't necessarily know this on every platform.
 */
export declare function get_user_languages(): number[];

export declare function request_termination(): void;

export declare function is_terminable(): boolean;
