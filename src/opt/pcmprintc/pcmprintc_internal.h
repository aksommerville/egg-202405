#ifndef PCMPRINTC_INTERNAL_H
#define PCMPRINTC_INTERNAL_H

#include "pcmprintc.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

struct pcmprintc {
  struct sr_encoder dst;
  const char *id;
  int idc;
  struct sr_decoder src;
  const char *refname;
  int lineno;
  
  float master;
  int duration;
  struct pcmprintc_command {
    const char *kw,*arg;
    int kwc,argc;
    int lineno;
  } *commandv;
  int commandc,commanda;
  
  // Transient cache of level length in ms.
  int recent_level_length;
  int discard_voice;
};

#endif
