#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#define SYNTH_ORIGIN_SONG 1
#define SYNTH_ORIGIN_USER 2

#include "opt/sfg/sfg.h"
#include "synth.h"
#include "synth_env.h"
#include "synth_filter.h"
#include "synth_delay.h"
#include "synth_cache.h"
#include "synth_song.h"
#include "synth_channel.h"
#include "synth_voice.h"
#include "synth_proc.h"
#include "synth_playback.h"
#include "opt/midi/midi.h"
#include "opt/romr/romr.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>

#define MIDI_OPCODE_NOTE_ONCE 0x98

/* Client can update any length at all, but we'll slice to no more than this before entering the signal graph.
 * So if we need temporary buffers anywhere, they have a definite upper limit.
 * In samples.
 */
#define SYNTH_BUFFER_LIMIT 1024

/* Where we use wave tables, they'll have a fixed power-of-two length.
 * You can walk them with a uint32_t position, and just right-shift it down to get the index.
 */
#define SYNTH_WAVE_SIZE_BITS 10
#define SYNTH_WAVE_SIZE_SAMPLES (1<<SYNTH_WAVE_SIZE_BITS)
#define SYNTH_WAVE_SHIFT (32-SYNTH_WAVE_SIZE_BITS)
#define SYNTH_HARMONICS_LIMIT ((SYNTH_WAVE_SIZE_SAMPLES>>1)-1) /* way more than one would actually provide in a sane universe */

/* MIDI allows 16 channels, and our songs are limited to 8.
 * So channels 8..15 are only reachable thru our API, songs can't touch them.
 * The low 8 channels get wiped when the song changes.
 */
#define SYNTH_CHANNEL_COUNT 16
#define SYNTH_SONG_CHANNEL_COUNT 8

/* Signal-generating objects live in fixed-size lists.
 * When a new one gets created, it might evict some older one.
 * Safe to raise or lower these limits arbitrarily.
 */
#define SYNTH_VOICE_LIMIT 32
#define SYNTH_PROC_LIMIT 16
#define SYNTH_PLAYBACK_LIMIT 16

struct synth {
  int rate;
  int chanc;
  int buffer_limit; // SYNTH_BUFFER_LIMIT, rounded down to a multiple of (chanc).
  float sine[SYNTH_WAVE_SIZE_SAMPLES];
  float ffreqv[0x80]; // Note frequencies in 0..1 (hopefully 0..1/2).
  uint32_t ifreqv[0x80]; // Note frequencies in 0..0xffffffff, for wave runners.
  int update_in_progress; // Duration of running update in frames, for new pcm printers.
  int64_t framec; // Total count generated since construction.
  struct romr *romr; // WEAK, OPTIONAL
  struct synth_cache *cache;
  
  // Event graph.
  struct synth_song *song;
  struct synth_song *song_next;
  struct synth_channel *channelv[SYNTH_CHANNEL_COUNT];
  int pidv[SYNTH_CHANNEL_COUNT];
  struct synth_builtin override_pid_0; // Hack for live instrument editor.
  
  // Signal graph.
  float qbuf[SYNTH_BUFFER_LIMIT];
  float qlevel;
  struct synth_voice voicev[SYNTH_VOICE_LIMIT];
  int voicec;
  struct synth_proc procv[SYNTH_PROC_LIMIT];
  int procc;
  struct synth_playback playbackv[SYNTH_PLAYBACK_LIMIT];
  int playbackc;
  struct sfg_printer **printerv;
  int printerc,printera;
};

void synth_end_song(struct synth *synth);
int synth_has_song_voices(const struct synth *synth);
void synth_welcome_song(struct synth *synth);

void synth_drop_voices_for_channel(struct synth *synth,uint8_t chid);

struct synth_voice *synth_voice_new(struct synth *synth);
struct synth_proc *synth_proc_new(struct synth *synth);
struct synth_playback *synth_playback_new(struct synth *synth);
struct synth_voice *synth_find_voice_by_chid_noteid(struct synth *synth,uint8_t chid,uint8_t noteid);
struct synth_proc *synth_find_proc_by_chid(struct synth *synth,uint8_t chid);

float *synth_wave_new_harmonics(const struct synth *synth,const uint8_t *coefv,int coefc);

int synth_frames_per_beat(const struct synth *synth);

#endif
