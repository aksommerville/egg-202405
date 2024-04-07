/* sfg.h
 * Sound effects generator for Egg. (it stands for "Sound Effeggts").
 */
 
#ifndef SFG_H
#define SFG_H

#include <stdint.h>

struct sr_encoder;

/* Dumb PCM dump.
 ********************************************************/

struct sfg_pcm {
  int refc;
  int c;
  float v[];
};

void sfg_pcm_del(struct sfg_pcm *pcm);
int sfg_pcm_ref(struct sfg_pcm *pcm);
struct sfg_pcm *sfg_pcm_new(int c);

/* Printer.
 * You must supply an sfg sound in the binary format.
 *******************************************************/

struct sfg_printer;

void sfg_printer_del(struct sfg_printer *printer);
struct sfg_printer *sfg_printer_new(int rate,const void *bin,int binc);

/* The PCM dump can be fetched from a printer at any time.
 * It will have its final length immediately, but samples will be zero until printed.
 * Returns WEAK.
 */
struct sfg_pcm *sfg_printer_get_pcm(const struct sfg_printer *printer);

/* Print at least (c) more samples, or stop at end of sound.
 * Returns zero if more samples remain to be printed, nonzero if finished.
 * It's legal to call with (c<1), to only test for completion.
 */
int sfg_printer_update(struct sfg_printer *printer,int c);

/* Compiler.
 * Generate our binary format from our text format.
 ********************************************************/

/* Generate binary from text, without fences.
 * If (refname) not null, we'll log errors to stderr.
 */
int sfg_compile(struct sr_encoder *dst,const char *src,int srcc,const char *refname,int lineno0);

/* For multi-sound files, trigger (cb) for each sound block, with content ready for sfg_compile().
 * If there's no fences, we'll trigger (cb) once, with the entire input and no id.
 */
int sfg_split(
  const char *src,int srcc,
  const char *refname,
  int (*cb)(const char *src,int srcc,const char *id,int idc,int idn,const char *refname,int lineno0,void *userdata),
  void *userdata
);

#endif
