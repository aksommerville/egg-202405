#include "synth_internal.h"

/* Delete.
 */

void synth_song_del(struct synth_song *song) {
  if (!song) return;
  if (song->keepalive) free(song->keepalive);
  free(song);
}

/* Validate.
 */
 
static int synth_song_validate(int *tempo,int *startp,int *loopp,const uint8_t *src,int srcc) {
  const int hdrlen=42;
  if (hdrlen!=10+4*SYNTH_SONG_CHANNEL_COUNT) return -1;
  if (!src) return -1;
  if (srcc<hdrlen) return -1;
  if (memcmp(src,"\xbe\xee\xeeP",4)) return -1;
  *tempo=(src[4]<<8)|src[5];
  *startp=(src[6]<<8)|src[7];
  *loopp=(src[8]<<8)|src[9];
  if (!*tempo) return -1;
  if (*startp<hdrlen) return -1;
  if (*loopp<*startp) return -1;
  if (*loopp>=srcc) return -1;
  return 0;
}

/* New.
 */

struct synth_song *synth_song_new(
  struct synth *synth,
  const void *src,int srcc,
  int safe_to_borrow,
  int repeat,
  int qual,int songid
) {
  int tempo,startp,loopp;
  if (synth_song_validate(&tempo,&startp,&loopp,src,srcc)<0) return 0;
  struct synth_song *song=calloc(1,sizeof(struct synth_song));
  if (!song) return 0;
  song->frames_per_ms=(float)synth->rate/1000.0f;
  song->tempo=tempo;
  song->startp=startp;
  song->loopp=loopp;
  song->repeat=repeat;
  song->qual=qual;
  song->songid=songid;
  if (safe_to_borrow) {
    song->src=src;
  } else {
    if (!(song->keepalive=malloc(srcc))) {
      synth_song_del(song);
      return 0;
    }
    memcpy(song->keepalive,src,srcc);
    song->src=song->keepalive;
  }
  song->srcc=srcc;
  return song;
}

/* Initialize channels in context.
 */

int synth_song_init_channels(struct synth *synth,struct synth_song *song) {
  const uint8_t *src=song->src+10;
  struct synth_channel **chanp=synth->channelv;
  int *pidv=synth->pidv;
  int chid=0;
  for (;chid<SYNTH_SONG_CHANNEL_COUNT;chid++,src+=4,chanp++,pidv++) {
    *pidv=src[0];
    if (!src[1]) continue; // volume zero; don't instantiate
    // src[3]=reserved
    struct synth_channel *channel=synth_channel_new(synth,chid,src[0]);
    if (!channel) return -1;
    synth_channel_control(synth,channel,MIDI_CONTROL_VOLUME_MSB,src[1]>>1);
    synth_channel_control(synth,channel,MIDI_CONTROL_PAN_MSB,src[2]>>1);
    *chanp=channel;
  }
  song->srcp=song->startp;
  return 0;
}

/* Update.
 */

int synth_song_update(struct synth *synth,struct synth_song *song) {
  for (;;) {
  
    // Finished?
    if (song->delay>0) return song->delay;
    if ((song->srcp>=song->srcc)||!song->src[song->srcp]) {
      if (!song->repeat) return 0;
      // Looping. Force a tiny delay, just in case the song is invalid and has no delays of its own.
      song->srcp=song->loopp;
      song->delay=1;
      song->playhead_ms_next=song->playhead_ms;
      return 1;
    }
    
    uint8_t lead=song->src[song->srcp++];
    // 0x00 is EOF, that's handled above.
  
    // Delay.
    if (!(lead&0x80)) {
      if ((song->delay=lroundf(lead*song->frames_per_ms))<1) song->delay=1;
      song->playhead_ms_next=song->playhead_ms+lead;
      return song->delay;
    }
  
    // Note.
    if ((lead&0xf0)==0x80) {
      if (song->srcp>song->srcc-2) return -1;
      uint8_t a=song->src[song->srcp++];
      uint8_t b=song->src[song->srcp++];
      uint8_t chid=a>>5;
      uint8_t velocity=(lead&0x0f)<<3;
      velocity|=velocity>>4;
      uint8_t noteid=((a&0x1f)<<2)|(b>>6);
      int dur=lroundf(((b&0x3f)<<5)*song->frames_per_ms);
      if (dur<0) dur=0;
      synth_event(synth,chid,MIDI_OPCODE_NOTE_ONCE,noteid,velocity,dur);
      continue;
    }
  
    // Fire-and-forget.
    if ((lead&0xf0)==0x90) {
      if (song->srcp>song->srcc-1) return -1;
      uint8_t a=song->src[song->srcp++];
      uint8_t chid=((lead&0x03)<<1)|(a>>7);
      uint8_t noteid=(a&0x7f);
      uint8_t velocity=(lead&0x0c)<<2;
      velocity|=velocity>>2;
      velocity|=velocity>>4;
      synth_event(synth,chid,MIDI_OPCODE_NOTE_ONCE,noteid,velocity,0);
      continue;
    }
  
    // Wheel.
    if ((lead&0xf8)==0xa0) {
      if (song->srcp>song->srcc-1) return -1;
      uint8_t v=song->src[song->srcp++];
      uint8_t chid=lead&0x07;
      synth_event(synth,chid,MIDI_OPCODE_WHEEL,v<<6,v>>1,0);
      continue;
    }
  
    // Everything else is reserved. (10101xxx, 1011xxxx, 11xxxxxx)
    return -1;
  }
}

/* Advance time.
 */

void synth_song_advance(struct synth_song *song,int framec) {
  if (framec<1) return;
  song->playhead_ms+=framec/song->frames_per_ms;
  if ((song->delay-=framec)<=0) {
    song->delay=0;
    if (song->playhead_ms_next) {
      song->playhead_ms=song->playhead_ms_next;
      song->playhead_ms_next=0;
    }
  }
}
