#include "romw_internal.h"

/* Delete.
 */
 
static void romw_res_cleanup(struct romw_res *res) {
  if (res->path) free(res->path);
  if (res->name) free(res->name);
  if (res->serial) free(res->serial);
}

void romw_del(struct romw *romw) {
  if (!romw) return;
  if (romw->resv) {
    while (romw->resc-->0) romw_res_cleanup(romw->resv+romw->resc);
    free(romw->resv);
  }
  free(romw);
}

/* New.
 */

struct romw *romw_new() {
  struct romw *romw=calloc(1,sizeof(struct romw));
  if (!romw) return 0;
  return romw;
}

/* Search.
 */

int romw_search_exact(const struct romw *romw,uint8_t tid,uint16_t qual,uint16_t rid) {
  const struct romw_res *res=romw->resv;
  int i=0;
  for (;i<romw->resc;i++,res++) {
    if (res->tid!=tid) continue;
    if (res->qual!=qual) continue;
    if (res->rid!=rid) continue;
    return i;
  }
  return -1;
}

int romw_search_tid(const struct romw *romw,uint8_t tid,int startp) {
  int i=(startp>=0)?startp:0;
  const struct romw_res *res=romw->resv+i;
  for (;i<romw->resc;i++,res++) {
    if (res->tid!=tid) continue;
    return i;
  }
  return -1;
}

int romw_search_tid_qual(const struct romw *romw,uint8_t tid,uint16_t qual,int startp) {
  int i=(startp>=0)?startp:0;
  const struct romw_res *res=romw->resv+i;
  for (;i<romw->resc;i++,res++) {
    if (res->tid!=tid) continue;
    if (res->qual!=qual) continue;
    return i;
  }
  return -1;
}

int romw_search_tid_rid(const struct romw *romw,uint8_t tid,uint16_t rid,int startp) {
  int i=(startp>=0)?startp:0;
  const struct romw_res *res=romw->resv+i;
  for (;i<romw->resc;i++,res++) {
    if (res->tid!=tid) continue;
    if (res->rid!=rid) continue;
    return i;
  }
  return -1;
}

/* Add resource.
 */

struct romw_res *romw_res_add(struct romw *romw) {
  if (!romw) return 0;
  if (romw->resc>=romw->resa) {
    int na=romw->resa+128;
    if (na>INT_MAX/sizeof(struct romw_res)) return 0;
    void *nv=realloc(romw->resv,sizeof(struct romw_res)*na);
    if (!nv) return 0;
    romw->resv=nv;
    romw->resa=na;
  }
  struct romw_res *res=romw->resv+romw->resc++;
  memset(res,0,sizeof(struct romw_res));
  return res;
}

/* Remove resource.
 */
 
void romw_res_remove(struct romw *romw,int p) {
  if ((p<0)||(p>=romw->resc)) return;
  struct romw_res *res=romw->resv+p;
  romw_res_cleanup(res);
  romw->resc--;
  memmove(res,res+1,sizeof(struct romw_res)*(romw->resc-p));
}

/* Set dynamic res members.
 */

int romw_res_set_path(struct romw_res *res,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (res->path) free(res->path);
  res->path=nv;
  res->pathc=srcc;
  return 0;
}

int romw_res_set_name(struct romw_res *res,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (res->name) free(res->name);
  res->name=nv;
  res->namec=srcc;
  return 0;
}

int romw_res_set_serial(struct romw_res *res,const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc))) return -1;
    memcpy(nv,src,srcc);
  }
  if (res->serial) free(res->serial);
  res->serial=nv;
  res->serialc=srcc;
  return 0;
}

int romw_res_handoff_serial(struct romw_res *res,void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  if (res->serial) free(res->serial);
  res->serial=src;
  res->serialc=srcc;
  return 0;
}

/* Sort.
 */
 
static int romw_res_cmp(const void *a,const void *b) {
  const struct romw_res *A=a,*B=b;
  if (A->tid<B->tid) return -1;
  if (A->tid>B->tid) return 1;
  if (A->qual<B->qual) return -1;
  if (A->qual>B->qual) return 1;
  if (A->rid<B->rid) return -1;
  if (A->rid>B->rid) return 1;
  return 0;
}

void romw_sort(struct romw *romw) {
  if (!romw) return;
  qsort(romw->resv,romw->resc,sizeof(struct romw_res),romw_res_cmp);
}

/* Pre-encode cleanup and validation.
 */
 
int romw_validate_preencode(struct romw *romw) {
  if (!romw) {
    fprintf(stderr,"Resource set not initialized.\n");
    return -2;
  }
  int i=0;
  struct romw_res *res=romw->resv;
  while (i<romw->resc) {
  
    // Drop fake and empty records.
    if (!res->tid||!res->rid||!res->serialc) {
      romw_res_cleanup(res);
      romw->resc--;
      memmove(res,res+1,sizeof(struct romw_res)*(romw->resc-i));
      continue;
    }
    
    // Validate sort order.
    if (i) {
      const struct romw_res *prev=res-1;
      int cmp=romw_res_cmp(prev,res);
      if (cmp>0) {
        fprintf(stderr,"Resource list not sorted.\n");
        return -2;
      }
      if (cmp==0) {
        fprintf(stderr,
          "Duplicate resource %d:%d:%d (%s and %s)\n",
          res->tid,res->qual,res->tid,
          prev->path,res->path
        );
        return -2;
      }
    }
    
    // Validate serial length.
    if (res->serialc>=0x100000) {
      fprintf(stderr,
        "Invalid length %d for resource %d:%d:%d (%s), limit %d (1MB-1).\n",
        res->serialc,res->tid,res->qual,res->rid,res->path,0x0fffff
      );
      return -2;
    }
    
    res++;
    i++;
  }
  return 0;
}

/* Encode TOC.
 * Returns expected heap length.
 */
 
static int romw_encode_toc(struct sr_encoder *dst,const struct romw *romw) {
  int heapc=0;
  int tid=1,qual=0,rid=1;
  const struct romw_res *res=romw->resv;
  int i=romw->resc;
  for (;i-->0;res++) {
    if (!res->tid||!res->rid) continue;
    
    // Advance tid?
    if (res->tid<tid) return -1;
    while (res->tid>tid) { // can take up to 3 steps, if tids are very sparse.
      int d=res->tid-tid-1;
      if (d>0x3f) d=0x3f;
      if (sr_encode_u8(dst,d)<0) return -1;
      tid+=d+1;
      qual=0;
      rid=1;
    }
    
    // Advance qual?
    if (res->qual<qual) return -1;
    if (res->qual>qual) { // one step always suffices.
      int d=res->qual-qual-1;
      if (sr_encode_u8(dst,0x40)<0) return -1;
      if (sr_encode_intbe(dst,d,2)<0) return -1;
      qual=res->qual;
      rid=1;
    }
    
    // Advance rid?
    if (res->rid<rid) return -1;
    while (res->rid>rid) { // we don't have an "advance rid" command, must emit blank resources until we reach it.
      if (sr_encode_u8(dst,0x80)<0) return -1; // zero-length resource
      rid++;
    }
    
    /* Resources thru 63 bytes can use the short format.
     * Up to 1 MB exclusive can use long.
     * Beyond that we must fail.
     */
    if (res->serialc<0x40) {
      if (sr_encode_u8(dst,0x80|res->serialc)<0) return -1;
    } else if (res->serialc<0x100000) {
      if (sr_encode_intbe(dst,0xc00000|res->serialc,3)<0) return -1;
    } else {
      return -1;
    }
    rid++;
    heapc+=res->serialc;
  }
  return heapc;
}

/* Encode Heap.
 * Returns emitted length, which you must check against romw_encode_toc().
 */
 
static int romw_encode_heap(struct sr_encoder *dst,const struct romw *romw) {
  int dstc0=dst->c;
  const struct romw_res *res=romw->resv;
  int i=romw->resc;
  for (;i-->0;res++) {
    if (!res->tid||!res->rid) continue;
    if (sr_encode_raw(dst,res->serial,res->serialc)<0) return -1;
  }
  return dst->c-dstc0;
}

/* Encode, outer.
 */

int romw_encode(struct sr_encoder *dst,const struct romw *romw) {
  if (!romw) return -1;
  int hdrp=dst->c;
  if (sr_encode_raw(dst,"\x00\x0egg",4)<0) return -1;
  if (sr_encode_intbe(dst,16,4)<0) return -1; // Header length
  if (sr_encode_raw(dst,"\0\0\0\0\0\0\0\0",8)<0) return -1; // TOC length, Heap length
  
  int tocp=dst->c;
  int expect_heapc=romw_encode_toc(dst,romw);
  if (expect_heapc<0) return -1;
  int tocc=dst->c-tocp;
  
  int actual_heapc=romw_encode_heap(dst,romw);
  if (actual_heapc<0) return -1;
  if (actual_heapc!=expect_heapc) {
    fprintf(stderr,"!!! PANIC !!! %s:%d: expect_heapc=%d actual_heapc=%d\n",__FILE__,__LINE__,expect_heapc,actual_heapc);
    return -1;
  }
  
  uint8_t *h=((uint8_t*)dst->v)+hdrp+8;
  h[0]=tocc>>24;
  h[1]=tocc>>16;
  h[2]=tocc>>8;
  h[3]=tocc;
  h[4]=actual_heapc>>24;
  h[5]=actual_heapc>>16;
  h[6]=actual_heapc>>8;
  h[7]=actual_heapc;
  
  return 0;
}
