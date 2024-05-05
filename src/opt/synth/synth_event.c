#include "synth_internal.h"

/* Override pid 0.
 */
 
void synth_override_pid_0(struct synth *synth,const struct synth_builtin *builtin) {
  if (builtin) {
    memcpy(&synth->override_pid_0,builtin,sizeof(struct synth_builtin));
  } else {
    memset(&synth->override_pid_0,0,sizeof(struct synth_builtin));
  }
  // Drop everything hard. Except song, pidv, and playbackv, no problem keep those.
  while (synth->voicec>0) {
    synth->voicec--;
    synth_voice_cleanup(synth->voicev+synth->voicec);
  }
  while (synth->procc>0) {
    synth->procc--;
    synth_proc_cleanup(synth->procv+synth->procc);
  }
  int chid=SYNTH_CHANNEL_COUNT; while (chid-->0) {
    if (synth->channelv[chid]) {
      synth_channel_del(synth->channelv[chid]);
      synth->channelv[chid]=0;
    }
  }
}

/* Get channel for chid, instantiating if needed.
 */
 
static struct synth_channel *synth_get_channel(struct synth *synth,uint8_t chid) {
  if (chid>=SYNTH_CHANNEL_COUNT) return 0;
  if (synth->channelv[chid]) return synth->channelv[chid];
  return synth->channelv[chid]=synth_channel_new(synth,chid,synth->pidv[chid]);
}

/* Note events: Forward to channel.
 * Off and On can only appear on the API bus, and Once typically only comes from the Song bus.
 * (API clients are welcome to send Once too).
 */
 
static void synth_event_note_off(struct synth *synth,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  struct synth_channel *channel=synth_get_channel(synth,chid);
  if (!channel) return;
  synth_channel_note_off(synth,channel,noteid,velocity);
}
 
static void synth_event_note_on(struct synth *synth,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  struct synth_channel *channel=synth_get_channel(synth,chid);
  if (!channel) return;
  synth_channel_note_on(synth,channel,noteid,velocity);
}
 
static void synth_event_note_once(struct synth *synth,uint8_t chid,uint8_t noteid,uint8_t velocity,int dur) {
  struct synth_channel *channel=synth_get_channel(synth,chid);
  if (!channel) return;
  synth_channel_note_once(synth,channel,noteid,velocity,dur);
}

/* Control Change.
 * Songs won't do this, only the API bus.
 */
 
static void synth_event_control(struct synth *synth,uint8_t chid,uint8_t k,uint8_t v) {
  //if (chid<SYNTH_SONG_CHANNEL_COUNT) return;
  if (chid>=SYNTH_CHANNEL_COUNT) return;
  switch (k) {
  
    // Bank changes take effect at the next Program Change. No impact immediately.
    case MIDI_CONTROL_BANK_MSB: synth->pidv[chid]=(synth->pidv[chid]&0xffe03fff)|(v<<14); return;
    case MIDI_CONTROL_BANK_LSB: synth->pidv[chid]=(synth->pidv[chid]&0xffffc07f)|(v<<7); return;
    
    // All other control changes, let the channel take a crack at it.
    // Actually not "All other"... Let's discard the LSB events, I know we're not going to use those.
    default: {
        if ((k>=0x20)&&(k<0x40)) return;
        struct synth_channel *channel=synth_get_channel(synth,chid);
        if (!channel) return;
        synth_channel_control(synth,channel,k,v);
      }
  }
}

/* Program Change.
 * Songs won't do this, only the API bus.
 * We don't load the program at this time.
 * Rather, we unload the old one and enter a state in which the next event will cause program's instantiation.
 */
 
static void synth_event_program(struct synth *synth,uint8_t chid,uint8_t pid) {
  //if (chid<SYNTH_SONG_CHANNEL_COUNT) return;
  if (chid>=SYNTH_CHANNEL_COUNT) return;
  synth->pidv[chid]=(synth->pidv[chid]&~0x7f)|pid;
  if (synth->channelv[chid]) {
    // Dropping voices cold when the program changes is antisocial and not normal for MIDI synthesizers.
    // We have to do it though, because the channel may own state referenced by its voices.
    // This will only happen on the API bus.
    synth_drop_voices_for_channel(synth,chid);
    synth_channel_del(synth->channelv[chid]);
    synth->channelv[chid]=0;
  }
}

/* Pitch Wheel.
 * Songs and API bus both send these.
 * (v) in 0..0x4000, center at 0x2000.
 */
 
static void synth_event_wheel(struct synth *synth,uint8_t chid,uint16_t v) {
  struct synth_channel *channel=synth_get_channel(synth,chid);
  if (!channel) return;
  synth_channel_wheel(synth,channel,v);
}

/* Reset.
 * Our API bus allows this to arrive with a (chid).
 * If sourced from MIDI, that should always be 0xff for "all".
 * Songs can't send it, so only allow reset of API-only channels.
 */
 
static void synth_event_reset(struct synth *synth,uint8_t chid) {
  if (chid<SYNTH_SONG_CHANNEL_COUNT) return;
  //TODO Should Reset clear (pidv)? I'm thinking no.
  if (chid<SYNTH_CHANNEL_COUNT) { // Reset one.
    synth_drop_voices_for_channel(synth,chid);
    synth_channel_del(synth->channelv[chid]);
    synth->channelv[chid]=0;
  } else { // Reset all API channels.
    //int i=SYNTH_SONG_CHANNEL_COUNT;
    int i=0; // Actually, reset all of them: We might be sourcing events from the MIDI bus, which would use song channels.
    for (;i<SYNTH_CHANNEL_COUNT;i++) {
      synth_drop_voices_for_channel(synth,i);
      synth_channel_del(synth->channelv[i]);
      synth->channelv[i]=0;
    }
  }
}

/* Dispatch event.
 */
 
void synth_event(struct synth *synth,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int dur) {
  //fprintf(stderr,"%s %02x %02x %02x %02x\n",__func__,chid,opcode,a,b);
  switch (opcode) {
    case MIDI_OPCODE_NOTE_OFF: synth_event_note_off(synth,chid,a,b); break;
    case MIDI_OPCODE_NOTE_ON: synth_event_note_on(synth,chid,a,b); break;
    case MIDI_OPCODE_NOTE_ONCE: synth_event_note_once(synth,chid,a,b,dur); break;
    case MIDI_OPCODE_CONTROL: synth_event_control(synth,chid,a,b); break;
    case MIDI_OPCODE_PROGRAM: synth_event_program(synth,chid,a); break;
    case MIDI_OPCODE_WHEEL: synth_event_wheel(synth,chid,a|(b<<7)); break;
    case MIDI_OPCODE_RESET: synth_event_reset(synth,chid); break;
  }
}
