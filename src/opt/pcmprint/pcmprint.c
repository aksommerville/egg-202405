#include "pcmprint.h"

//TODO

void pcmprint_pcm_del(struct pcmprint_pcm *pcm) {}

int pcmprint_pcm_ref(struct pcmprint_pcm *pcm) { return -1; }

struct pcmprint_pcm *pcmprint_pcm_new(int c) { return 0; }

void pcmprint_del(struct pcmprint *pcmprint) {}

struct pcmprint *pcmprint_new(int rate,const void *src,int srcc) { return 0; }

struct pcmprint_pcm *pcmprint_get_pcm(const struct pcmprint *pcmprint) { return 0; }

int pcmprint_update(struct pcmprint *pcmprint,int c) { return 1; }
