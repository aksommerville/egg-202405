#include "synth_internal.h"

/* Delete.
 */
 
void synth_channel_del(struct synth_channel *channel) {
  if (!channel) return;
  if (channel->wave) free(channel->wave);
  free(channel);
}

/* Initialize with builtin.
 */
 
static int synth_channel_init_builtin(struct synth *synth,struct synth_channel *channel,const struct synth_builtin *builtin) {
  fprintf(stderr,"%s %p\n",__func__,builtin);
  switch (channel->mode=builtin->mode) {
    case SYNTH_CHANNEL_MODE_DRUM: return -1; // Not valid for builtins.
    case SYNTH_CHANNEL_MODE_BLIP: break; // OK, done!
    
    case SYNTH_CHANNEL_MODE_WAVE: {
        if (!(channel->wave=synth_wave_new_harmonics(synth,builtin->wave.wave,sizeof(builtin->wave.wave)))) return -1;
        synth_env_config_init_tiny(synth,&channel->level,builtin->wave.level);
      } break;
      
    case SYNTH_CHANNEL_MODE_ROCK: {
        if (!(channel->wave=synth_wave_new_harmonics(synth,builtin->rock.wave,sizeof(builtin->rock.wave)))) return -1;
        synth_env_config_init_tiny(synth,&channel->level,builtin->rock.level);
        synth_env_config_init_parameter(&channel->param0,&channel->level,builtin->rock.mix);
      } break;
      
    case SYNTH_CHANNEL_MODE_FMREL: {
        synth_env_config_init_tiny(synth,&channel->level,builtin->fmrel.level);
        synth_env_config_init_parameter(&channel->param0,&channel->level,builtin->fmrel.range);
        synth_env_config_gain(&channel->param0,builtin->fmrel.scale/16.0f);
        channel->fmrate=builtin->fmrel.rate/16.0f;
      } break;
      
    case SYNTH_CHANNEL_MODE_FMABS: {
        synth_env_config_init_tiny(synth,&channel->level,builtin->fmabs.level);
        synth_env_config_init_parameter(&channel->param0,&channel->level,builtin->fmabs.range);
        synth_env_config_gain(&channel->param0,builtin->fmabs.scale/16.0f);
        float nrate=builtin->fmabs.rate/(256.0f*(float)synth->rate);
        channel->fmarate=(uint32_t)(nrate*4294967296.0f);
      } break;
      
    case SYNTH_CHANNEL_MODE_SUB: {
        synth_env_config_init_tiny(synth,&channel->level,builtin->sub.level);
        channel->sv[0]=(float)builtin->sub.width1/(float)synth->rate;
        channel->sv[1]=(float)builtin->sub.width2/(float)synth->rate;
        channel->sv[2]=(float)builtin->sub.gain;
        fprintf(stderr,"sub: %f %f %f\n",channel->sv[0],channel->sv[1],channel->sv[2]);
      } break;
      
    /*struct {
      uint8_t rangelfo; // u4.4, beats
      uint8_t rate; // u4.4
      uint8_t scale; // u4.4, fm range
      uint8_t tremolo_rate; // u4.4, beats
      uint8_t tremolo_depth;
      uint8_t detune_rate; // u4.4, beats
      uint8_t detune_depth;
      uint8_t overdrive;
      uint8_t delay_rate; // u4.4, beats
      uint8_t delay_depth;
      //TODO These probably need "depth" parameters in addition to "rate".
      //TODO We are going to need tempo from the song. Can we store it as u16 ms?
    } fx; TODO */
    case SYNTH_CHANNEL_MODE_FX: {
        synth_env_config_init_tiny(synth,&channel->level,builtin->fx.level);
        return -1;//TODO
      } break;
      
    default: {
        fprintf(stderr,"%s:%d: Undefined channel mode %d for pid %d\n",__FILE__,__LINE__,builtin->mode,channel->pid);
        return -1;
      }
  }
  return 0;
}

/* Initialize as drum kit.
 */
 
static int synth_channel_init_drums(struct synth *synth,struct synth_channel *channel,int soundid0) {
  fprintf(stderr,"%s %d\n",__func__,soundid0);
  channel->mode=SYNTH_CHANNEL_MODE_DRUM;
  channel->drumbase=soundid0;
  return 0;
}

/* New.
 */

struct synth_channel *synth_channel_new(struct synth *synth,uint8_t chid,int pid) {
  if ((pid<0)||(pid>=0x100)) return 0;
  struct synth_channel *channel=calloc(1,sizeof(struct synth_channel));
  if (!channel) return 0;
  channel->chid=chid;
  channel->pid=pid;
  channel->wheel=0x2000;
  channel->wheel_range=200;
  channel->bend=1.0f;
  channel->master=0.25f;
  channel->trim=0.5f;
  channel->pan=0.0f;
    
  // pid 0..127 are General MIDI, defined in synth_builtin.
  if (pid<0x80) {
    if (synth_channel_init_builtin(synth,channel,synth_builtin+pid)<0) {
      synth_channel_del(channel);
      return 0;
    }
    
  // pid 128..255 are drum kits, each referring to 128 different sound effects.
  } else if (pid<0x100) {
    if (synth_channel_init_drums(synth,channel,(pid-0x80)*0x80)<0) {
      synth_channel_del(channel);
      return 0;
    }
    
  // pid above 255 are not defined.
  } else {
    synth_channel_del(channel);
    return 0;
  }
  
  return channel;
}

/* Note On.
 */
 
void synth_channel_note_on(struct synth *synth,struct synth_channel *channel,uint8_t noteid,uint8_t velocity) {
  switch (channel->mode) {
  
    case SYNTH_CHANNEL_MODE_DRUM: {
        int soundid=channel->drumbase+noteid;
        float trim=0.200f+(channel->trim*velocity)/100.0f;
        synth_play_sound(synth,0,soundid,trim,channel->pan);
      } break;
      
    case SYNTH_CHANNEL_MODE_FX: {
        //TODO Dispatch to proc
      } break;
      
    // Everything else starts a voice.
    default: {
        struct synth_voice *voice=synth_voice_new(synth);
        synth_voice_begin(synth,voice,channel,noteid,velocity,INT_MAX);
      }
  }
}

/* Note Off.
 */
 
void synth_channel_note_off(struct synth *synth,struct synth_channel *channel,uint8_t noteid,uint8_t velocity) {
  struct synth_voice *voice=synth_find_voice_by_chid_noteid(synth,channel->chid,noteid);
  if (!voice) return;
  synth_voice_release(synth,voice);
}

/* Note Once.
 */

void synth_channel_note_once(struct synth *synth,struct synth_channel *channel,uint8_t noteid,uint8_t velocity,int dur) {
  switch (channel->mode) {
  
    case SYNTH_CHANNEL_MODE_DRUM: synth_channel_note_on(synth,channel,noteid,velocity); return;
    
    case SYNTH_CHANNEL_MODE_FX: {
        //TODO Dispatch to proc
      } break;
      
    default: {
        struct synth_voice *voice=synth_voice_new(synth);
        synth_voice_begin(synth,voice,channel,noteid,velocity,dur);
      }
  }
}

/* Control Change.
 * Mind that only VOLUME_MSB and PAN_MSB are available to Egg songs.
 * All other keys are available on the API bus.
 */
 
void synth_channel_control(struct synth *synth,struct synth_channel *channel,uint8_t k,uint8_t v) {
  switch (k) {
    case MIDI_CONTROL_VOLUME_MSB: {
        channel->trim=v/127.0f;
      } break;
    case MIDI_CONTROL_PAN_MSB: {
        channel->pan=v/64.0f-1.0f;
      } break;
  }
}

/* Pitch Wheel.
 */
 
void synth_channel_wheel(struct synth *synth,struct synth_channel *channel,uint16_t v) {
  if (v==channel->wheel) return;
  channel->wheel=v;
  switch (channel->mode) {
  
    case SYNTH_CHANNEL_MODE_DRUM: break;
  
    case SYNTH_CHANNEL_MODE_FX: {
        //TODO Dispatch to proc
      } break;
      
    default: {
        channel->bend=powf(2.0f,((channel->wheel-0x2000)*channel->wheel_range)/(8192.0f*1200.0f));
        struct synth_voice *voice=synth->voicev;
        int i=synth->voicec;
        for (;i-->0;voice++) {
          if (synth_voice_is_defunct(voice)) continue;
          if (!synth_voice_is_channel(voice,channel->chid)) continue;
          voice->dp=(uint32_t)((float)voice->dp0*channel->bend);
        }
      } break;
  }
}
