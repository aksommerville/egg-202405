/* egg_public.js
 */
 
import { Runtime } from "./Runtime.js";
 
export function log(fmt, ...vargs) {
  console.log(`egg_log: ${fmt} ${JSON.stringify(vargs)}`);
}

export function event_next() {
  return [];
}

export function event_enable(eventType, eventState) {
  return 1; // IMPOSSIBLE
}

export function input_device_get_name(devid) {
  return "";
}

export function input_device_get_ids(devid) {
  return { vid: 0, pid: 0, version: 0 };
}

export function input_device_get_button(devid, index) {
  return null; // {btnid,hidusage,lo,hi,value}
}

export function input_device_disconnect(devid) {
}

export function video_get_size() {
  const canvas = Runtime.singleton.canvas;
  return [canvas.width, canvas.height];
}

export function video_get_context() {
  return Runtime.singleton.gl;
}

export function audio_play_song(songid, force, repeat) {
}

export function audio_play_sound(soundid, trim, pan) {
}

export function audio_get_playhead() {
  return 0;
}

export function res_get(tid, qual, rid) {
  return null;
}

export function res_id_by_index(index) {
  return null;
}

export function store_set(k, v) {
  return -1;
}

export function store_get(k) {
  return "";
}

export function store_key_by_index(index) {
  return "";
}

export function http_request(method, url, body) {
  return -1;
}

export function http_get_status(reqid) {
  return -1;
}

export function http_get_header(reqid, k) {
  return "";
}

export function http_get_body(reqid) {
  return null;
}

export function ws_connect(url) {
  return -1;
}

export function ws_disconnect(wsid) {
}

export function ws_get_message(wsid, msgid) {
  return "";
}

export function ws_send(wsid, opcode, v) {
}

export function time_real() {
  return 0;
}

export function time_get() {
  return { year: 0, month: 0, day: 0, hour: 0, minute: 0, second: 0, milli: 0 };
}

export function get_user_languages() {
  return [];
}

export function request_termination() {
}

export function is_terminable() {
  return false;
}
