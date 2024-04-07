#include "synth_internal.h"

/* Cleanup.
 */

void synth_del(struct synth *synth) {
  int i;
  if (!synth) return;
  synth_cache_del(synth->cache);
  synth_song_del(synth->song);
  synth_song_del(synth->song_next);
  for (i=SYNTH_CHANNEL_COUNT;i-->0;) synth_channel_del(synth->channelv[i]);
  for (i=synth->voicec;i-->0;) synth_voice_cleanup(synth->voicev+i);
  for (i=synth->procc;i-->0;) synth_proc_cleanup(synth->procv+i);
  for (i=synth->playbackc;i-->0;) synth_playback_cleanup(synth->playbackv+i);
  if (synth->printerv) {
    while (synth->printerc-->0) sfg_printer_del(synth->printerv[synth->printerc]);
    free(synth->printerv);
  }
  free(synth);
}

/* Precalculate some things we're going to need often.
 */
 
static void synth_precalculate_freq(struct synth *synth) {
  float frate=(float)synth->rate;
  float *f=synth->ffreqv;
  uint32_t *i=synth->ifreqv;
  int noteid=0;
  for (;noteid<0x80;noteid++,f++,i++) {
    *f=midi_frequency_for_noteid(noteid)/frate;
    *i=(*f)*4294967296.0f;
  }
}

static void synth_precalculate_sine(struct synth *synth) {
  float *dst=synth->sine;
  float p=0.0f,dp=(M_PI*2.0f)/(float)SYNTH_WAVE_SIZE_SAMPLES;
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;p+=dp,dst++) *dst=sinf(p);
}

/* New.
 */

struct synth *synth_new(
  int rate,int chanc,
  struct romr *romr
) {
  if ((rate<200)||(rate>200000)) return 0;
  if ((chanc<1)||(chanc>8)) return 0;
  struct synth *synth=calloc(1,sizeof(struct synth));
  if (!synth) return 0;
  if (!(synth->cache=synth_cache_new())) {
    free(synth);
    return 0;
  }
  synth->rate=rate;
  synth->chanc=chanc;
  synth->romr=romr;
  synth->buffer_limit=(SYNTH_BUFFER_LIMIT/chanc)*chanc;
  synth->qlevel=32000.0f;
  synth_precalculate_freq(synth);
  synth_precalculate_sine(synth);
  return synth;
}

/* Drop (synth->song) and arrange for its signal bits to wind down.
 */

void synth_end_song(struct synth *synth) {
  if (!synth->song) return;
  
  synth_song_del(synth->song);
  synth->song=0;
  
  int i;
  struct synth_voice *voice=synth->voicev;
  for (i=synth->voicec;i-->0;voice++) synth_voice_release(synth,voice);
  struct synth_proc *proc=synth->procv;
  for (i=synth->procc;i-->0;proc++) synth_proc_release(synth,proc);
  // Nothing equivalent for playbacks; they always run to completion.
  // Do not delete channels at this time -- the winding-down voices and proc might depend on them.
  
  for (i=SYNTH_SONG_CHANNEL_COUNT;i-->0;) synth->pidv[i]=0;
}

/* Check for song-related signal objects.
 * It's important that these finish before welcoming the next song.
 */
 
int synth_has_song_voices(const struct synth *synth) {
  int i;
  const struct synth_voice *voice=synth->voicev;
  for (i=synth->voicec;i-->0;voice++) if (synth_voice_is_song(voice)) return 1;
  const struct synth_proc *proc=synth->procv;
  for (i=synth->procc;i-->0;proc++) if (synth_proc_is_song(proc)) return 1;
  return 0;
}

/* Having just reassigned (synth->song), replace channels as needed to begin playback.
 */
 
void synth_welcome_song(struct synth *synth) {
  int i=SYNTH_SONG_CHANNEL_COUNT;
  for (;i-->0;) {
    synth_channel_del(synth->channelv[i]);
    synth->channelv[i]=0;
  }
  if (!synth->song) return;
  synth_song_init_channels(synth,synth->song);
}

/* Play song from resource.
 */

void synth_play_song(struct synth *synth,int qual,int songid,int force,int repeat) {
  
  // Without (force), check for easy outs.
  if (!force) {
    // If there's a "next", that's the one to look at.
    if (synth->song_next) {
      if (synth_song_is_resource(synth->song_next,qual,songid)) return;
    } else if (synth->song) {
      if (synth_song_is_resource(synth->song,qual,songid)) return;
    }
  }
  
  // Acquire the serial data.
  // If empty, don't abort -- that means play silence.
  const void *serial=0;
  int serialc=synth->romr?romr_get_qualified(&serial,synth->romr,EGG_TID_song,qual,songid):0;
  struct synth_song *nsong=0;
  if (serialc>0) {
    if (!(nsong=synth_song_new(synth,serial,serialc,1,repeat,qual,songid))) return;
  }
  
  // If we don't currently have a song running or pending, start the new one immediately.
  if (!synth->song&&!synth->song_next&&!synth_has_song_voices(synth)) {
    synth->song=nsong;
    synth_welcome_song(synth);
    return;
  }
  
  // Install as "next", drop the current song, and let nature take its course.
  synth_song_del(synth->song_next);
  synth->song_next=nsong;
  synth_end_song(synth);
}

/* Play song from serial data.
 */
 
void synth_play_song_serial(
  struct synth *synth,
  const void *src,int srcc,
  int safe_to_borrow,
  int repeat
) {
  if (srcc<1) {
    synth_song_del(synth->song_next);
    synth->song_next=0;
    synth_end_song(synth);
    return;
  }
  struct synth_song *nsong=synth_song_new(synth,src,srcc,safe_to_borrow,repeat,-1,-1);
  if (!nsong) return;
  if (!synth->song&&!synth->song_next&&!synth_has_song_voices(synth)) {
    synth->song=nsong;
    synth_welcome_song(synth);
    return;
  }
  synth_song_del(synth->song_next);
  synth->song_next=nsong;
  synth_end_song(synth);
}

/* Create a pcm printer and register it.
 * On success, returns WEAK reference to the new PCM dump.
 * The printer stays resident until next update, even if it runs to completion immediately, to keep the pcm alive.
 */

static struct sfg_pcm *synth_begin_pcmprint(struct synth *synth,const void *src,int srcc) {
  if (synth->printerc>=synth->printera) {
    int na=synth->printera+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(synth->printerv,sizeof(void*)*na);
    if (!nv) return 0;
    synth->printerv=nv;
    synth->printera=na;
  }
  struct sfg_printer *printer=sfg_printer_new(synth->rate,src,srcc);
  if (!printer) return 0;
  synth->printerv[synth->printerc++]=printer;
  if (synth->update_in_progress>0) {
    sfg_printer_update(printer,synth->update_in_progress);
  }
  return sfg_printer_get_pcm(printer);
}

/* Begin playing PCM.
 */
 
static void synth_play_pcm(struct synth *synth,struct sfg_pcm *pcm,float trim,float pan) {
  struct synth_playback *playback=synth_playback_new(synth);
  synth_playback_init(synth,playback,pcm,trim,pan);
}

/* Play sound from resource.
 */

void synth_play_sound(struct synth *synth,int qual,int soundid,float trim,float pan) {
  if (trim<=0.0) return;

  // Find insertion point in cache, and if we already have it, start playing.
  int cachep=synth_cache_search(synth->cache,qual,soundid);
  if (cachep>=0) {
    struct sfg_pcm *pcm=synth_cache_get(synth->cache,cachep);
    if (pcm) synth_play_pcm(synth,pcm,trim,pan);
    return;
  }
  if (!synth->romr) return;
  cachep=-cachep-1;
  
  // Find serial data.
  const void *serial=0;
  int serialc=romr_get_qualified(&serial,synth->romr,EGG_TID_sound,qual,soundid);
  if (serialc<1) return;
  
  // Add a pcm printer.
  struct sfg_pcm *pcm=synth_begin_pcmprint(synth,serial,serialc);
  if (!pcm) return;
  
  // Add to cache and start playing.
  synth_cache_add(synth->cache,cachep,qual,soundid,pcm);
  synth_play_pcm(synth,pcm,trim,pan);
}

/* Play sound from serial data.
 */
 
void synth_play_sound_serial(
  struct synth *synth,
  const void *src,int srcc,
  float trim,float pan
) {
  if (trim<=0.0) return;
  if (!src||(srcc<1)) return;
  struct sfg_pcm *pcm=synth_begin_pcmprint(synth,src,srcc);
  if (!pcm) return;
  synth_play_pcm(synth,pcm,trim,pan);
}

/* Get playhead.
 */
 
double synth_get_playhead(struct synth *synth) {
  // Use "next" if present, so we're discussing the new song as soon as the user asks for it.
  // Its playhead will linger at zero for a little while, until it starts playing for real.
  if (synth->song_next) return synth_song_get_playhead(synth,synth->song_next);
  if (synth->song) return synth_song_get_playhead(synth,synth->song);
  return -1.0;
}

/* Drop any voice or proc that might refer to the given channel.
 */
 
void synth_drop_voices_for_channel(struct synth *synth,uint8_t chid) {
  int i;
  struct synth_voice *voice=synth->voicev;
  for (i=synth->voicec;i-->0;voice++) {
    if (!synth_voice_is_channel(voice,chid)) continue;
    synth_voice_cleanup(voice);
    memset(voice,0,sizeof(struct synth_voice));
  }
  struct synth_proc *proc=synth->procv;
  for (i=synth->procc;i-->0;proc++) {
    if (!synth_proc_is_channel(proc,chid)) continue;
    synth_proc_cleanup(proc);
    memset(proc,0,sizeof(struct synth_proc));
  }
}

/* Get signal object, evicting an old one if needed.
 */
 
#define GETOBJ(t,T) \
  struct synth_##t *synth_##t##_new(struct synth *synth) { \
    if (synth->t##c<SYNTH_##T##_LIMIT) { \
      struct synth_##t *obj=synth->t##v+synth->t##c++; \
      memset(obj,0,sizeof(struct synth_##t)); \
      return obj; \
    } \
    struct synth_##t *obj=synth->t##v; \
    struct synth_##t *q=synth->t##v; \
    int i=SYNTH_##T##_LIMIT; \
    for (;i-->0;q++) { \
      if (synth_##t##_is_defunct(q)) { \
        obj=q; \
        break; \
      } \
      if (synth_##t##_compare(obj,q)>0) obj=q; \
    } \
    synth_##t##_cleanup(obj); \
    memset(obj,0,sizeof(struct synth_##t)); \
    return obj; \
  }
  
GETOBJ(voice,VOICE)
GETOBJ(proc,PROC)
GETOBJ(playback,PLAYBACK)

#undef GETOBJ

/* Search signal objects.
 */
 
struct synth_voice *synth_find_voice_by_chid_noteid(struct synth *synth,uint8_t chid,uint8_t noteid) {
  struct synth_voice *voice=synth->voicev;
  int i=synth->voicec;
  for (;i-->0;voice++) {
    if (synth_voice_is_defunct(voice)) continue;
    if (voice->chid!=chid) continue;
    if (voice->noteid!=noteid) continue;
    return voice;
  }
  return 0;
}

struct synth_proc *synth_find_proc_by_chid(struct synth *synth,uint8_t chid) {
  struct synth_proc *proc=synth->procv;
  int i=synth->procc;
  for (;i-->0;proc++) {
    if (synth_proc_is_defunct(proc)) continue;
    if (proc->chid!=chid) continue;
    return proc;
  }
  return 0;
}

/* Global tempo.
 */
 
int synth_frames_per_beat(const struct synth *synth) {
  if (!synth->song) return synth->rate>>1;
  if (synth->song->tempo<1) return synth->rate>>1;
  return (synth->song->tempo*synth->rate)/1000;
}
