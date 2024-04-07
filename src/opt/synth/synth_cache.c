#include "synth_internal.h"

/* Delete.
 */
 
static void synth_cache_entry_cleanup(struct synth_cache_entry *entry) {
  sfg_pcm_del(entry->pcm);
}
 
void synth_cache_del(struct synth_cache *cache) {
  if (!cache) return;
  if (cache->entryv) {
    while (cache->entryc-->0) synth_cache_entry_cleanup(cache->entryv+cache->entryc);
    free(cache->entryv);
  }
  free(cache);
}

/* New.
 */

struct synth_cache *synth_cache_new() {
  struct synth_cache *cache=calloc(1,sizeof(struct synth_cache));
  if (!cache) return 0;
  return cache;
}

/* Search.
 */

int synth_cache_search(const struct synth_cache *cache,int qual,int soundid) {
  int lo=0,hi=cache->entryc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct synth_cache_entry *q=cache->entryv+ck;
         if (qual<q->qual) hi=ck;
    else if (qual>q->qual) lo=ck+1;
    else if (soundid<q->soundid) hi=ck;
    else if (soundid>q->soundid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Get by index.
 */

struct sfg_pcm *synth_cache_get(const struct synth_cache *cache,int p) {
  if ((p<0)||(p>=cache->entryc)) return 0;
  return cache->entryv[p].pcm;
}

/* Add.
 */

int synth_cache_add(struct synth_cache *cache,int p,int qual,int soundid,struct sfg_pcm *pcm) {
  if ((p<0)||(p>cache->entryc)) return -1;
  if (p) {
    const struct synth_cache_entry *q=cache->entryv+p-1;
    if (qual<q->qual) return -1;
    if ((qual==q->qual)&&(soundid<=q->soundid)) return -1;
  }
  if (p<cache->entryc) {
    const struct synth_cache_entry *q=cache->entryv+p;
    if (qual>q->qual) return -1;
    if ((qual==q->qual)&&(soundid>=q->soundid)) return -1;
  }
  if (cache->entryc>=cache->entrya) {
    int na=cache->entrya+32;
    if (na>INT_MAX/sizeof(struct synth_cache_entry)) return -1;
    void *nv=realloc(cache->entryv,sizeof(struct synth_cache_entry)*na);
    if (!nv) return -1;
    cache->entryv=nv;
    cache->entrya=na;
  }
  if (sfg_pcm_ref(pcm)<0) return -1;
  struct synth_cache_entry *entry=cache->entryv+p;
  memmove(entry+1,entry,sizeof(struct synth_cache_entry)*(cache->entryc-p));
  cache->entryc++;
  entry->qual=qual;
  entry->soundid=soundid;
  entry->pcm=pcm;
  return 0;
}
