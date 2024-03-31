/* synth_builtin.c
 * Definitions for the 128 General MIDI instruments.
 */
 
#include "synth_internal.h"

// Tiny envelope.
#define IMPULSE 0x00
#define PLUCK   0x40
#define TONE    0x80
#define BOW     0xc0
#define ATTACK(zero_thru_seven) (zero_thru_seven<<3)
#define RELEASE(zero_thru_seven) (zero_thru_seven)

#define BLIP(pid) [pid]={.mode=SYNTH_CHANNEL_MODE_BLIP},
#define WAVE(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_WAVE,.wave={__VA_ARGS__}},
#define ROCK(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_ROCK,.rock={__VA_ARGS__}},
#define FMREL(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_FMREL,.fmrel={__VA_ARGS__}},
#define FMABS(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_FMABS,.fmabs={__VA_ARGS__}},
#define SUB(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_SUB,.sub={__VA_ARGS__}},
#define FX(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_FX,.fx={__VA_ARGS__}},
#define ALIAS(pid,otherpid) [pid]={.mode=SYNTH_CHANNEL_MODE_ALIAS,.alias=otherpid},

const struct synth_builtin synth_builtin[0x80]={
  //WAVE(0x00,.wave={0})
  FX(0x00,
    .rangeenv=0x8fc4,
    .rangelfo=0x20,
    .rangelfo_depth=0x10,
    .rate=0x10,
    .scale=0x30,
    .level=PLUCK|ATTACK(1)|RELEASE(1),
    //.detune_rate=0x40,
    //.detune_depth=0x10,
    .overdrive=0xff,
    .delay_rate=0x08,
    .delay_depth=0x80,
  )
  BLIP(0x01)
  FX(0x51,
    .rangeenv=0x0f80,
    .rangelfo=0x10,
    .rangelfo_depth=0x08,
    .rate=0x08,
    .scale=0x30,
    .level=PLUCK|ATTACK(2)|RELEASE(3),
    .detune_rate=0x10,
    .detune_depth=0x08,
    .overdrive=0x00,
    .delay_rate=0x00,
    .delay_depth=0x00,
  )
  BLIP(0x02)
  BLIP(0x03)
  BLIP(0x04)
  BLIP(0x05)
  BLIP(0x06)
  BLIP(0x07)
  BLIP(0x08)
  BLIP(0x09)
  BLIP(0x0a)
  BLIP(0x0b)
  BLIP(0x0c)
  BLIP(0x0d)
  
  // eye-of-newt
  FX(0x0e,
    .rangeenv=0x0f80,
    .rangelfo=0x10,
    .rate=0x20,
    .scale=0x30,
    .level=PLUCK|ATTACK(1)|RELEASE(5),
    .detune_rate=0x10,
    .detune_depth=0x10,
    .overdrive=0x00,
    .delay_rate=0x20,
    .delay_depth=0x40,
  )
  ROCK(0x0f,
    .wave={0xff,0x00,0x55,0x00,0x33,0x00,0x11,0x00},
    .mix=0x0f80,
    .level=PLUCK|ATTACK(0)|RELEASE(7),
  )
  FMREL(0x10,
    .rate=0x08,
    .scale=0x30,
    .range=0x0f61,
    .level=TONE|ATTACK(2)|RELEASE(4),
  )
  SUB(0x11,
    .width1=40,
    .width2=20,
    .gain=30,
    .level=BOW|ATTACK(7)|RELEASE(6),
  )
  FMABS(0x12,
    .rate=0x0600,
    .scale=0x04,
    .range=0x8f20,
    .level=TONE|ATTACK(1)|RELEASE(3),
  )
  
  BLIP(0x13)
  BLIP(0x14)
  BLIP(0x15)
  BLIP(0x16)
  BLIP(0x17)
  BLIP(0x18)
  BLIP(0x19)
  BLIP(0x1a)
  BLIP(0x1b)
  BLIP(0x1c)
  BLIP(0x1d)
  BLIP(0x1e)
  BLIP(0x1f)
  BLIP(0x20)
  BLIP(0x21)
  BLIP(0x22)
  BLIP(0x23)
  BLIP(0x24)
  BLIP(0x25)
  BLIP(0x26)
  BLIP(0x27)
  BLIP(0x28)
  BLIP(0x29)
  BLIP(0x2a)
  BLIP(0x2b)
  FMREL(0x2c,
    .rate=0x08,
    .scale=0x20,
    .range=0x8f84,
    .level=TONE|ATTACK(1)|RELEASE(4),
  )
  WAVE(0x71,
    .wave={0xff,0x00,0x55,0x00,0x33,0x00,0x11,0x00},
    .level=PLUCK|ATTACK(0)|RELEASE(7),
  )
  ROCK(0x72,
    .wave={0xff,0x00,0x55,0x00,0x33,0x00,0x11,0x00},
    .mix=0x0f80,
    .level=PLUCK|ATTACK(0)|RELEASE(7),
  )
  FMREL(0x73,
    .rate=0x08,
    .scale=0x30,
    .range=0x0f61,
    .level=TONE|ATTACK(2)|RELEASE(4),
  )
  FMABS(0x74,
    .rate=0x0600,
    .scale=0x30,
    .range=0x8f20,
    .level=TONE|ATTACK(1)|RELEASE(3),
  )
  SUB(0x75,
    .width1=40,
    .width2=20,
    .gain=30,
    .level=BOW|ATTACK(7)|RELEASE(6),
  )
  FX(0x76,
    .rangeenv=0x0f80,
    .rangelfo=0x10,
    .rate=0x20,
    .scale=0x30,
    .level=PLUCK|ATTACK(1)|RELEASE(5),
    .detune_rate=0x10,
    .detune_depth=0x40,
    .overdrive=0x80,
    .delay_rate=0x20,/*spacelessComment*/
    .delay_depth=0x40,
  )
};
