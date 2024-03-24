#include "synth_internal.h"

/* Delete context.
 */
 
static void synth_proc_cleanup(struct synth *synth,struct synth_proc *proc) {
  if (proc->del) {
    proc->del(synth,proc);
    proc->del=0;
  }
  proc->update=0;
}
 
void synth_del(struct synth *synth) {
  if (!synth) return;
  struct synth_proc *proc=synth->procv;
  for (;synth->procc-->0;proc++) synth_proc_cleanup(synth,proc);
  if (synth->qbuf) free(synth->qbuf);
  if (synth->songkeepalive) free(synth->songkeepalive);
  free(synth);
}

/* Precalculated tables of note frequencies, and a reference sine wave.
 */
 
static void synth_precalculate_frequencies(struct synth *synth) {
  float *dstf=synth->freqn_by_noteid;
  uint32_t *dsti=synth->freqi_by_noteid;
  int noteid=0;
  float frate=(float)synth->rate;
  for (;noteid<0x80;noteid++,dstf++,dsti++) {
    *dstf=midi_frequency_for_noteid(noteid)/frate;
    *dsti=(*dstf)*4294967296.0f;
  }
}

static void synth_precalculate_sine(struct synth *synth) {
  float *v=synth->sine;
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  float p=0.0f;
  float dp=(M_PI*2.0f)/SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;v++,p+=dp) *v=sinf(p);
}

/* New context.
 */

struct synth *synth_new(
  int rate,int chanc,
  struct romr *romr
) {
  if ((rate<200)||(rate>200000)) return 0;
  if ((chanc<1)||(chanc>8)) return 0;
  struct synth *synth=calloc(1,sizeof(struct synth));
  if (!synth) return 0;
  synth->rate=rate;
  synth->chanc=chanc;
  synth->romr=romr;
  synth->qlevel=SYNTH_QLEVEL_DEFAULT;
  synth->frames_per_ms=(double)synth->rate/1000.0;
  synth_precalculate_frequencies(synth);
  synth_precalculate_sine(synth);
  return synth;
}

/* Start of an update cycle, advance all PCM printers by so many frames.
 */
 
static void synth_update_printers(struct synth *synth,int framec) {
  synth->preprint_framec=framec;
  //TODO run printers
}

/* Update, floating-point.
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
      dst--;
      *dst=*src;
    }
  }
}
 
static void synth_updatef_limited(float *v,int c,struct synth *synth) {
  switch (synth->chanc) {
    case 1: {
        synth_updatef_mono(v,c,synth);
      } break;
    case 2: {
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

void synth_updatef(float *v,int c,struct synth *synth) {
  synth_update_printers(synth,c/synth->chanc);
  memset(v,0,c*sizeof(float));
  while (c>=SYNTH_UPDATE_LIMIT) {
    int subc=(SYNTH_UPDATE_LIMIT/synth->chanc)*synth->chanc;
    synth_updatef_limited(v,subc,synth);
    v+=subc;
    c-=subc;
  }
  if (c>0) {
    synth_updatef_limited(v,c,synth);
  }
  synth->preprint_framec=0;
  
  // Drop defunct objects from the end of each list.
  // It's OK to leave defunct ones in the middle anywhere, we'll get them eventually.
  while (synth->voicec>0) {
    struct synth_voice *voice=synth->voicev+synth->voicec-1;
    if (voice->origin) break;
    synth->voicec--;
  }
  while (synth->procc>0) {
    struct synth_proc *proc=synth->procv+synth->procc-1;
    if (proc->update) break;
    synth->procc--;
    synth_proc_cleanup(synth,proc);
  }
  while (synth->playbackc>0) {
    struct synth_playback *playback=synth->playbackv+synth->playbackc-1;
    if (playback->srcc) break;
    synth->playbackc--;
  }
}

/* Update, integer.
 */
 
static void synth_quantize(int16_t *dst,const float *src,int c,float level) {
  //int16_t lo=0,hi=hi;
  for (;c-->0;dst++,src++) {
    *dst=(int16_t)((*src)*level);
    //if (*dst<lo) lo=*dst; else if (*dst>hi) hi=*dst;
  }
  //fprintf(stderr,"master: %d..%d\n",lo,hi);
}
 
void synth_updatei(int16_t *v,int c,struct synth *synth) {
  if (!synth->qbuf) {
    synth->qbufa=(SYNTH_QBUF_SIZE/synth->chanc)*synth->chanc; // must be a multiple of chanc
    if (!(synth->qbuf=malloc(sizeof(float)*synth->qbufa))) {
      memset(v,0,c<<1);
      synth->qbufa=0;
      return;
    }
  }
  if (synth->qbufa<1) {
    // impossible, but if something is broken, just emit silence.
    memset(v,0,c<<1);
    return;
  }
  while (c>0) {
    int subc=synth->qbufa;
    if (subc>c) subc=c;
    synth_updatef(synth->qbuf,subc,synth);
    synth_quantize(v,synth->qbuf,subc,synth->qlevel);
    v+=subc;
    c-=subc;
  }
}

/* End current song.
 */
 
void synth_end_song(struct synth *synth) {
  
  int i;
  struct synth_voice *voice=synth->voicev;
  for (i=synth->voicec;i-->0;voice++) {
    if (voice->origin!=SYNTH_ORIGIN_SONG) continue;
    synth_voice_release(synth,voice);
  }
  struct synth_proc *proc=synth->procv;
  for (i=synth->procc;i-->0;proc++) {
    if (proc->origin!=SYNTH_ORIGIN_SONG) continue;
    synth_proc_release(synth,proc);
  }
  // Nothing equivalent for playbacks; let them run to completion.
  
  synth->songqual=0;
  synth->songid=0;
  synth->songserial=0;
  synth->songserialc=0;
  synth->songp=0;
  synth->songdelay=0;
}

/* Look for song-origin objects.
 * (ie can we complete the transition yet?)
 */
 
int synth_has_song_voices(const struct synth *synth) {
  int i;
  const struct synth_voice *voice=synth->voicev;
  for (i=synth->voicec;i-->0;voice++) {
    if (voice->origin==SYNTH_ORIGIN_SONG) return 1;
  }
  const struct synth_proc *proc=synth->procv;
  for (i=synth->procc;i-->0;proc++) {
    if (proc->origin==SYNTH_ORIGIN_SONG) return 1;
  }
  return 0;
}

/* Complete song transition.
 * This means applying the song's channel headers, then set (songtransition) false.
 */
 
void synth_complete_song_transition(struct synth *synth) {
  if (synth->songserialc>=40) {
    const uint8_t *src=synth->songserial+8;
    int i=SYNTH_CHANNEL_COUNT,chid=0;
    for (;i-->0;src+=4,chid++) {
      synth_apply_channel_header(synth,chid,src);
    }
  }
  synth->songtransition=0;
}

/* Play song from resource.
 */

void synth_play_song(struct synth *synth,int qual,int songid,int force,int repeat) {
  if (!force) {
    if ((qual==synth->songqual)&&(songid==synth->songid)) return;
  }
  if (!synth->romr) {
    synth_play_song_serial(synth,0,0,1,force,repeat);
    return;
  }
  const void *serial=0;
  int serialc=romr_get_qualified(&serial,synth->romr,EGG_TID_song,qual,songid);
  synth_play_song_serial(synth,serial,serialc,1,1,repeat);
  synth->songqual=qual;
  synth->songid=songid;
}

/* Play song from serial.
 */
 
void synth_play_song_serial(
  struct synth *synth,
  const void *src,int srcc,
  int safe_to_borrow,
  int force,int repeat
) {

  // Validate, drop current song, setup pointers.
  if (!src||(srcc<1)) srcc=0;
  if (!force) {
    if ((src==synth->songserial)&&(srcc==synth->songserialc)) return;
  }
  synth_end_song(synth);
  if (srcc&&!safe_to_borrow) {
    if (synth->songkeepalive) free(synth->songkeepalive);
    if (!(synth->songkeepalive=malloc(srcc))) return;
    memcpy(synth->songkeepalive,src,srcc);
    src=synth->songkeepalive;
  }
  synth->songserial=src;
  synth->songserialc=srcc;
  synth->songrepeat=repeat;
  if (srcc<1) return;
  
  // Validate header. We can return at any point, and no song will play.
  if ((synth->songserialc<40)||memcmp(synth->songserial,"\xbe\xee\xeeP",4)) return;
  int startp=(synth->songserial[4]<<8)|synth->songserial[5];
  int loopp=(synth->songserial[6]<<8)|synth->songserial[7];
  if ((startp<40)||(startp>=synth->songserialc)) return;
  if ((loopp<startp)||(loopp>=synth->songserialc)) return;
  
  // We'll need to read the channel headers and update live channels.
  // But before that can happen, wait for all voices and procs from the previous song to finish.
  synth->songtransition=1;
  synth->songp=startp;
}

/* Play sound from resource.
 */

void synth_play_sound(struct synth *synth,int qual,int soundid,double trim,double pan) {
  if (!synth->romr) return;
  if (trim<=0.0) return;
  //TODO Look for sounds already decoded.
  const void *serial=0;
  int serialc=romr_get_qualified(&serial,synth->romr,EGG_TID_sound,qual,soundid);
  if (serialc<1) return;
  synth_play_sound_serial(synth,serial,serialc,trim,pan);
}

/* Play sound from serial.
 */
 
void synth_play_sound_serial(
  struct synth *synth,
  const void *src,int srcc,
  double trim,double pan
) {
  if (!src||(srcc<1)) return;
  if (trim<=0.0) return;
  //TODO Decode sound.
  //TODO Add printer if applicable.
  //TODO Begin playback.
}

/* Bend normalized rate.
 */
 
static uint32_t synth_apply_wheel(uint32_t rate,float bend) {
  return (uint32_t)((float)rate*bend);
}

/* Play tuned note.
 */

void synth_play_note(struct synth *synth,uint8_t chid,uint8_t noteid,uint8_t velocity,int dur_frames) {
  //fprintf(stderr,"%s chid=%d noteid=0x%02x velocity=0x%02x dur=%d\n",__func__,chid,noteid,velocity,dur_frames);
  if (noteid&0x80) return;
  if (chid>=SYNTH_CHANNEL_COUNT) return;
  
  // If there's a noteless proc associated with this channel, it must consume the note.
  struct synth_proc *proc=synth->procv;
  int i=synth->procc;
  for (;i-->0;proc++) {
    if (!proc->update) continue;
    if (proc->chid!=chid) continue;
    if (proc->noteid!=0xff) continue;
    if (proc->note) proc->note(synth,proc,noteid,velocity,dur_frames);
    return;
  }
  
  // Normal case, create a voice based on channel parameters.
  const struct synth_channel *channel=synth->channelv+chid;
  struct synth_voice *voice=synth_voice_new(synth);
  voice->wave=synth->waves+chid*SYNTH_WAVE_SIZE_SAMPLES;
  voice->chid=chid;
  voice->noteid=noteid;
  voice->origin=SYNTH_ORIGIN_SONG;
  voice->p=0;
  voice->dp0=synth->freqi_by_noteid[noteid];
  voice->dp=synth_apply_wheel(voice->dp0,channel->bend);
  synth_env_init(&voice->level,&channel->level,velocity);
  synth_env_gain(&voice->level,channel->trim);
  if ((voice->level.sust=dur_frames)<1) voice->level.sust=1;
  if (channel->rock.atktlo) {
    synth_env_init(&voice->rock,&channel->rock,velocity);
  } else {
    voice->rock.atkt=0;
  }
  voice->modp=0;
  if (channel->modrel>0.0f) {
    voice->moddp=synth_apply_wheel(voice->dp0,channel->modrel);
    synth_env_init(&voice->modrange,&channel->modrange,velocity);
  } else {
    voice->moddp=0;
  }
}

/* Adjust pitch wheel.
 */
 
void synth_turn_wheel(struct synth *synth,uint8_t chid,uint8_t v) {
  fprintf(stderr,"%s chid=%d v=0x%02x\n",__func__,chid,v);
  if (chid>=SYNTH_CHANNEL_COUNT) return;
  struct synth_channel *channel=synth->channelv+chid;
  if (v==channel->wheel) return;
  if (channel->wheel_range<=0.0f) return;
  channel->wheel=v;
  channel->bend=powf(2.0f,(((v-0x40)*channel->wheel_range)/(128.0f*1200.0f)));//TODO verify, coding blind
  int i;
  struct synth_voice *voice=synth->voicev;
  for (i=synth->voicec;i-->0;voice++) {
    if (!voice->origin) continue;
    if (voice->chid!=chid) continue;
    voice->dp=synth_apply_wheel(voice->dp0,channel->bend);
  }
  struct synth_proc *proc=synth->procv;
  for (i=synth->procc;i-->0;proc++) {
    if (!proc->update) continue;
    if (proc->chid!=chid) continue;
    if (!proc->wheel) continue;
    proc->wheel(synth,proc,v);
  }
}

/* Get playhead.
 */

int synth_get_playhead(struct synth *synth) {
  return -1;//TODO
}

/* Allocate a new voice.
 */

struct synth_voice *synth_voice_new(struct synth *synth) {
  if (synth->voicec<SYNTH_VOICE_LIMIT) {
    struct synth_voice *voice=synth->voicev+synth->voicec++;
    memset(voice,0,sizeof(struct synth_voice));
    voice->birthday=synth->framec;
    return voice;
  }
  struct synth_voice *oldest=synth->voicev;
  struct synth_voice *q=synth->voicev;
  int i=SYNTH_VOICE_LIMIT;
  for (;i-->0;q++) {
    if (!q->origin) {
      memset(q,0,sizeof(struct synth_voice));
      q->birthday=synth->framec;
      return q;
    }
    if (q->birthday<oldest->birthday) {
      oldest=q;
    }
  }
  memset(oldest,0,sizeof(struct synth_voice));
  oldest->birthday=synth->framec;
  return oldest;
}

/* Allocate a new proc.
 */
 
struct synth_proc *synth_proc_new(struct synth *synth) {
  if (synth->procc<SYNTH_PROC_LIMIT) {
    struct synth_proc *proc=synth->procv+synth->procc++;
    memset(proc,0,sizeof(struct synth_proc));
    proc->birthday=synth->framec;
    return proc;
  }
  struct synth_proc *oldest=synth->procv;
  struct synth_proc *q=synth->procv;
  int i=SYNTH_PROC_LIMIT;
  for (;i-->0;q++) {
    if (!q->update) {
      synth_proc_cleanup(synth,q);
      memset(q,0,sizeof(struct synth_proc));
      q->birthday=synth->framec;
      return q;
    }
    if (q->birthday<oldest->birthday) {
      oldest=q;
    }
  }
  synth_proc_cleanup(synth,oldest);
  memset(oldest,0,sizeof(struct synth_proc));
  oldest->birthday=synth->framec;
  return oldest;
}

/* Allocate a new playback.
 */
 
struct synth_playback *synth_playback_new(struct synth *synth) {
  struct synth_playback *playback=0;
  if (synth->playbackc<SYNTH_PLAYBACK_LIMIT) {
    playback=synth->playbackv+synth->playbackc++;
  } else {
    int nearest_score=INT_MAX;
    struct synth_playback *nearest_termination=synth->playbackv;
    struct synth_playback *q=synth->playbackv;
    int i=SYNTH_PLAYBACK_LIMIT;
    for (;i-->0;q++) {
      if (!q->srcc) {
        playback=q;
        break;
      }
      int score=q->srcc-q->srcp;
      if (score<nearest_score) {
        nearest_termination=q;
        nearest_score=score;
      }
    }
    if (!playback) playback=nearest_termination;
  }
  memset(playback,0,sizeof(struct synth_playback));
  return playback;
}

/* Release voice or proc.
 */
 
void synth_voice_release(struct synth *synth,struct synth_voice *voice) {
  //TODO
  voice->origin=0;
}

void synth_proc_release(struct synth *synth,struct synth_proc *proc) {
  //TODO
  proc->update=0;
}
