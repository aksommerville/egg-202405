/* pcmprintc.h
 * Compiler for our "pcmprint" unit.
 * The printer unit only accepts the binary format that we produce.
 * They're separate because compilation is only needed at build time, no sense building that into the runtime.
 */
 
#ifndef PCMPRINTC_H
#define PCMPRINTC_H

struct pcmprintc;

void pcmprintc_del(struct pcmprintc *ppc);

/* (src) and (refname) are borrowed -- you must hold them constant throughout pcmprintc's life.
 * (lineno0) is normally zero. Use the introducer line number for inner blocks, if you split them in advance.
 */
struct pcmprintc *pcmprintc_new(const char *src,int srcc,const char *refname,int lineno0);

struct pcmprintc_output {
  const void *bin;
  int binc;
  const char *id;
  int idc;
  int idn;
  const char *refname;
  int lineno;
};

/* Advance compiler across the next sound effect.
 * If one is present, returns >0 and populates (output).
 * At EOF, returns zero and doesn't touch (output).
 * -2 for errors if we logged them.
 * Any other <0 for unlogged errors.
 *
 * In (output):
 *  - (bin) may be freed or overwritten by the next pcmprintc call.
 *  - (id) is the sound's name verbatim from source. Can be empty.
 *  - (idn) is (id) as an integer, or zero by default.
 *  - (refname,lineno) identify the first line of the effect.
 */
int pcmprintc_next(struct pcmprintc_output *output,struct pcmprintc *ppc);

/* Same as pcmprintc_next(), but assert that one sound exist and we reach EOF after it.
 * This is a convenience for eggrom_tid_sound.c:eggrom_sound_compile().
 */
int pcmprintc_next_final(struct pcmprintc_output *output,struct pcmprintc *ppc);

#endif
