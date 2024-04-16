/* romr.h
 * Read-only ROM file assistant.
 * Ouch, the redundancy of that...
 * But we also have "romw" for writing ROM files at build time.
 */
 
#ifndef ROMR_H
#define ROMR_H

#include <stdint.h>

/* Resource types.
 */
 
#define EGG_TID_metadata 1
#define EGG_TID_wasm 2
#define EGG_TID_js 3
#define EGG_TID_image 4
#define EGG_TID_string 5
#define EGG_TID_song 6
#define EGG_TID_sound 7
#define EGG_TID_map 8

#define EGG_TID_FOR_EACH \
  _(metadata) \
  _(wasm) \
  _(js) \
  _(image) \
  _(string) \
  _(song) \
  _(sound) \
  _(map)

/* Stateless one-time decode.
 * Call (cb) until we finish the file or you return nonzero.
 * (v) points into (src), it's safe to retain as long as (src) is.
 * Files are sorted by nature, so you will always be called in order by (tid,qual,rid).
 */
int romr_for_each(
  const void *src,int srcc,
  int (*cb)(uint8_t tid,uint16_t qual,uint16_t rid,const void *v,int c,void *userdata),
  void *userdata
);

struct romr {
  const void *src;
  int srcc;
  int ownsrc;
  struct romr_bucket {
    uint8_t tid;
    uint16_t qual;
    struct romr_res {
      const void *v;
      int c;
    } *resv; // indexed by (id-1)
    int resc,resa;
  } *bucketv;
  int bucketc,bucketa;
  uint16_t qual_by_tid[256];
};

void romr_cleanup(struct romr *romr);

/* (romr) must be empty when you start decode; initialize to zeroes.
 * Use "borrow" if (src) is going to remain in scope and constant for (romr)'s entire life.
 * "copy" if you can't guarantee that; we'll make a copy of it.
 */
int romr_decode_borrow(struct romr *romr,const void *src,int srcc);
int romr_decode_copy(struct romr *romr,const void *src,int srcc);

/* Put a read-only pointer to this resource at (*dstpp) and return its length.
 * Returns zero if not found.
 */
int romr_get_qualified(void *dstpp,const struct romr *romr,uint8_t tid,uint16_t qual,uint16_t rid);

/* Get a resource using the type's default qualifier, and then try qual zero if that fails.
 */
int romr_get(void *dstpp,const struct romr *romr,uint8_t tid,uint16_t rid);

/* Just so you don't have to remember which types are language-qualified.
 * You're free to touch (romr->qual_by_tid) directly too.
 */
uint16_t romr_get_language(const struct romr *romr);
void romr_set_language(struct romr *romr,uint16_t lang);

/* Enumerate resource IDs for non-empty resources.
 * This is more expensive than it sounds (considerably more so than fetching resources!),
 * because it skips zero-length resources.
 */
void romr_id_by_index(int *tid,int *qual,int *rid,const struct romr *romr,int p);
int romr_for_valid_resources(const struct romr *romr,int (*cb)(int tid,int qual,int rid,void *userdata),void *userdata);

#endif
