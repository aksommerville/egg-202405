/* romw.h
 * Interface for writing ROM files.
 * Separate from 'romr', because typical consumers only need to read ROM files. What both Rs stand for.
 * romw keeps a large amount of data for each resource, and keeps it loose to facilitate editing.
 */
 
#ifndef ROMW_H
#define ROMW_H

#include <stdint.h>

struct sr_encoder;

struct romw {
  struct romw_res {
    uint8_t tid;
    uint16_t qual;
    uint16_t rid;
    char *path;
    int pathc;
    char *name;
    int namec;
    void *serial;
    int serialc;
  } *resv;
  int resc,resa;
};

void romw_del(struct romw *romw);

struct romw *romw_new();

/* Index in (romw->resv) or -1.
 * The list is not sorted, but fully-qualified IDs are expected to be unique.
 * All searches except 'exact' accept a start position. Zero initially.
 */
int romw_search_exact(const struct romw *romw,uint8_t tid,uint16_t qual,uint16_t rid);
int romw_search_tid(const struct romw *romw,uint8_t tid,int startp);
int romw_search_tid_qual(const struct romw *romw,uint8_t tid,uint16_t qual,int startp);
int romw_search_tid_rid(const struct romw *romw,uint8_t tid,uint16_t rid,int startp);

/* Add a new resource record, initially zeroed.
 * (tid,rid) zero are legal in our list but will not encode.
 */
struct romw_res *romw_res_add(struct romw *romw);

int romw_res_set_path(struct romw_res *res,const char *src,int srcc);
int romw_res_set_name(struct romw_res *res,const char *src,int srcc);
int romw_res_set_serial(struct romw_res *res,const void *src,int srcc); // copies
int romw_res_handoff_serial(struct romw_res *res,void *src,int srcc); // assumes ownership

void romw_res_remove(struct romw *romw,int p);

/* Sort resources by (tid,qual,rid).
 * Must be sorted before encoding.
 */
void romw_sort(struct romw *romw);

/* After sorting, it's wise to call this.
 *  - Drop resources with zero (tid,rid,serialc).
 *  - Validate fully sorted.
 *  - Confirm each (serialc) is within the encodable limit.
 * If we fail, we will log the reason to stderr.
 * This is not mandatory. romw_encode() validates everything it needs to, and fails without logging.
 */
int romw_validate_preencode(struct romw *romw);

/* Produce a full ROM file in memory.
 * Resources with zero tid or rid are quietly ignored.
 * Caller must sort first. We fail if any IDs are duplicated or out of order.
 */
int romw_encode(struct sr_encoder *dst,const struct romw *romw);

#endif
