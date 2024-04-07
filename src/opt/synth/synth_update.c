#include "synth_internal.h"

/* Update printers.
 */
 
static void synth_update_printers(struct synth *synth,int framec) {
  int i=synth->printerc;
  struct sfg_printer **p=synth->printerv+i-1;
  for (;i-->0;p--) {
    struct sfg_printer *printer=*p;
    int err=sfg_printer_update(printer,framec);
    if (err) {
      sfg_printer_del(printer);
      synth->printerc--;
      memmove(p,p+1,sizeof(void*)*(synth->printerc-i));
    }
  }
}

/* Drop defunct signal-graph objects from the end of their list.
 * There can be defunct ones mid-list too, but we're not going to look for those here.
 * (When a new object gets added, it will check for those inner defuncts, would be excessive here).
 */
 
static void synth_reap_defunct_objects(struct synth *synth) {
  while (synth->voicec&&synth_voice_is_defunct(synth->voicev+synth->voicec-1)) {
    synth->voicec--;
    synth_voice_cleanup(synth->voicev+synth->voicec);
  }
  while (synth->procc&&synth_proc_is_defunct(synth->procv+synth->procc-1)) {
    synth->procc--;
    synth_proc_cleanup(synth->procv+synth->procc);
  }
  while (synth->playbackc&&synth_playback_is_defunct(synth->playbackv+synth->playbackc-1)) {
    synth->playbackc--;
    synth_playback_cleanup(synth->playbackv+synth->playbackc);
  }
}

/* Update, floating-point, mono, limited length.
 * Buffer must be zeroed first.
 * Here we can begin the real work.
 */
 
static void synth_updatef_mono(float *v,int c,struct synth *synth) {
  while (c>0) {
    
    int updc=c;
    if (synth->song) {
      int err=synth_song_update(synth,synth->song);
      if (err<=0) {
        synth_end_song(synth);
      } else {
        if (err<updc) updc=err;
        synth_song_advance(synth->song,updc);
      }
    } else if (synth->song_next&&!synth_has_song_voices(synth)) {
      synth->song=synth->song_next;
      synth->song_next=0;
      synth_welcome_song(synth);
      // Don't bother updating it this time around, we'll start on the next pass.
    }
    
    int i;
    struct synth_voice *voice=synth->voicev;
    for (i=synth->voicec;i-->0;voice++) synth_voice_update(v,updc,synth,voice);
    struct synth_proc *proc=synth->procv;
    for (i=synth->procc;i-->0;proc++) synth_proc_update(v,updc,synth,proc);
    struct synth_playback *playback=synth->playbackv;
    for (i=synth->playbackc;i-->0;playback++) synth_playback_update(v,updc,synth,playback);
    
    v+=updc;
    c-=updc;
  }
}

/* Update, floating-point, all channels, limited length.
 * Buffer must be zeroed first.
 * Call out for a mono signal, then expand it.
 */
 
static void synth_expand_stereo(float *v,int framec) {
  const float *src=v+framec;
  float *dst=v+(framec<<1);
  while (framec-->0) {
    src--;
    *(--dst)=*src;
    *(--dst)=*src;
  }
}

static void synth_expand_multi(float *v,int framec,int chanc) {
  const float *src=v+framec;
  float *dst=v+framec*chanc;
  while (framec-->0) {
    src--;
    int i=chanc;
    while (i-->0) {
      *(--dst)=*src;
    }
  }
}
 
static void synth_updatef_limited(float *v,int c,struct synth *synth) {
  switch (synth->chanc) {
    case 1: {
        synth_updatef_mono(v,c,synth);
      } break;
    case 2: {
        //TODO I expect eventually we'll want stereo output. Our API mostly is ready for it, but internals not.
        int framec=c>>1;
        synth_updatef_mono(v,framec,synth);
        synth_expand_stereo(v,framec);
      } break;
    default: {
        int framec=c/synth->chanc;
        synth_updatef_mono(v,framec,synth);
        synth_expand_multi(v,framec,synth->chanc);
      } break;
  }
}

/* Update, floating-point, all channels, unlimited length.
 * Top level of update.
 */
 
void synth_updatef(float *v,int c,struct synth *synth) {

  int framec=c/synth->chanc;
  synth->framec+=framec;
  synth->update_in_progress=framec;
  synth_update_printers(synth,framec);

  memset(v,0,sizeof(float)*c);
  while (c>=synth->buffer_limit) {
    synth_updatef_limited(v,synth->buffer_limit,synth);
    v+=synth->buffer_limit;
    c-=synth->buffer_limit;
  }
  if (c>0) {
    synth_updatef_limited(v,c,synth);
  }
  
  synth->update_in_progress=0;
  synth_reap_defunct_objects(synth);
}

/* Update, integer, all channels, unlimited length.
 */
 
static void synth_quantize(int16_t *dst,const float *src,int c,float level) {
  for (;c-->0;dst++,src++) {
    *dst=(int16_t)((*src)*level);
  }
}
 
void synth_updatei(int16_t *v,int c,struct synth *synth) {
  while (c>=synth->buffer_limit) {
    synth_updatef(synth->qbuf,synth->buffer_limit,synth);
    synth_quantize(v,synth->qbuf,synth->buffer_limit,synth->qlevel);
    v+=synth->buffer_limit;
    c-=synth->buffer_limit;
  }
  if (c>0) {
    synth_updatef(synth->qbuf,c,synth);
    synth_quantize(v,synth->qbuf,c,synth->qlevel);
  }
}
