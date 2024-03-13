/* wav.h
 * WAV is a simple header for uncompressed audio.
 */
 
#ifndef WAV_H
#define WAV_H

struct sr_encoder;

struct wav_file {
  void *v;
  int ownv;
  int samplec;
  int framec; // samplec*chanc
  int chanc;
  int rate; // hz
  int samplesize; // bits, must be a multiple of 8.
};

void wav_file_cleanup(struct wav_file *file);
void wav_file_del(struct wav_file *file);

/* If we're borrowing the sample data (always the case after decoding),
 * this copies it and allows you to safely free the original serial dump.
 * If we're already marked as owning it, does nothing.
 * (samplec,samplesize) must be correctly populated.
 */
int wav_file_own(struct wav_file *file);

/* To encode, you must populate (v,samplesize) and at least one of (samplec,framec).
 * We can make something up for everything else.
 */
int wav_file_encode(struct sr_encoder *dst,const struct wav_file *file);

/* Decode to a new object.
 * ***The new object's samples point into the original (src).***
 * Call wav_file_own() after to get a fresh copy if desired.
 */
struct wav_file *wav_file_decode(const void *src,int srcc);

/* In general, reformatting PCM is somebody else's problem.
 * We do provide this one hook to drop extra channels and truncate or expand to 16 bits/samples.
 * Reallocates (file->v) if necessary. Will not overwrite if we don't own it.
 */
int wav_file_force_16bit_mono(struct wav_file *file);

#endif
