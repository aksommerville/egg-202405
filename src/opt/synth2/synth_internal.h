#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#define SYNTH_CHANNEL_COUNT 16

// Updates will never be longer than this, in samples.
#define SYNTH_BUFFER_SIZE 512

#include "synth.h"
#include "synth_node.h"
#include "synth_channel.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

struct synth {
  int rate,chanc;
  struct synth_delegate delegate;
  struct synth_channel *channelv[SYNTH_CHANNEL_COUNT];
  struct synth_node *master;
};

#endif
