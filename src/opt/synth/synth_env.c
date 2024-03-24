#include "synth_internal.h"

/* Init level config.
 */
 
void synth_env_config_init_level(
  struct synth *synth,struct synth_env_config *config,
  uint8_t atkt, // attack time, in vague units
  uint8_t rlst, // release time, in vague units
  uint8_t peak, // attack level
  uint8_t drop, // decay+sustain level relative to attack
  uint8_t velsens // constant..sensitive to velocity
) {
  config->inivlo=config->inivhi=config->rlsvlo=config->rlsvhi=0.0f;
  config->sustlo=config->susthi=1;
  
  config->atkthi=5+atkt; // 5..260 ms
  config->rlsthi=40+(rlst<<2); // 40..1060 ms
  config->atkvhi=0.050f+peak/260.0f; // 0.05..oneish
  config->decvhi=(config->atkvhi*(5+drop))/260.0f;
  config->decthi=(config->atkthi*3)/2;
  
  if (velsens) {
    float leveladjust=(260-velsens)/260.0f;
    config->atkvlo=config->atkvhi*leveladjust;
    config->decvlo=config->decvhi*leveladjust;
    config->atkvlo=(config->atkvlo+config->decvlo)*0.5f; // draw attack and sustain levels closer relatively, too
    config->atktlo+=(config->atkthi*(0xff-velsens))>>8; // attack time increases, up to double
    if ((config->rlstlo=(int)(config->rlsthi*leveladjust))<40) config->rlstlo=40; // release time decreases
    config->dectlo=(config->atktlo*3)/2; // decay time stays pegged at 1.5 attack time
  } else {
    config->atktlo=config->atkthi;
    config->rlstlo=config->rlsthi;
    config->atkvlo=config->atkvhi;
    config->decvlo=config->decvhi;
    config->susvlo=config->susvhi;
    config->dectlo=config->decthi;
  }
  
  config->susvhi=config->decvhi;
  config->susvlo=config->decvlo;
  
  // Convert times from ms to frames, and clamp at one.
  if ((config->atktlo=(config->atktlo*synth->rate)/1000)<1) config->atktlo=1;
  if ((config->atkthi=(config->atkthi*synth->rate)/1000)<1) config->atkthi=1;
  if ((config->dectlo=(config->dectlo*synth->rate)/1000)<1) config->dectlo=1;
  if ((config->decthi=(config->decthi*synth->rate)/1000)<1) config->decthi=1;
  if ((config->rlstlo=(config->rlstlo*synth->rate)/1000)<1) config->rlstlo=1;
  if ((config->rlsthi=(config->rlsthi*synth->rate)/1000)<1) config->rlsthi=1;
}

/* Init general parameter config.
 */
 
void synth_env_config_init_param(
  struct synth_env_config *config,
  const struct synth_env_config *reference,
  float iniv,float atkv,float decv,float susv,float rlsv,
  float variation
) {
  memcpy(config,reference,sizeof(struct synth_env_config)); // acquire all the times; they won't change
  config->inivhi=iniv;
  config->atkvhi=atkv;
  config->decvhi=decv;
  config->susvhi=susv;
  config->rlsvhi=rlsv;
  config->inivlo=iniv*variation;
  config->atkvlo=atkv*variation;
  config->decvlo=decv*variation;
  config->susvlo=susv*variation;
  config->rlsvlo=rlsv*variation;
}

/* Multiply all levels in a config.
 */
 
void synth_env_config_gain(struct synth_env_config *config,float gain) {
  config->inivlo*=gain;
  config->atkvlo*=gain;
  config->decvlo*=gain;
  config->susvlo*=gain;
  config->rlsvlo*=gain;
  config->inivhi*=gain;
  config->atkvhi*=gain;
  config->decvhi*=gain;
  config->susvhi*=gain;
  config->rlsvhi*=gain;
}

/* Init env runner.
 */

void synth_env_init(struct synth_env *env,const struct synth_env_config *config,uint8_t velocity) {
  if (velocity<=0x00) {
    env->iniv=config->inivlo;
    env->atkt=config->atktlo;
    env->atkv=config->atkvlo;
    env->dect=config->dectlo;
    env->decv=config->decvlo;
    env->sust=config->sustlo;
    env->susv=config->susvlo;
    env->rlst=config->rlstlo;
    env->rlsv=config->rlsvlo;
  } else if (velocity>=0x7f) {
    env->iniv=config->inivhi;
    env->atkt=config->atkthi;
    env->atkv=config->atkvhi;
    env->dect=config->decthi;
    env->decv=config->decvhi;
    env->sust=config->susthi;
    env->susv=config->susvhi;
    env->rlst=config->rlsthi;
    env->rlsv=config->rlsvhi;
  } else {
    uint8_t hiweight=0x80-velocity;
    float felocity=(float)velocity/127.0f;
    float fiweight=1.0f-felocity;
    env->iniv=(config->inivlo*felocity+config->inivhi*fiweight);
    env->atkv=(config->atkvlo*felocity+config->atkvhi*fiweight);
    env->decv=(config->decvlo*felocity+config->decvhi*fiweight);
    env->susv=(config->susvlo*felocity+config->susvhi*fiweight);
    env->rlsv=(config->rlsvlo*felocity+config->rlsvhi*fiweight);
    env->atkt=(config->atktlo*velocity+config->atkthi*hiweight)>>7;
    env->dect=(config->dectlo*velocity+config->decthi*hiweight)>>7;
    env->sust=(config->sustlo*velocity+config->susthi*hiweight)>>7;
    env->rlst=(config->rlstlo*velocity+config->rlsthi*hiweight)>>7;
  }
  if (env->atkt<1) env->atkt=1;
  if (env->dect<1) env->dect=1;
  if (env->sust<1) env->sust=1;
  if (env->rlst<1) env->rlst=1;
  
  env->stage=0;
  env->v=env->iniv;
  env->ttl=env->atkt;
  env->dv=(env->atkv-env->v)/(float)env->ttl;
}

/* Apply gain to runner.
 */
 
void synth_env_gain(struct synth_env *env,float gain) {
  env->iniv*=gain;
  env->atkv*=gain;
  env->decv*=gain;
  env->susv*=gain;
  env->rlsv*=gain;
  
  env->stage=0;
  env->v=env->iniv;
  env->ttl=env->atkt;
  env->dv=(env->atkv-env->v)/(float)env->ttl;
}

/* Release.
 */

void synth_env_release(struct synth_env *env) {
  if (env->stage<2) {
    env->sust=1;
  } else if (env->stage==2) {
    // If (devc!=susv) there will be a pop. But those should be equal.
    env->ttl=1;
  } else {
    // Already releasing or released, don't change anything.
  }
}

/* Advance.
 */

void synth_env_advance(struct synth_env *env) {
  env->stage++;
  float from=env->v;
  switch (env->stage) {
    case 1: { // enter decay
        env->ttl=env->dect;
        env->v=env->atkv;
        env->dv=(env->decv-env->v)/(float)env->ttl;
      } break;
    case 2: { // enter sustain
        env->ttl=env->sust;
        env->v=env->decv;
        env->dv=(env->susv-env->v)/(float)env->ttl;
      } break;
    case 3: { // enter release
        env->ttl=env->rlst;
        env->v=env->susv;
        env->dv=(env->rlsv-env->v)/(float)env->ttl;
      } break;
    default: { // finish
        env->stage=4;
        env->ttl=INT_MAX;
        env->dv=0.0f;
      } break;
  }
  float travel=env->v-from;
  if ((travel<-0.05f)||(travel>0.05f)) {
    fprintf(stderr,"!!! travel %f entering stage %d (%f => %f)\n",travel,env->stage,from,env->v);
  }
}
