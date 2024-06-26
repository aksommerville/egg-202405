/* egg.h
 */
 
#ifndef EGG_H
#define EGG_H

#include <stdint.h>

/* Client hooks.
 * For full-native builds, you are required to implement all four, even if they're noop.
 * WebAssembly builds, it's OK to leave unneeded hooks unimplemented.
 ************************************************************************/

/* Last hook you'll get.
 * It is still safe to write to the store during this call.
 * HTTP calls should not be expected to go out.
 * Normally there's nothing for a game to do here.
 */
void egg_client_quit();

/* First hook you'll get.
 * Return <0 to abort or >=0 to proceed.
 */
int egg_client_init();

/* Called repeatedly, hosts should aim for 60 Hz.
 * (elapsed) is the time in seconds since the last update.
 * This number is fudged to keep it in a reasonable range.
 * (but take it at face value; let the platform decide when to run fast or slow).
 * Normally a game will drain the event queue, then update its model state during this call.
 */
void egg_client_update(double elapsed);

/* Called repeatedly, normally one render per update, though the platform has some discretion there.
 * During this call, your GX context is guaranteed available.
 * That's not necessarily true during other callbacks.
 * If you are not using continuous time, it's fair to do your full update cycle during render.
 */
void egg_client_render();

/* Input.
 ***********************************************************************/
 
#define EGG_EVENT_INPUT          1 /* [devid,btnid,value,_] */
#define EGG_EVENT_CONNECT        2 /* [devid,mapping,_,_] */
#define EGG_EVENT_DISCONNECT     3 /* [devid,_,_,_] */
#define EGG_EVENT_HTTP_RSP       4 /* [reqid,status,length,_] */
#define EGG_EVENT_WS_CONNECT     5 /* [wsid,_,_,_] */
#define EGG_EVENT_WS_DISCONNECT  6 /* [wsid,_,_,_] */
#define EGG_EVENT_WS_MESSAGE     7 /* [wsid,msgid,length,_] */
#define EGG_EVENT_MMOTION        8 /* [x,y,_,_] */
#define EGG_EVENT_MBUTTON        9 /* [btnid,value,x,y] */
#define EGG_EVENT_MWHEEL        10 /* [dx,dy,x,y] */
#define EGG_EVENT_KEY           11 /* [hidusage,value,_,_] */
#define EGG_EVENT_TEXT          12 /* [codepoint,_,_,_] */
#define EGG_EVENT_TOUCH         13 /* [id,state(0,1,2),x,y] (0,1,2)=(release,press,move) */
#define EGG_EVENT_ACCELEROMETER 14 /* [x,y,z,_] ms/s**2 s16.16 */

/* Native platforms will usually only be able to use RAW here.
 * But if we become able to canonicalize gamepad layouts, eventually we may do so.
 * When running in a browser, it's normal and joyous to receive STANDARD mapping.
 */
#define EGG_INPUT_MAPPING_RAW       0
#define EGG_INPUT_MAPPING_STANDARD  1 /* https://w3c.github.io/gamepad/#remapping */
 
struct egg_event {
  int type;
  int v[4];
};

/* If any events are queued, populate (eventv) and return the count, up to (eventa).
 * If this returns (eventa) exactly, process them and call again, there could be more.
 * Does not block.
 */
int egg_event_next(struct egg_event *eventv,int eventa);

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
int egg_event_enable(int evttype,int evtstate);
#define EGG_EVTSTATE_QUERY      0
#define EGG_EVTSTATE_IMPOSSIBLE 1
#define EGG_EVTSTATE_DISABLED   2
#define EGG_EVTSTATE_ENABLED    3
#define EGG_EVTSTATE_REQUIRED   4

/* You'll want to do these after EGG_EVENT_CONNECT to examine each new device.
 */
int egg_input_device_get_name(char *dst,int dsta,int devid);
void egg_input_device_get_ids(int *vid,int *pid,int *version,int devid);
void egg_input_device_get_button(
  int *btnid,int *hidusage,int *lo,int *hi,int *value,
  int devid,int index
);

void egg_input_device_disconnect(int devid);

/* Zero to hide cursor even when enabled.
 * By default, we show the system cursor if there is one and any of the mouse events is enabled.
 */
void egg_show_cursor(int show);

/* Video.
 ****************************************************************/

#define EGG_TEX_FMT_UNSPEC  0
#define EGG_TEX_FMT_RGBA    1
#define EGG_TEX_FMT_A8      2
#define EGG_TEX_FMT_A1      3
#define EGG_TEX_FMT_Y8      4
#define EGG_TEX_FMT_Y1      5

#define EGG_XFERMODE_ALPHA     0 /* Use alpha from source. */
#define EGG_XFERMODE_OPAQUE    1 /* Ignore alpha, cover the entire output rectangle. */

#define EGG_XFORM_XREV 1
#define EGG_XFORM_YREV 2
#define EGG_XFORM_SWAP 4

/* Access to the platform's image decoder.
 * You don't need this for normal image rendering.
 * If you are doing software rendering client-side or some other weird thing,
 * you can use these to fetch image resources, decoding the same way egg_texture_load_image() would.
 * You must first call egg_image_get_header(), then allocate enough space for it, then egg_image_decode().
 * You provide the row-to-row stride at decode, largely as a validation mechanism.
 */
void egg_image_get_header(int *w,int *h,int *fmt,int qual,int imageid);
int egg_image_decode(void *dst,int dsta,int stride,int qual,int imageid);

/* Create or delete a texture.
 * Platform will have some internal limit, but it's not publically defined and may vary across hosts.
 * (texid) is a positive integer, zero for invalid, and never negative.
 * Texid One is the main output. It exists before init, and can't be replaced.
 */
void egg_texture_del(int texid);
int egg_texture_new();

/* Populate texture from an image resource or raw client-side pixels.
 * You may upload with empty data to create the texture with all pixels zeroed.
 * Otherwise, (srcc==stride*h).
 */
int egg_texture_load_image(int texid,int qual,int imageid);
int egg_texture_upload(int texid,int w,int h,int stride,int fmt,const void *src,int srcc);

/* Get size and format of a texture.
 */
void egg_texture_get_header(int *w,int *h,int *fmt,int texid);

/* Clear all pixels to zero.
 */
void egg_texture_clear(int texid);

/* Global state impacting egg_draw_decal and egg_draw_tile.
 * This resets to (0,0,0xff) at the start of each render cycle.
 * Use ALPHA, the default, for normal copying of images with transparency.
 * Use OPAQUE if you want to force the input side to not use an alpha channel.
 * (tint) is rgba; every pixel tints toward it according to tint's alpha.
 * (alpha) is multiplied against input after color replacement.
 */
void egg_draw_mode(int xfermode,uint32_t tint,uint8_t alpha);

/* Draw a flat rectangle.
 * The global alpha does apply. xfermode and replacement do not.
 */
void egg_draw_rect(int texid,int x,int y,int w,int h,uint32_t pixel);

/* Copy a portion from (srctexid) to (dsttexid).
 * In (src), the bounds are always (srcx,srcy,w,h) regardless of (xform).
 * In (dst), output always begins at (dstx,dsty), and size is (w,h) or (h,w) depending on EGG_XFORM_SWAP.
 * XREV and YREV always refer to the source X and Y axes.
 * So, XREV changes your nose direction and YREV your hat direction, regardless of SWAP.
 * (srcx,srcy,w,h) must be in bounds of (srctexid), behavior is deliberately undefined if not.
 */
void egg_draw_decal(
  int dsttexid,int srctexid,
  int dstx,int dsty,
  int srcx,int srcy,
  int w,int h,
  int xform
);

/* Copy multiple tiles cut from a 16x16-tile sheet.
 * (x,y) are the center of the tile's output.
 * (tileid) reads LRTB: 0x00 is top-left tile, 0x0f top-right, 0xf0 bottom-left.
 * This is preferred over egg_draw_decal where possible; it's much more efficient.
 */
struct egg_draw_tile {
  int16_t x,y;
  uint8_t tileid;
  uint8_t xform;
};
void egg_draw_tile(
  int dsttexid,int srctexid,
  const struct egg_draw_tile *v,int c
);

/* Audio.
 * The host provides an opinionated synthesizer.
 * Games interact with it only at a very high level.
 *************************************************************/

/* Begin playing a song resource, and end the current one.
 * If it's already playing, do nothing. Or if (force) restart it.
 * (repeat) is usually nonzero. Zero to stop music at the end of the song.
 * (songid) zero is legal, to play no music, and that's the default.
 */
void egg_audio_play_song(int qual,int songid,int force,int repeat);

/* Play a one-off sound effect.
 * (trim) in 0..1, (pan) in -1..1.
 */
void egg_audio_play_sound(int qual,int soundid,double trim,double pan);

/* Estimate the current song position in fractional beats from song start.
 * <0 if no song is playing.
 */
double egg_audio_get_playhead();

/* Storage.
 ***************************************************************/
 
// Canonical list: src/opt/romr/romr.h
#define EGG_TID_metadata 1
#define EGG_TID_wasm 2
#define EGG_TID_js 3
#define EGG_TID_image 4
#define EGG_TID_string 5
#define EGG_TID_song 6
#define EGG_TID_sound 7
#define EGG_TID_map 8

/* Copy a resource from the ROM file.
 * May return >dsta if your buffer isn't long enough, and does not populate (dst) in that case.
 * Managing qualifiers is up to you. We'll provide client-side helpers for that.
 */
int egg_res_get(void *dst,int dsta,int tid,int qual,int rid);

/* Examine the resource TOC.
 * You shouldn't need this -- they're your resources.
 */
void egg_res_id_by_index(int *tid,int *qual,int *rid,int index);

/* We provide a persistent key=value store, at the user's discretion.
 * Keys should be C identifiers plus dot, and values should be UTF-8.
 * User may impose arbitrary limits, outside the game's control.
 * If the platform knows a field won't persist, we won't fake it, it will fail at set.
 * Set an empty value to delete a field.
 * **Lengths must be provided everywhere, you can't give <0 to measure to NUL**
 */
int egg_store_set(const char *k,int kc,const char *v,int vc);
int egg_store_get(char *dst,int dsta,const char *k,int kc);
int egg_store_key_by_index(char *dst,int dsta,int index);

/* Network.
 ******************************************************************/

/* Initiate an HTTP request.
 * You are not allowed to set request headers, by design.
 * If the request queues successfully, we return >0 (reqid).
 * You'll receive EGG_EVENT_HTTP_RSP when the request completes.
 * Failure at this point is rare, and probably means that HTTP access is disabled by the user.
 */
int egg_http_request(
  const char *method,
  const char *url,
  const void *body,int bodyc
);

/* <0 if invalid or failed at transport level.
 * 0 if pending.
 * Otherwise the HTTP status code, eg 200.
 * Normally you don't need this; EGG_EVENT_HTTP_RSP includes the status.
 */
int egg_http_get_status(int reqid);

int egg_http_get_header(char *dst,int dsta,int reqid,const char *k,int kc);//XXX complicated. Can we live without this?
int egg_http_get_body(void *dst,int dsta,int reqid);

/* Establish a WebSocket connection.
 * Returns >0 (wsid) on success; this means we've accepted the request, not necessarily actually connected.
 * You will receive EGG_EVENT_WS_CONNECT/DISCONNECT/MESSAGE later with that wsid.
 */
int egg_ws_connect(const char *url);

/* Drop a WebSocket connection.
 * You should not receive EGG_EVENT_WS_DISCONNECT in this case.
 */
void egg_ws_disconnect(int wsid);

/* Retrieve a message payload, after being notified by EGG_EVENT_WS_MESSAGE.
 */
int egg_ws_get_message(void *dst,int dsta,int wsid,int msgid);

/* Queue a message for delivery.
 * No I/O during this call, and you won't be notified of the status.
 */
void egg_ws_send(int wsid,int opcode,const void *v,int c);

/* System utilities.
 *******************************************************************/

/* Dump text to the console if supported.
 * Do not use this to communicate with the user; real users will probably have it disabled.
 * Do not include a newline in (fmt).
 * (fmt) is phrased like standard printf, though we don't implement all the bells and whistles. (TODO document this exactly)
 */
void egg_log(const char *fmt,...);

/* Current real time in seconds since the host's epoch (which is not knowable to you).
 * Note that the intervals reported to egg_client_update() are controlled, and will not always match real time.
 */
double egg_time_real();

/* Current real local time, split out.
 * OK to pass null for fields you don't need.
 */
void egg_time_get(int *year,int *month,int *day,int *hour,int *minute,int *second,int *milli);

/* Fill (dst) with languages the user prefers, in order of her preference.
 * Big-endian ISO 639 codes, eg English "en" = 0x656e.
 * Be prepared for an empty response, we don't necessarily know this on every platform.
 */
int egg_get_user_languages(int *dst,int dsta);

/* Requesting termination does not guarantee that execution will end immediately, or at all.
 * egg_is_terminable() will return the same every time:
 *   0 for web browsers and maybe consoles, where there's no concept of termination.
 *   1 if egg_request_termination() is expected to work. Though again, not necessarily immediately, be careful.
 */
void egg_request_termination();
int egg_is_terminable();

#endif
