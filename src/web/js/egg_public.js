/* egg_public.js
 * XXX Modules aren't going to work for this.
 * It's shoehorned in that way just now, so this exists. Route everything to 'window.egg'.
 */
 
import { Runtime } from "./Runtime.js";
 
export function log(fmt, ...vargs) { egg.log(fmt, ...vargs); }
export function event_next() { return egg.event_next(); }
export function event_enable(eventType, eventState) { return egg.event_enable(eventType, eventState); }
export function input_device_get_name(devid) { return egg.input_device_get_name(devid); }
export function input_device_get_ids(devid) { return egg.input_device_get_ids(devid); }
export function input_device_get_button(devid, index) { return egg.input_device_get_button(devid, index); }
export function input_device_disconnect(devid) { egg.input_device_disconnect(devid); }
export function video_get_size() { return egg.video_get_size(); }
export function audio_play_song(songid, force, repeat) { egg.audio_play_song(songid, force, repeat); }
export function audio_play_sound(soundid, trim, pan) { egg.audio_play_sound(soundid, trim, pan); }
export function audio_get_playhead() { return egg.audio_get_playhead(); }
export function res_get(tid, qual, rid) { return egg.res_get(tid, qual, rid); }
export function res_id_by_index(index) { return egg.res_id_by_index(index); }
export function store_set(k, v) { return egg.store_set(k, v); }
export function store_get(k) { return egg.store_get(k); }
export function store_key_by_index(index) { return egg.store_key_by_index(index); }
export function http_request(method, url, body) { return egg.http_request(method, url, body); }
export function http_get_status(reqid) { return egg.http_get_status(reqid); }
export function http_get_header(reqid, k) { return egg.http_get_header(reqid, k); }
export function http_get_body(reqid) { return egg.http_get_body(reqid); }
export function ws_connect(url) { return egg.ws_connect(url); }
export function ws_disconnect(wsid) { egg.ws_disconnect(wsid); }
export function ws_get_message(wsid, msgid) { return egg.ws_get_message(wsid, msgid); }
export function ws_send(wsid, opcode, v) { egg.ws_send(wsid, opcode, v); }
export function time_real() { return egg.time_real(); }
export function time_get() { return egg.time_get(); }
export function get_user_languages() { return egg.get_user_languages(); }
export function request_termination() { egg.request_termination(); }
export function is_terminable() { return egg.is_terminable(); }
