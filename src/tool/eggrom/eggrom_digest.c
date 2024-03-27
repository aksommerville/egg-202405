#include "eggrom_internal.h"

/* Any resource types where multiple resources come out of one file,
 * find them, create the multiple resources, and delete the original.
 */
 
int eggrom_expand_resources() {
  int err;
  int i=eggrom.romw->resc;
  while (i-->0) {
    struct romw_res *res=eggrom.romw->resv+i;
    if (res->rid) continue;
    switch (res->tid) {
    
      case EGG_TID_string: {
          if ((err=eggrom_string_expand(res->serial,res->serialc,res->qual,res->path))<0) {
            res=eggrom.romw->resv+i;
            if (err!=-2) fprintf(stderr,"%s: Unspecified error processing strings.\n",res->path);
            return -2;
          }
          romw_res_remove(eggrom.romw,i);
        } break;
        
      case EGG_TID_sound: {
          if ((err=eggrom_sound_expand(res->serial,res->serialc,res->qual,res->path))<0) {
            res=eggrom.romw->resv+i;
            if (err!=-2) fprintf(stderr,"%s: Unspecified error processing sounds.\n",res->path);
            return -2;
          }
          romw_res_remove(eggrom.romw,i);
        } break;
      
    }
  }
  return 0;
}

/* Any resource with (tid) but not (rid), make up a unique rid.
 */
 
static int eggrom_select_rid(struct romw_res *res) {
  
  /* First, if there is another of this tid with the same name and rid present, use the same rid.
   * This could come up a lot for strings, if they're created with names instead of numbers.
   */
  if (res->namec) {
    struct romw_res *other=eggrom.romw->resv;
    int i=eggrom.romw->resc;
    for (;i-->0;other++) {
      if (other->tid!=res->tid) continue;
      if (!other->rid) continue; // this also catches (other==res)
      if (other->namec!=res->namec) continue;
      if (memcmp(other->name,res->name,res->namec)) continue;
      res->rid=other->rid;
      return 0;
    }
  }
  
  /* Take the lowest unused rid for this tid, across all qual.
   * It's entirely possible for the rid space to be exhausted, ensure we catch that case.
   * Couple ways we could do this:
   *  1. Search for the highest existing rid and add one.
   *  2. Take a bitmap of all assigned rid.
   *  3. Check each valid rid starting at one.
   * (1) is no good because we really don't want to leave the list sparse.
   * (2) I dislike because it requires this 8 kB buffer.
   * (3) is going to perform very badly against large resource sets.
   * I don't like any of these options but will use (2).
   */
  uint8_t inuse[0x10000>>3]={0}; // [rid>>3]&(1<<(rid&7))
  inuse[0]=1; // Mark rid zero as "in use", we don't want it.
  struct romw_res *other=eggrom.romw->resv;
  int i=eggrom.romw->resc;
  for (;i-->0;other++) {
    if (other->tid!=res->tid) continue;
    inuse[other->rid>>3]|=1<<(other->rid&7);
  }
  int major=0;
  for (;major<sizeof(inuse);major++) {
    if (inuse[major]==0xff) continue;
    int minor=0;
    uint8_t mask=1;
    for (;inuse[major]&mask;minor++,mask<<=1) ;
    res->rid=(major<<3)|minor;
    return 0;
  }
  
  fprintf(stderr,"%s: Resource ID space for type %d is exhausted! Limit 65535.\n",eggrom.exename,res->tid);
  return -2;
}
 
int eggrom_assign_missing_ids() {
  int err;
  struct romw_res *res=eggrom.romw->resv;
  int i=eggrom.romw->resc;
  for (;i-->0;res++) {
    if (!res->tid) continue;
    if (res->rid) continue;
    if ((err=eggrom_select_rid(res))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unable to infer resource ID.\n",res->path);
      return -2;
    }
  }
  return 0;
}

/* Perform any special per-type processing.
 * The TOC is final at this point: All resources that are going to exist exist, and they have final IDs.
 * If we have text input that becomes binary output, this is the most appropriate place for it.
 */
 
int eggrom_link_resources() {
  struct romw_res *res=eggrom.romw->resv;
  int i=eggrom.romw->resc;
  for (;i-->0;res++) {
    if (!res->rid) continue; // Not a real resource, skip it.
    eggrom.scratch.c=0;
    int err=0;
    switch (res->tid) {
    
      case EGG_TID_metadata: err=eggrom_metadata_compile(&eggrom.scratch,res); break;
      case EGG_TID_image: err=eggrom_image_compile(&eggrom.scratch,res); break;
      case EGG_TID_song: err=eggrom_song_compile(&eggrom.scratch,res); break;
      case EGG_TID_sound: err=eggrom_sound_compile(&eggrom.scratch,res); break;
      case EGG_TID_map: err=eggrom_map_compile(&eggrom.scratch,res); break;
    
      default: continue;
    }
    if (err<0) {
      if (err!=-2) {
        if (res->path) fprintf(stderr,"%s: Unspecified error converting resource.\n",res->path);
        else fprintf(stderr,"%d:%d:%d: Unspecified error converting resource.\n",res->tid,res->qual,res->rid);
      }
      return -2;
    }
    if (!eggrom.scratch.c) continue; // Compilers may leave encoder untouched to preserve original content.
    if (romw_res_set_serial(res,eggrom.scratch.v,eggrom.scratch.c)<0) return -1;
  }
  return 0;
}
