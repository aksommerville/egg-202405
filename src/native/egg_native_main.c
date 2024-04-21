#include "egg_native_internal.h"
#include <signal.h>

struct egg egg={0};

/* Signal.
 */
 
static void egg_native_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(egg.sigc)>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Quit.
 */
 
static void egg_native_quit(int status) {
  egg_native_call_client_quit();
  if (egg.localstore.dirty&&egg.storepath) {
    localstore_save(&egg.localstore,egg.storepath);
  }
  if (!status) {
    timer_report(&egg.timer);
    fprintf(stderr,"%s: Normal exit.\n",egg.exename);
  } else {
    fprintf(stderr,"%s: Abnormal exit.\n",egg.exename);
  }
  render_del(egg.render);
  softrender_del(egg.softrender);
  hostio_del(egg.hostio);
  synth_del(egg.synth);
  wamr_del(egg.wamr);
  qjs_del(egg.qjs);
  egg_native_net_cleanup();
  egg_native_rom_cleanup();
  egg_native_input_cleanup();
  egg_configure_cleanup();
  localstore_cleanup(&egg.localstore);
}

/* Init.
 */
 
static int egg_native_init() {
  int err;
  
  if (egg.storepath) {
    if (localstore_load(&egg.localstore,egg.storepath)<0) {
      fprintf(stderr,"%s: Failed to load game's data store.\n",egg.storepath);
    } else {
      fprintf(stderr,"%s: Loaded game's data store.\n",egg.storepath);
    }
  }
  
  if ((err=egg_native_net_init())<0) {
    fprintf(stderr,"%s: Starting up without networking due to error.\n",egg.exename);
  }
  
  if ((err=egg_native_event_init())<0) return err;

  if ((err=egg_native_rom_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error loading ROM.\n",egg.exename);
    return -2;
  }
  
  /* Create both runtimes.
   */
  if (!(egg.wamr=wamr_new())) {
    fprintf(stderr,"%s: Failed to initialize WebAssembly runtime.\n",egg.exename);
    return -2;
  }
  if (!(egg.qjs=qjs_new())) {
    fprintf(stderr,"%s: Failed to initialize JavaScript runtime.\n",egg.exename);
    return -2;
  }
  if ((err=egg_native_install_runtime_exports())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error configuring Wasm or JS runtime.\n",egg.exename);
    return -2;
  }
  if ((err=egg_native_init_client_code())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing client code.\n",egg.exename);
    return -2;
  }
 
  /* Bring hostio up.
   */
  struct hostio_video_delegate video_delegate={
    .cb_close=egg_native_cb_close,
    .cb_focus=egg_native_cb_focus,
    .cb_resize=egg_native_cb_resize,
    .cb_key=egg_native_cb_key,
    .cb_text=egg_native_cb_text,
    .cb_mmotion=egg_native_cb_mmotion,
    .cb_mbutton=egg_native_cb_mbutton,
    .cb_mwheel=egg_native_cb_mwheel,
  };
  struct hostio_audio_delegate audio_delegate={
    .cb_pcm_out=egg_native_cb_pcm_out,
  };
  struct hostio_input_delegate input_delegate={
    .cb_connect=egg_native_cb_connect,
    .cb_disconnect=egg_native_cb_disconnect,
    .cb_button=egg_native_cb_button,
  };
  if (!(egg.hostio=hostio_new(&video_delegate,&audio_delegate,&input_delegate))) return -1;
  
  /* Initialize video driver.
   */
  struct hostio_video_setup video_setup={
    .w=egg.video_w,
    .h=egg.video_h,
    .fullscreen=egg.fullscreen,
    .fbw=egg.romfbw,
    .fbh=egg.romfbh,
    .device=egg.video_device,
  };
  if (egg.romtitle&&egg.romtitle[0]) video_setup.title=egg.romtitle;
  else if (egg.rompath&&egg.rompath[0]) video_setup.title=egg.rompath;
  else video_setup.title=egg.exename;
  if (egg.romicon&&(egg.romiconw>0)&&(egg.romiconh>0)) {
    video_setup.iconrgba=egg.romicon;
    video_setup.iconw=egg.romiconw;
    video_setup.iconh=egg.romiconh;
  } else {
    egg_appicon_init();
    video_setup.iconrgba=egg.appicon_rgba;
    video_setup.iconw=egg.appiconw;
    video_setup.iconh=egg.appiconh;
  }
  switch (egg.render_choice) {
    case 1: video_setup.access_mode=HOSTIO_VIDEO_ACCESS_GX; break;
    case 2: video_setup.access_mode=HOSTIO_VIDEO_ACCESS_FB; break;
  }
  if ((hostio_init_video(egg.hostio,egg.video_driver,&video_setup)<0)||!egg.hostio->video) {
    fprintf(stderr,"%s: Failed to initialize video.\n",egg.exename);
    return -2;
  }
  
  /* Choose and initialize renderer.
   */
  switch (egg.render_choice) {
    case 0: {
        if (egg.hostio->video->type->gx_begin) egg.render=render_new();
        else if (egg.hostio->video->type->fb_begin) egg.softrender=softrender_new();
      } break;
    case 1: egg.render=render_new(); break;
    case 2: egg.softrender=softrender_new(); break;
  }
  if (egg.render) {
    if (!egg.hostio->video->type->gx_begin||!egg.hostio->video->type->gx_end) {
      fprintf(stderr,"%s: Video driver '%s' does not support GX.\n",egg.exename,egg.hostio->video->type->name);
      return -2;
    }
    if (render_texture_new(egg.render)!=1) return -1;
    if (render_texture_load(egg.render,1,egg.romfbw,egg.romfbh,egg.romfbw<<2,EGG_TEX_FMT_RGBA,0,0)<0) return -1;
  } else if (egg.softrender) {
    if (!egg.hostio->video->type->fb_begin||!egg.hostio->video->type->fb_end||!egg.hostio->video->type->fb_describe) {
      // We could create a GX context and shovel framebuffers into that. I think there's no point; just use regular render in that case.
      fprintf(stderr,"%s: Video driver '%s' does not support direct framebuffer.\n",egg.exename,egg.hostio->video->type->name);
      return -2;
    }
    struct hostio_video_fb_description desc={0};
    if (egg.hostio->video->type->fb_describe(&desc,egg.hostio->video)<0) {
      fprintf(stderr,"%s: Failed to acquire framebuffer description from driver '%s'.\n",egg.exename,egg.hostio->video->type->name);
      return -2;
    }
    if ((err=softrender_init_texture_1(egg.softrender,&desc))<0) {
      if (err!=-2) fprintf(stderr,
        "%s: Failed to initialize soft renderer. size=%d,%d stride=%d pixelsize=%d\n",
        egg.exename,desc.w,desc.h,desc.stride,desc.pixelsize
      );
    }
  } else {
    fprintf(stderr,"%s: Failed to initialize renderer.\n",egg.exename);
    return -2;
  }
  
  /* Initialize input drivers.
   */
  struct hostio_input_setup input_setup={
    .path=egg.input_device,
  };
  if (hostio_init_input(egg.hostio,egg.input_driver,&input_setup)<0) {
    fprintf(stderr,"%s: Failed to initialize input.\n",egg.exename);
    return -2;
  }
  
  /* Initialize audio driver.
   */
  struct hostio_audio_setup audio_setup={
    .rate=egg.audio_rate,
    .chanc=egg.audio_chanc,
    .device=egg.audio_device,
    .buffer_size=egg.audio_buffer,
  };
  if (hostio_init_audio(egg.hostio,egg.audio_driver,&audio_setup)<0) {
    fprintf(stderr,"%s: Failed to initialize audio.\n",egg.exename);
    return -2;
  }
  if (!(egg.synth=synth_new(egg.hostio->audio->rate,egg.hostio->audio->chanc,&egg.romr))) {
    fprintf(stderr,"%s: Failed to initialize synthesizer.\n",egg.exename);
    return -2;
  }
  
  hostio_log_driver_names(egg.hostio);
  
  if ((err=egg_native_call_client_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Error initializing game.\n",egg.exename);
    return -2;
  }
  
  hostio_audio_play(egg.hostio,1);
  
  timer_init(&egg.timer,60,0.25);
  
  return 0;
}

/* Render.
 */
 
static int egg_native_render_gx() {
  int err;
  if (egg.hostio->video->type->gx_begin(egg.hostio->video)<0) {
    fprintf(stderr,"%s: Error entering GX context.\n",egg.exename);
    return -2;
  }
  
  render_draw_mode(egg.render,EGG_XFERMODE_ALPHA,0,0xff);
  
  if ((err=egg_native_call_client_render())<0) {
    if (err!=-2) fprintf(stderr,"%s: Error rendering game.\n",egg.exename);
    return -2;
  }
  
  render_draw_to_main(egg.render,egg.hostio->video->w,egg.hostio->video->h,1);

  if (egg.hostio->video->type->gx_end(egg.hostio->video)<0) {
    fprintf(stderr,"%s: Error exiting GX context.\n",egg.exename);
    return -2;
  }
  return 0;
}

static int egg_native_render_fb() {
  int err;
  void *fb=egg.hostio->video->type->fb_begin(egg.hostio->video);
  if (!fb) {
    fprintf(stderr,"%s: Error entering direct-render context.\n",egg.exename);
    return -2;
  }
  
  softrender_set_main(egg.softrender,fb);
  softrender_draw_mode(egg.softrender,EGG_XFERMODE_ALPHA,0,0xff);
  
  if ((err=egg_native_call_client_render())<0) {
    if (err!=-2) fprintf(stderr,"%s: Error rendering game.\n",egg.exename);
    return -2;
  }
  
  softrender_finalize_frame(egg.softrender);
  
  if (egg.hostio->video->type->fb_end(egg.hostio->video)<0) {
    fprintf(stderr,"%s: Failed to commit direct-render frame.\n",egg.exename);
    return -2;
  }
  return 0;
}

/* Update.
 */
 
static int egg_native_update() {
  int err;
  if (hostio_update(egg.hostio)<0) {
    fprintf(stderr,"%s: Unspecified error updating drivers.\n",egg.exename);
    return -2;
  }
  if ((err=egg_native_net_update())<0) {
    if (err!=-2) fprintf(stderr,"%s: Error updating network.\n",egg.exename);
    return -2;
  }
  if ((err=egg_native_vm_update())<0) {
    if (err!=-2) fprintf(stderr,"%s: Error updating Javascript or Wasm runtime.\n",egg.exename);
    return -2;
  }
  
  double elapsed=timer_tick(&egg.timer);
  if ((err=egg_native_call_client_update(elapsed))<0) {
    if (err!=-2) fprintf(stderr,"%s: Error updating game.\n",egg.exename);
    return -2;
  }
  
  if (egg.localstore.dirty&&egg.storepath&&egg.localstore.save_permit) {
    if (localstore_save(&egg.localstore,egg.storepath)<0) {
      fprintf(stderr,"%s: Failed to save game data.\n",egg.storepath);
    } else {
      fprintf(stderr,"%s: Saved.\n",egg.storepath);
    }
  }
  
  if (egg.render) err=egg_native_render_gx();
  else if (egg.softrender) err=egg_native_render_fb();
  else err=-1;
  if (err<0) {
    if (err!=-2) fprintf(stderr,
      "%s: Failed to render scene (%s)\n",
      egg.exename,egg.render?"gx":egg.softrender?"soft":"no renderer"
    );
    return -2;
  }

  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  signal(SIGINT,egg_native_rcvsig);
  int err;
  
  if ((err=egg_native_configure(argc,argv))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error reading configuration.\n",egg.exename);
    return 1;
  }
  if (egg.terminate) return 0; // eg --help
  
  if ((err=egg_native_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing.\n",egg.exename);
    egg_native_quit(1);
    return 1;
  }
  
  fprintf(stderr,"%s: Running.\n",egg.exename);
  while (!egg.sigc&&!egg.terminate) {
    if ((err=egg_native_update())<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error updating.\n",egg.exename);
      egg_native_quit(1);
      return 1;
    }
  }
  
  egg_native_quit(0);
  return 0;
}

/* App icon. (default only, when the ROM doesn't provide one).
 */
 
static const uint8_t egg_appicon_src[]={
#define ROW(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) (a<<6)|(b<<4)|(c<<2)|d,(e<<6)|(f<<4)|(g<<2)|h,(i<<6)|(j<<4)|(k<<2)|l,(m<<6)|(n<<4)|(o<<2)|p,
#define _ 0
#define K 1
#define W 2
#define R 3
  ROW(_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)
  ROW(_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)
  ROW(_,_,_,_,K,_,_,_,_,_,_,K,_,_,_,_)
  ROW(_,_,_,K,W,K,_,_,_,_,K,W,K,_,_,_)
  ROW(_,_,K,W,W,W,K,_,_,K,W,W,W,K,_,_)
  ROW(_,_,K,W,W,W,K,_,_,K,W,W,W,K,_,_)
  ROW(_,K,W,W,W,W,W,K,K,W,W,W,W,W,K,_)
  ROW(_,K,W,W,W,W,W,K,K,W,W,W,W,W,K,_)
  ROW(_,K,W,W,W,W,W,K,K,W,W,W,W,W,K,_)
  ROW(_,_,K,W,W,W,K,_,_,K,W,W,W,K,_,_)
  ROW(_,_,_,K,K,K,_,_,_,_,K,K,K,_,_,_)
  ROW(_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)
  ROW(_,R,_,_,_,_,_,_,_,_,_,_,_,_,R,_)
  ROW(_,_,R,_,_,_,_,_,_,_,_,_,_,R,_,_)
  ROW(_,_,_,R,R,R,R,R,R,R,R,R,R,_,_,_)
  ROW(_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)
#undef ROW
};
 
void egg_appicon_init() {
  if (!(egg.appicon_rgba=malloc(16*16*4))) return;
  const uint8_t *src=egg_appicon_src; // i2
  int srcshift=6;
  uint8_t *dst=egg.appicon_rgba;
  int pxc=16*16;
  for (;pxc-->0;dst+=4) {
    switch (((*src)>>srcshift)&3) {
      case _: dst[0]=dst[1]=dst[2]=dst[3]=0; break;
      case K: dst[0]=dst[1]=dst[2]=0x00; dst[3]=0xff; break;
      case W: dst[0]=dst[1]=dst[2]=dst[3]=0xff; break;
      case R: dst[0]=0xff; dst[1]=0x00; dst[2]=0x00; dst[3]=0xff; break;
    }
    if (srcshift) srcshift-=2;
    else { srcshift=6; src++; }
  }
  egg.appiconw=16;
  egg.appiconh=16;
}
#undef _
#undef K
#undef W
#undef R
