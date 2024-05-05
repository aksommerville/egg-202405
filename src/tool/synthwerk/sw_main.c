#include "synthwerk_internal.h"
#include <signal.h>

struct sw sw={0};

/* Signals.
 */
 
static void sw_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(sw.sigc)>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* MIDI event.
 * We'll send everything to synth on channel 8, regardless of what device or channel it comes in on.
 */
 
static void sw_midi_event(struct ossmidi *ossmidi,struct ossmidi_device *device,const struct midi_event *event) {
  fprintf(stderr,"%s %02x %02x %02x %02x\n",__func__,event->chid,event->opcode,event->a,event->b);
  
  // My MPK is misbehaving just now and emitting noise Wheel events. Drop them all.
  if (event->opcode==0xe0) return;
  
  if (!sw.audio->type->lock||(sw.audio->type->lock(sw.audio)>=0)) {
    synth_event(sw.synth,8,event->opcode,event->a,event->b,0);
    if (sw.audio->type->unlock) sw.audio->type->unlock(sw.audio);
  }
}

/* Fill PCM output.
 */
 
static void sw_pcm_out(int16_t *v,int c,struct hostio_audio *audio) {
  synth_updatei(v,c,sw.synth);
}

/* Main.
 */
 
int main(int argc,char **argv) {
  fprintf(stderr,"synthwerk main...\n");
  signal(SIGINT,sw_rcvsig);
  
  /* Launch HTTP server.
   */
  #if SW_USE_HTTP
  struct http_context_delegate http_delegate={
    .cb_serve=sw_serve,
  };
  if (!(sw.http=http_context_new(&http_delegate))) return -1;
  if (http_listen(sw.http,1,SW_HTTP_PORT)<0) {
    fprintf(stderr,"%s: Failed to open HTTP server on port %d\n",argv[0],SW_HTTP_PORT);
    return 1;
  }
  fprintf(stderr,"Serving HTTP on port %d.\n",SW_HTTP_PORT);
  #else
  fprintf(stderr,"HTTP service disabled.\n");
  #endif
  
  /* Acquire audio driver and synthesizer.
   */
  #if SW_USE_AUDIO
  struct hostio_audio_delegate audio_delegate={
    .cb_pcm_out=sw_pcm_out,
  };
  struct hostio_audio_setup audio_setup={
    .rate=44100,
    .chanc=1,
  };
  int p=0; for (;;p++) {
    const struct hostio_audio_type *type=hostio_audio_type_by_index(p);
    if (!type) {
      fprintf(stderr,"Failed to initialize any audio driver.\n");
      return 1;
    }
    if (!strcmp(type->name,"alsafd")) continue; // alsafd prevents Chrome from opening an AudioContext. yuck.
    if (sw.audio=hostio_audio_new(type,&audio_delegate,&audio_setup)) {
      fprintf(stderr,
        "Using audio driver '%s', rate=%d, chanc=%d\n",
        sw.audio->type->name,sw.audio->rate,sw.audio->chanc
      );
      break;
    }
  }
  
  /* Initialize romr with an Egg file containing the GM drums.
   * Keep going if this fails, it's not the end of the world.
   */
  {
    void *romserial=0;
    int romserialc=file_read(&romserial,"out/rom/hello.egg");
    if (romserialc>0) {
      romr_decode_borrow(&sw.romr,romserial,romserialc);
    }
  }
  
  /* Create synthesizer.
   */
  if (!(sw.synth=synth_new(sw.audio->rate,sw.audio->chanc,&sw.romr))) {
    fprintf(stderr,"Failed to create synthesizer.\n");
    return 1;
  }
  sw.ins.mode=SYNTH_CHANNEL_MODE_BLIP;
  synth_override_pid_0(sw.synth,&sw.ins);
  
  /* Load the config, or tell the user where to save it.
   */
  if (synthwerk_load_file()<0) {
    fprintf(stderr,"%s: Failed to load initial content. Using BLIP to start.\n",SW_INSTRUMENT_FILE);
  } else {
    fprintf(stderr,"%s: Loaded. Edit this file to replace instrument live.\n",SW_INSTRUMENT_FILE);
  }
  
  /* Prep inotify.
   */
  if ((sw.infd=inotify_init())<0) return 1;
  inotify_add_watch(sw.infd,SW_INSTRUMENT_DIR,IN_CREATE|IN_ATTRIB|IN_MOVED_TO);
  
  sw.audio->type->play(sw.audio,1);
  #else
  fprintf(stderr,"Native audio disabled.\n");
  #endif
  
  /* Create MIDI-in manager.
   */
  #if SW_USE_MIDI_IN
  struct ossmidi_delegate ossmidi_delegate={
    .cb_event=sw_midi_event,
  };
  if (!(sw.ossmidi=ossmidi_new(&ossmidi_delegate))) return 1;
  #else
  fprintf(stderr,"MIDI-In disabled.\n");
  #endif
  
  // One of the pollable things must have a nonzero timeout, and the other should be zero.
  // Recommend nonzero for ossmidi if enabled, since it's more sensitive to latency.
  #if SW_USE_MIDI_IN && SW_USE_HTTP
    int ossto=100;
    int httpto=0;
  #elif SW_USE_MIDI_IN
    int ossto=100;
    int httpto=0;
  #elif SW_USE_HTTP
    int ossto=0;
    int httpto=100;
  #else
    fprintf(stderr,"All features disabled. Aborting.\n");
    return 1;
  #endif
  
  /* Main loop.
   */
  while (!sw.sigc) {
    #if SW_USE_MIDI_IN
    if (ossmidi_update(sw.ossmidi,ossto)<0) {
      fprintf(stderr,"ossmidi_update failed\n");
      return 1;
    }
    #endif
    
    #if SW_USE_AUDIO
    if (sw.audio->type->update) {
      if (sw.audio->type->update(sw.audio)<0) {
        fprintf(stderr,"Failed to update audio driver.\n");
        return 1;
      }
    }
    #endif
    
    #if SW_USE_HTTP
    if (http_update(sw.http,httpto)<0) {
      fprintf(stderr,"Error updating HTTP context.\n");
      return 1;
    }
    #endif
    
    #if SW_USE_AUDIO
    struct pollfd pollfd={.fd=sw.infd,.events=POLLIN|POLLERR|POLLHUP};
    if (poll(&pollfd,1,0)>0) {
      char tmp[1024];
      int tmpc=read(sw.infd,tmp,sizeof(tmp));
      if (tmpc<=0) {
        fprintf(stderr,"Error reading inotify.\n");
        return 1;
      }
      int tmpp=0,update=0;
      while (1) {
        if (tmpp>tmpc-sizeof(struct inotify_event)) break;
        const struct inotify_event *event=(const struct inotify_event*)(tmp+tmpp);
        tmpp+=sizeof(struct inotify_event);
        if (tmpp>tmpc-event->len) break;
        tmpp+=event->len;
        int baselen=0;
        while ((baselen<event->len)&&event->name[baselen]) baselen++;
        if ((baselen==sizeof(SW_INSTRUMENT_BASE)-1)&&!memcmp(event->name,SW_INSTRUMENT_BASE,baselen)) {
          update=1;
          break;
        }
      }
      if (update) {
        if (synthwerk_load_file()<0) {
          fprintf(stderr,"%s: Load failed!\n",SW_INSTRUMENT_FILE);
        } else {
          fprintf(stderr,"%s: Reloaded.\n",SW_INSTRUMENT_FILE);
        }
      }
    }
    #endif
  }
  
  /* Clean up.
   */
  if (sw.infd>=0) close(sw.infd);
  hostio_audio_del(sw.audio);
  synth_del(sw.synth);
  ossmidi_del(sw.ossmidi);
  http_context_del(sw.http);
  fprintf(stderr,"Normal exit.\n");
  return 0;
}
