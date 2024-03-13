#ifndef EGGROM_INTERNAL_H
#define EGGROM_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include "opt/romr/romr.h"
#include "opt/romw/romw.h"

extern struct eggrom {
  const char *exename;
  const char *dstpath;
  char command; // [cxt]
  char format;
  struct romw *romw; // -c only
  struct sr_encoder scratch; // Used during compile.
} eggrom;

int eggrom_expand_resources();
int eggrom_assign_missing_ids();
int eggrom_link_resources();

int eggrom_tid_repr(char *dst,int dsta,uint8_t tid);
int eggrom_tid_eval(const char *src,int srcc,int names_only);

int eggrom_qual_repr(char *dst,int dsta,uint8_t tid,uint16_t qual);
int eggrom_qual_eval(const char *src,int srcc,uint8_t tid);

/* Caller should populate (path,serial) and optionally (tid) if known.
 * We populate (name,qual,rid) if we can determine them.
 */
int eggrom_infer_ids(struct romw_res *res);

/* "expand" handlers should create new resources in (eggrom.romw).
 */
int eggrom_string_expand(const char *src,int srcc,uint16_t qual,const char *path);

/* "compile" handlers should put rewritten serial content in (dst).
 * If you leave (dst) untouched, the original (res->serial) will be kept.
 */
int eggrom_metadata_compile(struct sr_encoder *dst,const struct romw_res *res);
int eggrom_image_compile(struct sr_encoder *dst,const struct romw_res *res);
int eggrom_song_compile(struct sr_encoder *dst,const struct romw_res *res);
int eggrom_sound_compile(struct sr_encoder *dst,const struct romw_res *res);
int eggrom_map_compile(struct sr_encoder *dst,const struct romw_res *res);

#endif
