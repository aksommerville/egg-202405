#include "egg_native_internal.h"

/* Window.
 */
 
void egg_native_cb_close(struct hostio_video *driver) {
  egg.terminate=1;
}

void egg_native_cb_focus(struct hostio_video *driver,int focus) {
}

void egg_native_cb_resize(struct hostio_video *driver,int w,int h) {
  //TODO
}

/* Keyboard.
 */
 
int egg_native_cb_key(struct hostio_video *driver,int keycode,int value) {
  return 0;
}

void egg_native_cb_text(struct hostio_video *driver,int codepoint) {
  switch (codepoint) {
    case 0x1b: egg.terminate=1; break;
  }
}

/* Mouse.
 */
 
void egg_native_cb_mmotion(struct hostio_video *driver,int x,int y) {
}

void egg_native_cb_mbutton(struct hostio_video *driver,int btnid,int value) {
}

void egg_native_cb_mwheel(struct hostio_video *driver,int dx,int dy) {
}

/* Audio.
 */
 
void egg_native_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver) {
  memset(v,0,c<<1);//TODO
}

/* Joystick.
 */
 
void egg_native_cb_connect(struct hostio_input *driver,int devid) {
}

void egg_native_cb_disconnect(struct hostio_input *driver,int devid) {
}

void egg_native_cb_button(struct hostio_input *driver,int devid,int btnid,int value) {
}
