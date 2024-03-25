/* synth_env.h
 * 5-point linear envelopes.
 * In general we assume that sustain time is known at construction.
 * You do have the option of setting it unreasonably high, then you can release at any time.
 */
 
#ifndef SYNTH_ENV_H
#define SYNTH_ENV_H

struct synth_env {
  float v;
  float dv;
  int ttl;
  int stage; // (0,1,2,3,4) = (attack,decay,sustain,release,finished)
  float iniv,atkv,decv,rlsv; // (decv) is the value at two points
  int atkt,dect,sust,rlst;
};

struct synth_env_config {
  int atktlo,atkthi;
  int dectlo,decthi;
  int sustlo,susthi; // usually overridden
  int rlstlo,rlsthi;
  float inivlo,inivhi; // usually zero
  float atkvlo,atkvhi;
  float decvlo,decvhi;
  float rlsvlo,rlsvhi; // usually zero
};

/* Initialize a level envelope from just 8 bits of configuration:
 *   0xc0 Decay relative to attack:
 *         0x00 IMPULSE: Do not sustain.
 *         0x40 PLUCK: Heavy loss after attack.
 *         0x80 TONE: Attack noticeably louder than sustain.
 *         0xc0 BOW: No appreciable attack.
 *   0x38 Attack time: 0x00=Fast .. 0x38=Slow
 *   0x07 Release time: 0x00=Short .. 0x07=Long
 */
void synth_env_config_init_tiny(struct synth *synth,struct synth_env_config *config,uint8_t src);

/* Initialize a parameter envelope with timings from some other, and 4 values in 0..1 (16 levels each).
 * (ref) is usually the level envelope associated with this instrument, and (config) is some other voicing parameter.
 * 0xf000 is the initial value, and 0x000f the final.
 * At lower velocities, parameters shift toward the average.
 */
void synth_env_config_init_parameter(struct synth_env_config *config,const struct synth_env_config *ref,uint16_t src);

/* Multiply all values.
 */
void synth_env_config_gain(struct synth_env_config *config,float gain);

/* Prepare an envelope runner.
 * (velocity) in 0..127.
 * (dur) in frames.
 */
void synth_env_init(struct synth_env *env,const struct synth_env_config *config,uint8_t velocity,int dur);

/* Multiply all values.
 * Don't do this after you've started running.
 */
void synth_env_gain(struct synth_env *env,float gain);

/* Drop sustain time to 1, or arrange to terminate it immediately if already sustaining.
 * Safe no matter what stage we're in.
 */
void synth_env_release(struct synth_env *env);

// Should only be called by synth_env_update, below.
void synth_env_advance(struct synth_env *env);

static inline int synth_env_is_finished(const struct synth_env *env) {
  return (env->stage>=4);
}

static inline float synth_env_update(struct synth_env *env) {
  if (!env->ttl) synth_env_advance(env);
  env->ttl--;
  env->v+=env->dv;
  return env->v;
}

#endif
