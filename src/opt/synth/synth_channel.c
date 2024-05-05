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
  if (builtin->mode==SYNTH_CHANNEL_MODE_ALIAS) {
    fprintf(stderr,
      "WARNING: Program 0x%02x not defined, using 0x%02x instead. This song might sound different in the future.\n",
      channel->pid,builtin->alias
    );
    builtin=synth_builtin+builtin->alias;
  }
  switch (channel->mode=builtin->mode) {
    case 0: break;
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
      } break;
      
    case SYNTH_CHANNEL_MODE_FX: {
        synth_env_config_init_tiny(synth,&channel->level,builtin->fx.level);
        struct synth_proc *proc=synth_proc_new(synth);
        synth_proc_init(synth,proc);
        proc->chid=channel->chid;
        proc->origin=SYNTH_ORIGIN_SONG;//TODO
        if (synth_proc_fx_init(synth,proc,channel,builtin)<0) {
          proc->update=0;
          return -1;
        }
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
  //fprintf(stderr,"%s soundid=%d chid=%d\n",__func__,soundid0,channel->chid);
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
    const struct synth_builtin *builtin=synth_builtin+pid;
    if (!pid&&synth->override_pid_0.mode) builtin=&synth->override_pid_0;
    if (synth_channel_init_builtin(synth,channel,builtin)<0) {
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
        float trim=0.200f+(channel->trim*channel->master*velocity)/150.0f;
        synth_play_sound(synth,0,soundid,trim,channel->pan);
      } break;
      
    case SYNTH_CHANNEL_MODE_FX: {
        struct synth_proc *proc=synth_find_proc_by_chid(synth,channel->chid);
        if (!proc||!proc->note_on) return;
        proc->note_on(synth,proc,noteid,velocity);
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
  if (channel->mode==SYNTH_CHANNEL_MODE_FX) {
    struct synth_proc *proc=synth_find_proc_by_chid(synth,channel->chid);
    if (!proc||!proc->note_off) return;
    proc->note_off(synth,proc,noteid,velocity);
  } else {
    struct synth_voice *voice=synth_find_voice_by_chid_noteid(synth,channel->chid,noteid);
    if (!voice) return;
    synth_voice_release(synth,voice);
  }
}

/* Note Once.
 */

void synth_channel_note_once(struct synth *synth,struct synth_channel *channel,uint8_t noteid,uint8_t velocity,int dur) {
  switch (channel->mode) {
  
    case SYNTH_CHANNEL_MODE_DRUM: synth_channel_note_on(synth,channel,noteid,velocity); return;
    
    case SYNTH_CHANNEL_MODE_FX: {
        struct synth_proc *proc=synth_find_proc_by_chid(synth,channel->chid);
        if (!proc) return;
        if (proc->note_once) {
          proc->note_once(synth,proc,noteid,velocity,dur);
        } else if (proc->note_on) {
          proc->note_on(synth,proc,noteid,velocity);
          if (proc->note_on) proc->note_off(synth,proc,noteid,velocity);
        }
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
  switch (channel->mode) {
    case SYNTH_CHANNEL_MODE_FX: {
        struct synth_proc *proc=synth_find_proc_by_chid(synth,channel->chid);
        if (!proc||!proc->control) return;
        proc->control(synth,proc,k,v);
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
        struct synth_proc *proc=synth_find_proc_by_chid(synth,channel->chid);
        if (!proc||!proc->wheel) return;
        proc->wheel(synth,proc,v);
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
