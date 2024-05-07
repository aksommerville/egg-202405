#include "synth_internal.h"

/* Cleanup.
 */
 
void synth_voice_cleanup(struct synth_voice *voice) {
}

/* Begin.
 */
 
void synth_voice_begin(struct synth *synth,struct synth_voice *voice,struct synth_channel *channel,uint8_t noteid,uint8_t velocity,int dur) {
  if (noteid>=0x80) return;
  switch (voice->mode=channel->mode) {
  
    case SYNTH_CHANNEL_MODE_BLIP: {
        voice->chid=channel->chid;
        voice->noteid=noteid;
        voice->origin=SYNTH_ORIGIN_SONG;//TODO distinguish SONG from USER... how?
        voice->p=0;
        voice->dp0=synth->ifreqv[noteid];
        voice->dp=(uint32_t)((float)voice->dp0*channel->bend);
        voice->ttl=dur;
        int mindur=synth->rate>>4;
        if (voice->ttl<mindur) voice->ttl=mindur;
        voice->bliplevel=(channel->trim*(velocity+10))/1000.0f; // denominator should be about 140, but square waves are really loud
      } break;
    
    case SYNTH_CHANNEL_MODE_WAVE: {
        voice->chid=channel->chid;
        voice->noteid=noteid;
        voice->origin=SYNTH_ORIGIN_SONG;
        voice->p=0;
        voice->dp0=synth->ifreqv[noteid];
        voice->dp=(uint32_t)((float)voice->dp0*channel->bend);
        synth_env_init(&voice->level,&channel->level,velocity,dur);
        synth_env_gain(&voice->level,channel->master*channel->trim);
        voice->wave=channel->wave?channel->wave:synth->sine;
      } break;
      
    case SYNTH_CHANNEL_MODE_ROCK: {
        voice->chid=channel->chid;
        voice->noteid=noteid;
        voice->origin=SYNTH_ORIGIN_SONG;
        voice->p=0;
        voice->dp0=synth->ifreqv[noteid];
        voice->dp=(uint32_t)((float)voice->dp0*channel->bend);
        synth_env_init(&voice->level,&channel->level,velocity,dur);
        synth_env_gain(&voice->level,channel->master*channel->trim);
        synth_env_init(&voice->param0,&channel->param0,velocity,dur);
        voice->wave=channel->wave?channel->wave:synth->sine;
      } break;
      
    case SYNTH_CHANNEL_MODE_FMREL: {
        voice->chid=channel->chid;
        voice->noteid=noteid;
        voice->origin=SYNTH_ORIGIN_SONG;
        voice->p=0;
        voice->dp0=synth->ifreqv[noteid];
        voice->dp=(uint32_t)((float)voice->dp0*channel->bend);
        synth_env_init(&voice->level,&channel->level,velocity,dur);
        synth_env_gain(&voice->level,channel->master*channel->trim);
        synth_env_init(&voice->param0,&channel->param0,velocity,dur);
        voice->modp=0;
        voice->moddp=(uint32_t)((float)voice->dp0*channel->fmrate);
      } break;
      
    case SYNTH_CHANNEL_MODE_FMABS: {
        voice->chid=channel->chid;
        voice->noteid=noteid;
        voice->origin=SYNTH_ORIGIN_SONG;
        voice->p=0;
        voice->dp0=synth->ifreqv[noteid];
        voice->dp=(uint32_t)((float)voice->dp0*channel->bend);
        synth_env_init(&voice->level,&channel->level,velocity,dur);
        synth_env_gain(&voice->level,channel->master*channel->trim);
        synth_env_init(&voice->param0,&channel->param0,velocity,dur);
        voice->modp=0;
        voice->moddp=channel->fmarate;
      } break;
      
    case SYNTH_CHANNEL_MODE_SUB: {
        voice->chid=channel->chid;
        voice->noteid=noteid;
        voice->origin=SYNTH_ORIGIN_SONG;
        synth_env_init(&voice->level,&channel->level,velocity,dur);
        synth_env_gain(&voice->level,channel->master*channel->trim*channel->sv[2]);
        float freq=synth->ffreqv[noteid];
        synth_filter_init_bandpass(&voice->filter1,freq,channel->sv[0]);
        synth_filter_init_bandpass(&voice->filter2,freq,channel->sv[1]);
      } break;
    
    // DRUM uses playback; FX uses proc.
  }
}

/* Release.
 */

void synth_voice_release(struct synth *synth,struct synth_voice *voice) {
  voice->noteid=0xff;
  switch (voice->mode) {
  
    case SYNTH_CHANNEL_MODE_BLIP: {
        voice->ttl=0;
      } break;
    
    case SYNTH_CHANNEL_MODE_WAVE:
    case SYNTH_CHANNEL_MODE_ROCK:
    case SYNTH_CHANNEL_MODE_FMREL:
    case SYNTH_CHANNEL_MODE_FMABS: 
    case SYNTH_CHANNEL_MODE_SUB: {
        synth_env_release(&voice->level);
      } break;
  }
}

/* Update.
 */

void synth_voice_update(float *v,int c,struct synth *synth,struct synth_voice *voice) {
  switch (voice->mode) {
  
    case SYNTH_CHANNEL_MODE_BLIP: {
        for (;c-->0;v++) {
          if (voice->ttl--<=0) {
            voice->ttl=0;
            voice->origin=0;
            return;
          }
          if (voice->p&0x80000000) (*v)+=voice->bliplevel;
          else (*v)-=voice->bliplevel;
          voice->p+=voice->dp;
        }
      } break;
    
    case SYNTH_CHANNEL_MODE_WAVE: {
        for (;c-->0;v++) {
          (*v)+=voice->wave[voice->p>>SYNTH_WAVE_SHIFT]*synth_env_update(&voice->level);
          voice->p+=voice->dp;
        }
        if (synth_env_is_finished(&voice->level)) {
          voice->origin=0;
        }
      } break;
      
    case SYNTH_CHANNEL_MODE_ROCK: {
        for (;c-->0;v++) {
          float a=voice->wave[voice->p>>SYNTH_WAVE_SHIFT];
          float b=synth->sine[voice->p>>SYNTH_WAVE_SHIFT];
          float mix=synth_env_update(&voice->param0);
          float sample=a*mix+b*(1.0f-mix);
          (*v)+=sample*synth_env_update(&voice->level);
          voice->p+=voice->dp;
        }
        if (synth_env_is_finished(&voice->level)) {
          voice->origin=0;
        }
      } break;
      
    case SYNTH_CHANNEL_MODE_FMREL:
    case SYNTH_CHANNEL_MODE_FMABS: {
        for (;c-->0;v++) {
          (*v)+=synth->sine[voice->p>>SYNTH_WAVE_SHIFT]*synth_env_update(&voice->level);
          float mod=synth->sine[voice->modp>>SYNTH_WAVE_SHIFT];
          voice->modp+=voice->moddp;
          mod*=synth_env_update(&voice->param0);
          voice->p+=voice->dp+(int32_t)((float)voice->dp*mod);
        }
        if (synth_env_is_finished(&voice->level)) {
          voice->origin=0;
        }
      } break;
      
    case SYNTH_CHANNEL_MODE_SUB: {
        for (;c-->0;v++) {
          float sample=(rand()&0xffff)/32768.0f-1.0f;
          sample=synth_filter_iir_update(&voice->filter1,sample);
          sample=synth_filter_iir_update(&voice->filter2,sample);
          sample*=synth_env_update(&voice->level);
          if (sample<-0.5f) sample=-0.5f;
          else if (sample>0.5f) sample=0.5f;
          (*v)+=sample;
        }
        if (synth_env_is_finished(&voice->level)) {
          voice->origin=0;
        }
      } break;
      
  }
}
