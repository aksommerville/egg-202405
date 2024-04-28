#include "localstore.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

/* Cleanup.
 */
 
static void localstore_entry_cleanup(struct localstore_entry *entry) {
  if (entry->k) free(entry->k);
  if (entry->v) free(entry->v);
}
 
void localstore_cleanup(struct localstore *localstore) {
  if (!localstore) return;
  if (localstore->entryv) {
    while (localstore->entryc-->0) {
      localstore_entry_cleanup(localstore->entryv+localstore->entryc);
    }
    free(localstore->entryv);
  }
  memset(localstore,0,sizeof(struct localstore));
}

/* Search.
 */
 
static int localstore_search(const struct localstore *localstore,const char *k,int kc) {
  int lo=0,hi=localstore->entryc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct localstore_entry *entry=localstore->entryv+ck;
         if (kc<entry->kc) hi=ck;
    else if (kc>entry->kc) lo=ck+1;
    else {
      int cmp=memcmp(k,entry->k,kc);
           if (cmp<0) hi=ck;
      else if (cmp>0) lo=ck+1;
      else return ck;
    }
  }
  return -lo-1;
}

/* Insert.
 */
 
static int localstore_insert(struct localstore *localstore,int p,const char *k,int kc,const char *v,int vc) {
  if ((kc<1)||(kc>0xff)) return -1;
  if ((vc<1)||(vc>0xffff)) return -1;
  if ((p<0)||(p>localstore->entryc)) return -1;
  if (localstore->entryc>=localstore->entrya) {
    int na=localstore->entrya+16;
    if (na>INT_MAX/sizeof(struct localstore_entry)) return -1;
    void *nv=realloc(localstore->entryv,sizeof(struct localstore_entry)*na);
    if (!nv) return -1;
    localstore->entryv=nv;
    localstore->entrya=na;
  }
  char *nk=malloc(kc+1);
  if (!nk) return -1;
  memcpy(nk,k,kc);
  nk[kc]=0;
  char *nv=malloc(vc+1);
  if (!nv) { free(nk); return -1; }
  memcpy(nv,v,vc);
  nv[vc]=0;
  struct localstore_entry *entry=localstore->entryv+p;
  memmove(entry+1,entry,sizeof(struct localstore_entry)*(localstore->entryc-p));
  localstore->entryc++;
  entry->k=nk;
  entry->kc=kc;
  entry->v=nv;
  entry->vc=vc;
  localstore->dirty=1;
  return 0;
}

/* Replace value.
 */
 
static int localstore_entry_replace(struct localstore_entry *entry,const char *v,int vc) {
  if ((vc<1)||(vc>0xffff)) return -1;
  if (vc>entry->vc) {
    char *nv=realloc(entry->v,vc+1);
    if (!nv) return -1;
    entry->v=nv;
  }
  memcpy(entry->v,v,vc);
  entry->v[vc]=0;
  entry->vc=vc;
  return 0;
}

/* Remove entry.
 */
 
static void localstore_remove(struct localstore *localstore,int p) {
  if ((p<0)||(p>=localstore->entryc)) return;
  struct localstore_entry *entry=localstore->entryv+p;
  localstore_entry_cleanup(entry);
  localstore->entryc--;
  memmove(entry,entry+1,sizeof(struct localstore_entry)*(localstore->entryc-p));
  localstore->dirty=1;
}

/* Get.
 */

int localstore_get(const char **dstpp,const struct localstore *localstore,const char *k,int kc) {
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  int p=localstore_search(localstore,k,kc);
  if (p<0) return 0;
  const struct localstore_entry *entry=localstore->entryv+p;
  if (dstpp) *dstpp=entry->v;
  return entry->vc;
}

/* Set.
 */
 
int localstore_set(struct localstore *localstore,const char *k,int kc,const char *v,int vc) {
  if (!k) return -1;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  int p=localstore_search(localstore,k,kc);
  if (p<0) {
    if (!vc) return 0;
    p=-p-1;
    if (localstore_insert(localstore,p,k,kc,v,vc)<0) return -1;
  } else {
    if (!vc) {
      localstore_remove(localstore,p);
    } else {
      if (localstore_entry_replace(localstore->entryv+p,v,vc)<0) return -1;
    }
  }
  localstore->dirty=1;
  return 0;
}

/* Read file and decode.
 */

int localstore_load(struct localstore *localstore,const char *path) {
  while (localstore->entryc>0) {
    localstore->entryc--;
    localstore_entry_cleanup(localstore->entryv+localstore->entryc);
  }
  unsigned char *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) return -1;
  int serialp=0,err=0;
  while (serialp<serialc) {
  
    int kc=serial[serialp++];
    if (kc>serialc-serialp) { err=-1; break; }
    const char *k=(char*)serial+serialp;
    serialp+=kc;
    if (serialp>serialc-2) { err=-1; break; }
    int vc=(serial[serialp]<<8)|serial[serialp+1];
    serialp+=2;
    if (serialp>serialc-vc) { err=-1; break; }
    const char *v=(char*)serial+serialp;
    serialp+=vc;
    
    // We could insert instead of set here, if we're willing to assume that the file was written in our sort order.
    // Realistically that should always be true. But I don't like betting on it.
    if ((err=localstore_set(localstore,k,kc,v,vc))<0) break;
  }
  free(serial);
  if (err<0) return -1;
  localstore->dirty=0;
  return 0;
}

/* Encode and write file.
 */
 
int localstore_save(struct localstore *localstore,const char *path) {
  if (!localstore->save_permit) return -1;
  struct sr_encoder encoder={0};
  const struct localstore_entry *entry=localstore->entryv;
  int i=localstore->entryc;
  for (;i-->0;entry++) {
    if (
      (sr_encode_u8(&encoder,entry->kc)<0)||
      (sr_encode_raw(&encoder,entry->k,entry->kc)<0)||
      (sr_encode_intbe(&encoder,entry->vc,2)<0)||
      (sr_encode_raw(&encoder,entry->v,entry->vc)<0)
    ) {
      sr_encoder_cleanup(&encoder);
      return -1;
    }
  }
  int err=file_write(path,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  if (err<0) return -1;
  localstore->dirty=0;
  return 0;
}
