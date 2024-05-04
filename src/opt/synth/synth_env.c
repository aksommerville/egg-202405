#include "synth_internal.h"

/* Init config, tiny format.
 */
 
void synth_env_config_init_tiny(struct synth *synth,struct synth_env_config *config,uint8_t src) {

  // Attack timing is orthogonal.
  // Phrase initially in ms.
  switch (src&0x38) {
    case 0x00: config->atkthi=5; break;
    case 0x08: config->atkthi=8; break;
    case 0x10: config->atkthi=12; break;
    case 0x18: config->atkthi=18; break;
    case 0x20: config->atkthi=30; break;
    case 0x28: config->atkthi=45; break;
    case 0x30: config->atkthi=60; break;
    case 0x38: config->atkthi=80; break;
  }
  
  // Same with release timing.
  switch (src&0x07) {
    case 0x00: config->rlsthi=40; break;
    case 0x01: config->rlsthi=60; break;
    case 0x02: config->rlsthi=100; break;
    case 0x03: config->rlsthi=200; break;
    case 0x04: config->rlsthi=300; break;
    case 0x05: config->rlsthi=400; break;
    case 0x06: config->rlsthi=800; break;
    case 0x07: config->rlsthi=1200; break;
  }

  // Decay level and sustain time placeholder.
  // Attack level is 1 for IMPULSE and PLUCK, and lower for the more sustainy ones.
  switch (src&0xc0) {
    case 0x00: { // IMPULSE
        config->susthi=0;
        config->decvhi=0.250f;
        config->atkvhi=1.0f;
      } break;
    case 0x40: { // PLUCK
        config->susthi=1;
        config->decvhi=0.200f;
        config->atkvhi=1.0f;
      } break;
    case 0x80: { // TONE
        config->susthi=1;
        config->decvhi=0.400f;
        config->atkvhi=0.750f;
      } break;
    case 0xc0: { // BOW
        config->susthi=1;
        config->decvhi=0.400f;
        config->atkvhi=0.400f;
      } break;
  }
  
  // Decay time derives entirely from attack time.
  config->decthi=(config->atkthi*3)/2;
  
  // The remaining three levels are constant.
  config->inivhi=0.0f;
  config->rlsvhi=0.0f;
  
  // At minimum velocity, all the values get smaller, except attack and decay time get larger.
  config->atktlo=config->atkthi<<1;
  config->dectlo=config->decthi<<1;
  config->sustlo=config->susthi;
  config->rlstlo=config->rlsthi>>1;
  config->inivlo=0.0f;
  config->atkvlo=config->atkvhi*0.333f;
  config->decvlo=config->decvhi*0.500f;
  config->rlsvlo=0.0f;
  
  // Finally, convert all times from ms to frames and ensure they're not zero.
  // (except sustain time, where zero must be preserved).
  if ((config->atktlo=(config->atktlo*synth->rate)/1000)<1) config->atktlo=1;
  if ((config->atkthi=(config->atkthi*synth->rate)/1000)<1) config->atkthi=1;
  if ((config->dectlo=(config->dectlo*synth->rate)/1000)<1) config->dectlo=1;
  if ((config->decthi=(config->decthi*synth->rate)/1000)<1) config->decthi=1;
  if (config->susthi) {
    if ((config->sustlo=(config->sustlo*synth->rate)/1000)<1) config->sustlo=1;
    if ((config->susthi=(config->susthi*synth->rate)/1000)<1) config->susthi=1;
  }
  if ((config->rlstlo=(config->rlstlo*synth->rate)/1000)<1) config->rlstlo=1;
  if ((config->rlsthi=(config->rlsthi*synth->rate)/1000)<1) config->rlsthi=1;
}

/* Initialize parameter envelope config.
 */

void synth_env_config_init_parameter(struct synth_env_config *config,const struct synth_env_config *ref,uint16_t src) {
  memcpy(config,ref,sizeof(struct synth_env_config));
  config->inivhi=((src>>12)&15)/15.0f;
  config->atkvhi=((src>> 8)&15)/15.0f;
  config->decvhi=((src>> 4)&15)/15.0f;
  config->rlsvhi=((src    )&15)/15.0f;
  float avg=(config->inivhi+config->atkvhi+config->decvhi+config->rlsvhi)/4.0f;
  config->inivlo=(config->inivhi+avg)/2.0f;
  config->atkvlo=(config->atkvhi+avg)/2.0f;
  config->decvlo=(config->decvhi+avg)/2.0f;
  config->rlsvlo=(config->rlsvhi+avg)/2.0f;
}

/* Scale config.
 */

void synth_env_config_gain(struct synth_env_config *config,float gain) {
  config->inivlo*=gain;
  config->atkvlo*=gain;
  config->decvlo*=gain;
  config->rlsvlo*=gain;
  config->inivhi*=gain;
  config->atkvhi*=gain;
  config->decvhi*=gain;
  config->rlsvhi*=gain;
}

/* Reset runner.
 */
 
static void synth_env_reset(struct synth_env *env) {
  env->stage=0;
  env->ttl=env->atkt;
  env->v=env->iniv;
  env->dv=(env->atkv-env->v)/(float)env->ttl;
}

/* Initialize runner.
 */

void synth_env_init(struct synth_env *env,const struct synth_env_config *config,uint8_t velocity,int dur) {
  if (velocity<=0) {
    env->iniv=config->inivlo;
    env->atkv=config->atkvlo;
    env->decv=config->decvlo;
    env->rlsv=config->rlsvlo;
    env->atkt=config->atktlo;
    env->dect=config->dectlo;
    env->sust=config->sustlo;
    env->rlst=config->rlstlo;
  } else if (velocity>=0x7f) {
    env->iniv=config->inivhi;
    env->atkv=config->atkvhi;
    env->decv=config->decvhi;
    env->rlsv=config->rlsvhi;
    env->atkt=config->atkthi;
    env->dect=config->decthi;
    env->sust=config->susthi;
    env->rlst=config->rlsthi;
  } else {
    uint8_t bi=0x7f-velocity;
    env->atkt=(config->atkthi*velocity+config->atktlo*bi)>>7;
    env->dect=(config->decthi*velocity+config->dectlo*bi)>>7;
    env->sust=(config->susthi*velocity+config->sustlo*bi)>>7;
    env->rlst=(config->rlsthi*velocity+config->rlstlo*bi)>>7;
    float af=velocity/127.0f;
    float bf=1.0f-af;
    env->iniv=config->inivhi*af+config->inivlo*bf;
    env->atkv=config->atkvhi*af+config->atkvlo*bf;
    env->decv=config->decvhi*af+config->decvlo*bf;
    env->rlsv=config->rlsvhi*af+config->rlsvlo*bf;
  }
  if (env->sust&&(dur>0)) { // Nonzero sustain time means let the caller override.
    env->sust=dur;
  } else { // Otherwise a constant '1' (not zero, that might gum up the works somewhere).
    env->sust=1;
  }
  synth_env_reset(env);
}

/* Scale runner.
 */

void synth_env_gain(struct synth_env *env,float gain) {
  env->iniv*=gain;
  env->atkv*=gain;
  env->decv*=gain;
  env->rlsv*=gain;
  synth_env_reset(env);
}

/* Release.
 */

void synth_env_release(struct synth_env *env) {
  if (env->stage>=3) return; // release or finished; nothing to do.
  if (env->stage==2) { // currently sustaining
    env->ttl=0;
  } else {
    env->sust=1;
  }
}

/* Advance.
 */

void synth_env_advance(struct synth_env *env) {
  env->stage++;
  switch (env->stage) {
    case 1: { // decay
        env->v=env->atkv;
        env->ttl=env->dect;
        env->dv=(env->decv-env->v)/(float)env->ttl;
      } break;
    case 2: { // sustain
        env->v=env->decv;
        env->ttl=env->sust;
        env->dv=0.0f;
      } break;
    case 3: { // release
        env->v=env->decv;
        env->ttl=env->rlst;
        env->dv=(env->rlsv-env->v)/(float)env->ttl;
      } break;
    default: { // finished
        env->stage=4;
        env->v=env->rlsv;
        env->ttl=INT_MAX;
        env->dv=0.0f;
      }
  }
}
