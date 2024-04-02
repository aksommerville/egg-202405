/* synth_channel.h
 * Configuration space and routing for one MIDI channel.
 * These get instantiated and deleted on the fly as songs begin and end.
 */
 
#ifndef SYNTH_CHANNEL_H
#define SYNTH_CHANNEL_H

#include "synth_env.h"

#define SYNTH_CHANNEL_MODE_DRUM  1 /* Note On => Play PCM */
#define SYNTH_CHANNEL_MODE_BLIP  2 /* Square wave and no envelope, the simplest synthesizer. */
#define SYNTH_CHANNEL_MODE_WAVE  3 /* Wavetable and envelope, the simplest synthesizer one might actually use. */
#define SYNTH_CHANNEL_MODE_ROCK  4 /* Envelope-driven mix from sine to custom wave. Gets some "whowmp" on the cheap. */
#define SYNTH_CHANNEL_MODE_FMREL 5 /* FM relative to note frequency. This is usually the mode you want. */
#define SYNTH_CHANNEL_MODE_FMABS 6 /* FM with absolute-frequency modulator. */
#define SYNTH_CHANNEL_MODE_SUB   7 /* IIR bandpass of white noise. */
#define SYNTH_CHANNEL_MODE_FX    8 /* Shared LFO, FM voices, post-processing. The cadillac of synths. */
#define SYNTH_CHANNEL_MODE_ALIAS 9 /* Use some other program and issue a warning. */

extern const struct synth_builtin {
  uint8_t mode;
  union {
    // Nothing for DRUM: Default drum kits (pid 0x80..0xff) do not use builtin.
    // Nothing for BLIP: There's actually nothing to configure.
    struct {
      uint8_t wave[8]; // Coefficients 0..255, indexed by harmonic.
      uint8_t level; // Envelope, tiny form.
    } wave;
    struct {
      uint8_t wave[8]; // Coefficients 0..255, indexed by harmonic.
      uint16_t mix; // u4,4,4,4: Mix envelope, packed form.
      uint8_t level; // Envelope, tiny form.
    } rock;
    struct {
      uint8_t rate; // u4.4
      uint8_t scale; // u4.4, maximum range
      uint16_t range; // u4,4,4,4: Envelope.
      uint8_t level; // Envelope, tiny form.
    } fmrel;
    struct {
      uint16_t rate; // u8.8, hz
      uint8_t scale; // u4.4, maximum range
      uint16_t range; // u4,4,4,4: Envelope.
      uint8_t level; // Envelope, tiny form.
    } fmabs;
    struct {
      uint16_t width1; // hz, first pass
      uint16_t width2; // hz, second pass, zero for one pass
      uint8_t gain;
      uint8_t level; // Envelope, tiny form.
    } sub;
    struct {
      uint16_t rangeenv; // u4,4,4,4
      uint8_t rangelfo; // u4.4, beats
      uint8_t rangelfo_depth; // u4.4
      uint8_t rate; // u4.4
      uint8_t scale; // u4.4, fm range
      uint8_t level; // Envelope, tiny form.
      uint8_t detune_rate; // u4.4, beats
      uint8_t detune_depth;
      uint8_t overdrive;
      uint8_t delay_rate; // u4.4, beats
      uint8_t delay_depth;
    } fx;
    uint8_t alias;
  };
} synth_builtin[0x80];

struct synth_channel {
  uint8_t chid;
  int pid;
  uint16_t wheel;
  uint16_t wheel_range; // cents
  float bend; // multiplier
  float master; // 0..1, constant
  float trim; // 0..1, default 0.5, addressable via Control Change
  float pan; // -1..1
  int mode;
  int drumbase;
  float *wave;
  struct synth_env_config level;
  struct synth_env_config param0;
  float fmrate;
  uint32_t fmarate;
  float sv[8]; // Mode-specific scalar parameters.
};

void synth_channel_del(struct synth_channel *channel);

struct synth_channel *synth_channel_new(struct synth *synth,uint8_t chid,int pid);

void synth_channel_note_once(struct synth *synth,struct synth_channel *channel,uint8_t noteid,uint8_t velocity,int dur);
void synth_channel_note_on(struct synth *synth,struct synth_channel *channel,uint8_t noteid,uint8_t velocity);
void synth_channel_note_off(struct synth *synth,struct synth_channel *channel,uint8_t noteid,uint8_t velocity);
void synth_channel_control(struct synth *synth,struct synth_channel *channel,uint8_t k,uint8_t v);

/* 0..0x4000, default 0x2000.
 */
void synth_channel_wheel(struct synth *synth,struct synth_channel *channel,uint16_t v);

#endif
