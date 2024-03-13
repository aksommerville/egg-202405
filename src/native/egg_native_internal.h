#ifndef EGG_NATIVE_INTERNAL_H
#define EGG_NATIVE_INTERNAL_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include "opt/hostio/hostio.h"
#include "opt/timer/timer.h"
#include "opt/wamr/wamr.h"
#include "opt/qjs/qjs.h"
#include "opt/romr/romr.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"

struct egg_function_location {
  uint8_t tid; // 0,EGG_TID_wasm,EGG_TID_js
  int modid,fnid;
};

extern struct egg {

  // Owned by configure.
  const char *exename;
  char *video_driver;
  char *video_device;
  int video_w,video_h;
  int fullscreen;
  char *audio_driver;
  char *audio_device;
  int audio_rate;
  int audio_chanc;
  int audio_buffer;
  char *input_driver;
  char *input_device;
  char *rompath;
  
  // From ROM file.
  char *romtitle;
  void *romicon; // rgba
  int romiconw,romiconh;
  int romfbw,romfbh;
  void *romserial;
  int romserialc;
  
  struct egg_function_location loc_client_init;
  struct egg_function_location loc_client_quit;
  struct egg_function_location loc_client_update;
  struct egg_function_location loc_client_render;
  
  volatile int sigc;
  int terminate;
  
  struct timer timer;
  struct hostio *hostio;
  struct wamr *wamr;
  struct qjs *qjs;
  struct romr romr;
  
} egg;

int egg_native_configure(int argc,char **argv);
void egg_configure_cleanup();

void egg_native_rom_cleanup();
int egg_native_rom_init();
int egg_native_uses_rom_file(); // constant but private

int egg_native_install_runtime_exports();
int egg_native_init_client_code();
int egg_native_call_client_init();
void egg_native_call_client_quit();
int egg_native_call_client_update(double elapsed);
int egg_native_call_client_render();

void egg_native_cb_close(struct hostio_video *driver);
void egg_native_cb_focus(struct hostio_video *driver,int focus);
void egg_native_cb_resize(struct hostio_video *driver,int w,int h);
int egg_native_cb_key(struct hostio_video *driver,int keycode,int value);
void egg_native_cb_text(struct hostio_video *driver,int codepoint);
void egg_native_cb_mmotion(struct hostio_video *driver,int x,int y);
void egg_native_cb_mbutton(struct hostio_video *driver,int btnid,int value);
void egg_native_cb_mwheel(struct hostio_video *driver,int dx,int dy);
void egg_native_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver);
void egg_native_cb_connect(struct hostio_input *driver,int devid);
void egg_native_cb_disconnect(struct hostio_input *driver,int devid);
void egg_native_cb_button(struct hostio_input *driver,int devid,int btnid,int value);

#endif
