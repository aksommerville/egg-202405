/* pcmprint.h
 * Generates PCM dumps from tiny source data, for sound effects.
 * See etc/doc/pcmprint.md for details.
 */
 
#ifndef PCMPRINT_H
#define PCMPRINT_H

struct pcmprint;

struct pcmprint_pcm {
  int refc;
  int c;
  float v[];
};

void pcmprint_pcm_del(struct pcmprint_pcm *pcm);
int pcmprint_pcm_ref(struct pcmprint_pcm *pcm);
struct pcmprint_pcm *pcmprint_pcm_new(int c);

void pcmprint_del(struct pcmprint *pcmprint);

/* Validates (src) and allocates the pcm dump but does not begin printing yet.
 */
struct pcmprint *pcmprint_new(int rate,const void *src,int srcc);

/* You can take the pcm object at any time, its address and length won't change during print.
 * Samples are zero until we print them.
 */
struct pcmprint_pcm *pcmprint_get_pcm(const struct pcmprint *pcmprint);

/* Print (c) samples of PCM, or less if we're at the end.
 * Returns 0 if some remain unprinted or 1 if finished, no errors.
 * Call with (c<1) to only test whether printing is complete.
 */
int pcmprint_update(struct pcmprint *pcmprint,int c);

#endif
