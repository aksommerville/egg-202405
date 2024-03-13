#include "romr.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

/* Validate and iterate ROM file, no context.
 */
 
int romr_for_each(
  const void *src,int srcc,
  int (*cb)(uint8_t tid,uint16_t qual,uint16_t rid,const void *v,int c,void *userdata),
  void *userdata
) {
  if (!src) return -1;
  if (srcc<16) return -1;
  const uint8_t *SRC=src;
  
  if (memcmp(SRC,"\x00\x0egg",4)) return -1;
  int hdrlen=(SRC[4]<<24)|(SRC[5]<<16)|(SRC[6]<<8)|SRC[7];
  int toclen=(SRC[8]<<24)|(SRC[9]<<16)|(SRC[10]<<8)|SRC[11];
  int heaplen=(SRC[12]<<24)|(SRC[13]<<16)|(SRC[14]<<8)|SRC[15];
  if (hdrlen<16) return -1;
  if (toclen<0) return -1;
  if (heaplen<0) return -1;
  int tocstart=hdrlen;
  if (tocstart>srcc-toclen) return -1;
  int heapstart=tocstart+toclen;
  if (heapstart>srcc-heaplen) return -1;
  
  const uint8_t *toc=SRC+tocstart;
  const uint8_t *heap=SRC+heapstart;
  int tocp=0,heapp=0,tid=1,qual=0,rid=1,err;
  while (tocp<toclen) {
    uint8_t lead=toc[tocp++];
    switch (lead&0xc0) {
    
      case 0x00: { // NEXTTYPE
          int d=lead+1;
          if (tid>0xff-d) return -1;
          tid+=d;
          qual=0;
          rid=1;
        } break;
        
      case 0x40: { // NEXTQUAL or invalid
          if (lead&0x3f) return -1;
          if (tocp>toclen-2) return -1;
          int d=((toc[tocp]<<8)|toc[tocp+1])+1;
          tocp+=2;
          if (qual>0xffff-d) return -1;
          qual+=d;
          rid=1;
        } break;
        
      case 0x80: { // SHORT
          if (rid>0xffff) return -1;
          int c=lead&0x3f;
          if (heapp>heaplen-c) return -1;
          if (err=cb(tid,qual,rid,heap+heapp,c,userdata)) return err;
          heapp+=c;
          rid++;
        } break;
        
      case 0xc0: { // LONG or invalid
          if (lead&0x30) return -1;
          if (tocp>toclen-2) return -1;
          int c=((lead&0x0f)<<16)|(toc[tocp]<<8)|toc[tocp+1];
          tocp+=2;
          if (heapp>heaplen-c) return -1;
          if (rid>0xffff) return -1;
          if (err=cb(tid,qual,rid,heap+heapp,c,userdata)) return err;
          heapp+=c;
          rid++;
        } break;
    
    }
  }
  return 0;
}

/* Cleanup.
 */
 
static void romr_bucket_cleanup(struct romr_bucket *bucket) {
  if (bucket->resv) free(bucket->resv);
}

void romr_cleanup(struct romr *romr) {
  if (romr->src&&romr->ownsrc) free((void*)romr->src);
  if (romr->bucketv) {
    while (romr->bucketc-->0) romr_bucket_cleanup(romr->bucketv+romr->bucketc);
    free(romr->bucketv);
  }
  memset(romr,0,sizeof(struct romr));
}

/* Decode.
 */
 
static int romr_bucket_add(struct romr_bucket *bucket,const void *v,int c) {
  if (bucket->resc>=bucket->resa) {
    int na=bucket->resa+32;
    if (na>INT_MAX/sizeof(struct romr_res)) return -1;
    void *nv=realloc(bucket->resv,sizeof(struct romr_res)*na);
    if (!nv) return -1;
    bucket->resv=nv;
    bucket->resa=na;
  }
  struct romr_res *res=bucket->resv+bucket->resc++;
  res->v=v;
  res->c=c;
  return 0;
}
 
static int romr_decode_1(uint8_t tid,uint16_t qual,uint16_t rid,const void *v,int c,void *userdata) {
  struct romr *romr=userdata;
  struct romr_bucket *bucket=0;
  if (romr->bucketc) {
    bucket=romr->bucketv+romr->bucketc-1;
    if ((bucket->tid!=tid)||(bucket->qual!=qual)) bucket=0;
  }
  if (!bucket) {
    if (romr->bucketc>=romr->bucketa) {
      int na=romr->bucketa+32;
      if (na>INT_MAX/sizeof(struct romr_bucket)) return -1;
      void *nv=realloc(romr->bucketv,sizeof(struct romr_bucket)*na);
      if (!nv) return -1;
      romr->bucketv=nv;
      romr->bucketa=na;
    }
    bucket=romr->bucketv+romr->bucketc++;
    bucket->tid=tid;
    bucket->qual=qual;
    bucket->resv=0;
    bucket->resc=0;
    bucket->resa=0;
  }
  while (rid>bucket->resc+1) {
    if (romr_bucket_add(bucket,0,0)<0) return -1;
  }
  if (romr_bucket_add(bucket,v,c)<0) return -1;
  return 0;
}

int romr_decode_borrow(struct romr *romr,const void *src,int srcc) {
  if (romr->bucketc) return -1;
  if (romr_for_each(src,srcc,romr_decode_1,romr)<0) {
    romr_cleanup(romr);
    return -1;
  }
  romr->src=src;
  romr->srcc=srcc;
  return 0;
}

/* Copy, then decode.
 */
 
int romr_decode_copy(struct romr *romr,const void *src,int srcc) {
  if (!src||(srcc<0)) return -1;
  void *nv=malloc(srcc);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  int err=romr_decode_borrow(romr,nv,srcc);
  if (err<0) {
    free(nv);
    romr_cleanup(romr);
    return err;
  }
  romr->ownsrc=1;
  return 0;
}

/* Search buckets.
 */
 
static int romr_bucketv_search(const struct romr *romr,uint8_t tid,uint16_t qual) {
  int lo=0,hi=romr->bucketc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct romr_bucket *bucket=romr->bucketv+ck;
         if (tid<bucket->tid) hi=ck;
    else if (tid>bucket->tid) lo=ck+1;
    else if (qual<bucket->qual) hi=ck;
    else if (qual>bucket->qual) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Get fully-qualified resource.
 */

int romr_get_qualified(void *dstpp,const struct romr *romr,uint8_t tid,uint16_t qual,uint16_t rid) {
  if (!rid) return 0;
  int bucketp=romr_bucketv_search(romr,tid,qual);
  if (bucketp<0) return 0;
  const struct romr_bucket *bucket=romr->bucketv+bucketp;
  if (rid>bucket->resc) return 0;
  const struct romr_res *res=bucket->resv+rid-1;
  *(const void**)dstpp=res->v;
  return res->c;
}

/* Get resource with default qualifier.
 */
 
int romr_get(void *dstpp,const struct romr *romr,uint8_t tid,uint16_t rid) {
  uint16_t qual=romr->qual_by_tid[tid];
  int c=romr_get_qualified(dstpp,romr,tid,qual,rid);
  if (c>0) return c;
  if (!qual) return 0;
  return romr_get_qualified(dstpp,romr,tid,0,rid);
}

/* Language-qualified types.
 */

uint16_t romr_get_language(const struct romr *romr) {
  return romr->qual_by_tid[EGG_TID_string];
}

void romr_set_language(struct romr *romr,uint16_t lang) {
  romr->qual_by_tid[EGG_TID_string]=lang;
  romr->qual_by_tid[EGG_TID_image]=lang;
  romr->qual_by_tid[EGG_TID_sound]=lang;
}
