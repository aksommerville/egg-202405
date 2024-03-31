/* synth.h
 */
 
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

struct synth;
struct synth_wave;
struct synth_pcm;
struct synth_node;
struct synth_node_type;
struct synth_channel;
struct synth_program;

struct synth_delegate {
  void *userdata;
  int (*get_sound)(void *dstpp,int qual,int rid,void *userdata);
  int (*get_song)(void *dstpp,int qual,int rid,void *userdata);
};

void synth_del(struct synth *synth);

struct synth *synth_new(
  int rate,int chanc,
  const struct synth_delegate *delegate
);

void synth_updatei(int16_t *v,int c,struct synth *synth);
void synth_updatef(float *v,int c,struct synth *synth);

void synth_play_song(struct synth *synth,int qual,int rid,int force,int repeat);
void synth_play_sound(struct synth *synth,int qual,int rid,float trim,float pan);
float synth_get_playhead(struct synth *synth);

void synth_event(struct synth *synth,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b);

#endif
