/* synth_node.h
 * Generic node in the signal graph.
 * Nodes don't interact with events; that's "channel".
 */
 
#ifndef SYNTH_NODE_H
#define SYNTH_NODE_H

struct synth;
struct synth_node;
struct synth_node_type;

/* Generic node.
 *********************************************************************/

struct synth_node_type {
  const char *name;
  int objlen;
  void (*del)(struct synth_node *node);
  int (*validate_src)(struct synth_node *node,struct synth_node *src); // null to accept any at any time
  int (*ready)(struct synth_node *node); // null to accept any; called before first update
  void (*end_cycle)(struct synth_node *node); // called at end of update cycle, most nodes shouldn't care
};

struct synth_node {
  const struct synth_node_type *type;
  struct synth *synth; // WEAK
  int refc;
  int chanc;
  int ready;
  int dstc; // We don't record identity of outputs, but do count them.
  int finished;
  void (*update)(float *v,int c,struct synth_node *node); // REQUIRED
  struct synth_node **srcv;
  int srcc,srca;
};

void synth_node_del(struct synth_node *node);
int synth_node_ref(struct synth_node *node);
struct synth_node *synth_node_new(struct synth *synth,const struct synth_node_type *type);

int synth_node_add_src(struct synth_node *dst,struct synth_node *src);
void synth_node_remove_src(struct synth_node *dst,struct synth_node *src);

int synth_node_ready(struct synth_node *node);

/* Node types.
 *******************************************************************/

/* Stereo or mono, and inputs can be stereo or mono.
 * When a mono node is input to a stereo mixer, you can set pan -1..1.
 */
extern const struct synth_node_type synth_node_type_mixer;
int synth_node_mixer_set_pan(struct synth_node *node,struct synth_node *src,float pan);

/* Verbatim PCM playback.
 */
extern const struct synth_node_type synth_node_type_pcm;
int synth_node_pcm_init(struct synth_node *node,struct synth_pcm *pcm);

/* One input, mono or stereo. Multiply input by an ADSR envelope.
 * Must set chanc before init, if not 1.
 */
extern const struct synth_node_type synth_node_type_env;
int synth_node_env_set_chanc(struct synth_node *node,int chanc);
int synth_node_env_init_tiny(struct synth_node *node,uint8_t v,float velocity);
int synth_node_env_init_param(struct synth_node *node,uint16_t v,float velocity);

/* Normal to have no inputs, then we run at a fixed rate.
 * May have one mono input. Signal is a pitch bend, in octaves.
 */
extern const struct synth_node_type synth_node_type_osc;
int synth_node_osc_set_rate(struct synth_node *node,float ratenorm);
int synth_node_osc_set_shape(struct synth_node *node,uint8_t shape);
int synth_node_osc_set_wave(struct synth_node *node,struct synth_wave *wave);
int synth_node_osc_set_phase(struct synth_node *node,float phase);
int synth_node_osc_input_mode(struct synth_node *node,int mode);

#define SYNTH_OSC_INPUT_BEND 0 /* Default. Input is a pitch bend in octaves. (ie multiply rate by 2**n) */
#define SYNTH_OSC_INPUT_MLT  1 /* Input is a multiplier. Should stay above zero and idle at one. */
#define SYNTH_OSC_INPUT_ABS  2 /* Input is absolute rate (normalized). Osc's own rate is ignored. */

#define SYNTH_OSC_SHAPE_SINE 0
#define SYNTH_OSC_SHAPE_SQUARE 1
#define SYNTH_OSC_SHAPE_SAW 2
#define SYNTH_OSC_SHAPE_TRIANGLE 3

/* White noise.
 */
extern const struct synth_node_type synth_node_type_noise;

/* Multiply by scalar.
 */
extern const struct synth_node_type synth_node_type_gain;
int synth_node_gain_set_gain(struct synth_node *node,float gain);

/* Same as WebAudio WaveShaperNode.
 * Incoming samples are replaced by values from (v), with (-1..1) scaling to (0..c), then interpolate linearly.
 * Curve should have an odd length, with zero in the middle, and should not change direction.
 */
extern const struct synth_node_type synth_node_type_waveshaper;
int synth_node_waveshaper_set_curve(struct synth_node *node,const float *v,int c);

/* Holds its input and delivers verbatim to each output.
 * eg for shared LFOs.
 */
extern const struct synth_node_type synth_node_type_splitter;

/* Specialized IIR filters.
 * I'll try to reproduce the behavior of WebAudio's BiquadFilterNode.
 */
extern const struct synth_node_type synth_node_type_biquad;
int synth_node_biquad_init_lowpass(struct synth_node *node,float q,float freq); // q in dB
int synth_node_biquad_init_highpass(struct synth_node *node,float q,float freq); // q in dB
int synth_node_biquad_init_bandpass(struct synth_node *node,float q,float freq); // q in 0..100 = wide..narrow
int synth_node_biquad_init_notch(struct synth_node *node,float q,float freq); // q in 0..100 = wide..narrow

/* Simple infinite delay.
 */
extern const struct synth_node_type synth_node_type_delay;
int synth_node_delay_init(struct synth_node *node,int framec,float dry,float wet,float sto,float fbk);

/* Drops level to zero over some time, then reports completion.
 */
extern const struct synth_node_type synth_node_type_terminator;
int synth_node_terminator_init(struct synth_node *node,int framec);

#endif
