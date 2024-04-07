/* synth_cache.h
 * Holds PCM objects we've printed.
 * Print may be in progress, one never really knows.
 */

#ifndef SYNTH_CACHE_H
#define SYNTH_CACHE_H

struct sfg_pcm;

struct synth_cache {
  struct synth_cache_entry {
    int qual,soundid;
    struct sfg_pcm *pcm;
  } *entryv;
  int entryc,entrya;
};

void synth_cache_del(struct synth_cache *cache);

struct synth_cache *synth_cache_new();

int synth_cache_search(const struct synth_cache *cache,int qual,int soundid);

struct sfg_pcm *synth_cache_get(const struct synth_cache *cache,int p);

int synth_cache_add(struct synth_cache *cache,int p,int qual,int soundid,struct sfg_pcm *pcm);

#endif
