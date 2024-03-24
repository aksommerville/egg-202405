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
 * Beware that when playing from serial, we won't necessarily recognize songs as being the same.
 */
void synth_play_song(struct synth *synth,int qual,int songid,int force,int repeat);
void synth_play_song_serial(
  struct synth *synth,
  const void *src,int srcc,
  int safe_to_borrow,
  int force,int repeat
);

/* Play a fire-and-forget sound effect.
 * Avoid using the direct serial one if you can; there's a fair amount of decoding involved.
 */
void synth_play_sound(struct synth *synth,int qual,int soundid,double trim,double pan);
void synth_play_sound_serial(
  struct synth *synth,
  const void *src,int srcc,
  double trim,double pan
);

/* Current song time in milliseconds, or -1 if no song.
 * This counter does return to zero when the song repeats.
 * Beware that this is not the whole picture, for reporting to the game: 
 * You should try to estimate how far into its last buffer the PCM driver is.
 */
int synth_get_playhead(struct synth *synth);

#endif
