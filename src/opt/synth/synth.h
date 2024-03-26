/* synth.h
 * Standard synthesizer for Egg, native runtime.
 */
 
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

struct synth;
struct romr;

void synth_del(struct synth *synth);

/* If you want to address resources by ID, you must provide a romr here.
 * We borrow it weakly; you must keep it alive as long as the synth.
 */
struct synth *synth_new(
  int rate,int chanc,
  struct romr *romr
);

/* Generating a floating-point signal is more natural for us.
 * That's probably not the case for you, so we also provide quantization to int16_t.
 * Both of these take a sample count regardless of channel count, my usual convention.
 * It's safe to change data types on the fly (but why would you?).
 */
void synth_updatef(float *v,int c,struct synth *synth);
void synth_updatei(int16_t *v,int c,struct synth *synth);

/* Begin a new song from songid (romr required) or raw serial data (Egg format).
 * (force) to play from the start even if already playing.
 * Playing from serial data, it's always "force".
 */
void synth_play_song(struct synth *synth,int qual,int songid,int force,int repeat);
void synth_play_song_serial(
  struct synth *synth,
  const void *src,int srcc,
  int safe_to_borrow,
  int repeat
);

/* Play a fire-and-forget sound effect, from some resource.
 */
void synth_play_sound(struct synth *synth,int qual,int soundid,float trim,float pan);

/* Play sound effect from encoded resource.
 * !!! Don't use this in real life !!!
 * This will decode and print the PCM every time you play, it's very expensive.
 * Intended for editor tooling, where you want to preview an effect on the fly.
 */
void synth_play_sound_serial(
  struct synth *synth,
  const void *src,int srcc,
  float trim,float pan
);

/* Current song time in beats, or -1 if no song.
 * This counter does return to zero when the song repeats.
 * Beware that this is not the whole picture, for reporting to the game: 
 * You should try to estimate how far into its last buffer the PCM driver is.
 */
double synth_get_playhead(struct synth *synth);

/* You may push events into the system at any time.
 * Beware that this is the same event bus the song is using.
 * Songs can only address channels 0..7. You can use 8..15 and be confident you fully control them.
 * We have a custom opcode 0x98 "Note Once", which begins a note with a prespecified duration (dur) in frames.
 * You may send 0xff System Reset with a channel to reset only that channel, or chid 0xff to reset everything.
 */
void synth_event(struct synth *synth,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int dur);

#endif
