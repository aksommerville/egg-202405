/* synth_channel.h
 * Event receiver.
 */
 
#ifndef SYNTH_CHANNEL_H
#define SYNTH_CHANNEL_H

/* Hard-coded configuration for a program.
 */
struct synth_program {
  uint8_t overdrive;
  uint8_t delay_rate; // u4.4
  uint8_t delay_depth;
  uint8_t mode;
  union {
    struct {
      uint8_t shape;
      uint8_t coefv[8]; // overrides (shape) if nonzero
      uint8_t env;
    } basic;
    struct {
      uint8_t rate; // u4.4
      uint8_t scale; // u4.4
      uint16_t range; // env
      uint8_t env;
    } fm;
    struct {
      uint8_t q;
      uint8_t env;
    } sub;
    uint8_t alias;
  };
};
#define SYNTH_PROGRAM_MODE_NOOP 0
#define SYNTH_PROGRAM_MODE_ALIAS 1
#define SYNTH_PROGRAM_MODE_BASIC 3
#define SYNTH_PROGRAM_MODE_FM 4
#define SYNTH_PROGRAM_MODE_SUB 5

extern const struct synth_program synth_programv[128];

struct synth_channel {
  struct synth *synth; // WEAK
  int chid;
  int pid;
  float trim;
  float pan;
  const struct synth_program *program;
};

void synth_channel_del(struct synth_channel *channel);

struct synth_channel *synth_channel_new(
  struct synth *synth,
  int pid,float trim,float pan
);

/* Remove everything we own from the signal graph, or arrange for them to wrap up soon.
 */
void synth_channel_finish(struct synth_channel *channel);

void synth_channel_event(struct synth_channel *channel,uint8_t opcode,uint8_t a,uint8_t b);

#endif
