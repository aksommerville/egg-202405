#include "synth_internal.h"

/* Frames from milliseconds.
 */
 
static int synth_frames_from_ms(const struct synth *synth,int ms) {
  if (ms<1) return 0;
  double framecf=(double)ms*synth->frames_per_ms;
  int framec=(int)framecf;
  if (framec<1) return 1;
  return framec;
}

/* Update song.
 * Call when (songp&&!songdelay).
 * Reads the next event from the song, and processes it.
 * Updates all song state.
 * If we fail, caller should drop the song.
 */
 
static int synth_read_and_deliver_song_event(struct synth *synth) {

  // Implicit EOF.
  if (synth->songp>=synth->songserialc) {
   _eof_:;
    if (synth->songrepeat) {
      synth->songp=(synth->songserial[6]<<8)|synth->songserial[7];
      synth->songdelay=1; // Force a tiny delay at repeat, in case the song has no delays of its own.
      return 0;
    } else {
      synth->songp=0;
      return -1;
    }
  }
  
  uint8_t lead=synth->songserial[synth->songp++];
  
  // Explicit EOF.
  if (!lead) goto _eof_;
  
  // Delay.
  if (!(lead&0x80)) {
    synth->songdelay=synth_frames_from_ms(synth,lead);
    return 0;
  }
  
  // Note.
  if ((lead&0xf0)==0x80) {
    if (synth->songp>synth->songserialc-2) goto _eof_;
    uint8_t a=synth->songserial[synth->songp++];
    uint8_t b=synth->songserial[synth->songp++];
    uint8_t chid=a>>5;
    uint8_t velocity=(lead&0x0f)<<3;
    velocity|=velocity>>4;
    uint8_t noteid=((a&0x1f)<<2)|(b>>6);
    int dur=synth_frames_from_ms(synth,(b&0x3f)<<5);
    synth_play_note(synth,chid,noteid,velocity,dur);
    return 0;
  }
  
  // Fire-and-forget.
  if ((lead&0xf0)==0x90) {
    if (synth->songp>synth->songserialc-1) goto _eof_;
    uint8_t a=synth->songserial[synth->songp++];
    uint8_t chid=((lead&0x03)<<1)|(a>>7);
    uint8_t noteid=(a&0x7f);
    uint8_t velocity=(lead&0x0c)<<2;
    velocity|=velocity>>2;
    velocity|=velocity>>4;
    synth_play_note(synth,chid,noteid,velocity,0);
    return 0;
  }
  
  // Wheel.
  if ((lead&0xf8)==0xa0) {
    if (synth->songp>synth->songserialc-1) goto _eof_;
    uint8_t v=synth->songserial[synth->songp++];
    uint8_t chid=lead&0x07;
    synth_turn_wheel(synth,chid,v);
    return 0;
  }
  
  // Everything else is reserved. (10101xxx, 1011xxxx, 11xxxxxx)
  goto _eof_;
}

/* Update voice.
 */
 
static inline void synth_voice_update(float *v,int c,struct synth *synth,struct synth_voice *voice) {
  int i=c;
  
  // Rock: Envelope-driven wave mixer between fundamental and voice's wave.
  if (voice->rock.atkt) {
    for (;i-->0;v++) {
      float rock=synth_env_next(&voice->rock);
      float wet=voice->wave[voice->p>>SYNTH_WAVE_SHIFT];
      float dry=synth->sine[voice->p>>SYNTH_WAVE_SHIFT];
      float sample=wet*rock+dry*(1.0f-rock);
      (*v)+=sample*synth_env_next(&voice->level);
      voice->p+=voice->dp;
    }
    
  // FM.
  } else if (voice->moddp) {
    float fdp=(float)voice->dp;
    for (;i-->0;v++) {
      (*v)+=voice->wave[voice->p>>SYNTH_WAVE_SHIFT]*synth_env_next(&voice->level);
      float mod=synth->sine[voice->modp>>SYNTH_WAVE_SHIFT];
      mod*=synth_env_next(&voice->modrange);
      voice->modp+=voice->moddp;
      uint32_t dp=voice->dp+(uint32_t)(fdp*mod);
      voice->p+=dp;
    }
  
  // Straight wave.
  } else {
    for (;i-->0;v++) {
      (*v)+=voice->wave[voice->p>>SYNTH_WAVE_SHIFT]*synth_env_next(&voice->level);
      voice->p+=voice->dp;
    }
  }
  
  if (synth_env_is_finished(&voice->level)) {
    voice->origin=0;
  }
}

/* Update playback.
 */
 
static inline void synth_playback_update(float *v,int c,struct synth *synth,struct synth_playback *playback) {
  int updc=playback->srcc-playback->srcp;
  if (updc>c) updc=c;
  int i=updc;
  for (;i-->0;v++) {
    (*v)+=playback->src[playback->srcp++]*playback->ltrim;
  }
  if (playback->srcp>=playback->srcc) {
    playback->srcc=0;
  }
}

/* Update mono floating-point to buffer of limited size.
 * Buffer must be initially zeroed.
 * This is where the real work begins.
 * TODO We're mono exclusively right now. Probably want to go stereo in the future. Song and sound APIs already provide it.
 */
 
void synth_updatef_mono(float *v,int c,struct synth *synth) {
  synth->framec+=c;
  
  if (synth->songtransition) {
    if (!synth_has_song_voices(synth)) {
      synth_complete_song_transition(synth);
    }
  }
  
  while (c>0) {
  
    int updc=c;
    while (synth->songp&&!synth->songdelay&&!synth->songtransition) {
      if (synth_read_and_deliver_song_event(synth)<0) {
        synth_end_song(synth);
        updc=c;
      }
    }
    if (synth->songdelay>0) {
      if (synth->songdelay<updc) updc=synth->songdelay;
      synth->songdelay-=updc;
    }
    
    int i;
    struct synth_voice *voice=synth->voicev;
    for (i=synth->voicec;i-->0;voice++) {
      if (!voice->origin) continue;
      synth_voice_update(v,updc,synth,voice);
    }
    struct synth_proc *proc=synth->procv;
    for (i=synth->procc;i-->0;proc++) {
      if (!proc->update) continue;
      proc->update(v,c,synth,proc);
    }
    struct synth_playback *playback=synth->playbackv;
    for (i=synth->playbackc;i-->0;playback++) {
      if (!playback->srcc) continue;
      synth_playback_update(v,c,synth,playback);
    }
    
    v+=updc;
    c-=updc;
  }
}
