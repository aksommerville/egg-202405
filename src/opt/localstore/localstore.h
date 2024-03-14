/* localstore.h
 * File-backed key=value store.
 * Aiming to make this a fairly painless substitute for browser localStorage.
 *
 * Encoded format:
 *   u8 keyc, ... key, u16 valuec, ... value, ...
 */
 
#ifndef LOCALSTORE_H
#define LOCALSTORE_H

struct localstore {
  struct localstore_entry {
    int kc,vc;
    char *k;
    char *v;
  } *entryv;
  int entryc,entrya;
  int dirty;
};

void localstore_cleanup(struct localstore *localstore);

/* Values from 'get' are always terminated if not null.
 * If we return zero from get, generally we do not populate (*dstpp).
 */
int localstore_get(const char **dstpp,const struct localstore *localstore,const char *k,int kc);
int localstore_set(struct localstore *localstore,const char *k,int kc,const char *v,int vc);

/* Loading drops any existing content first.
 * Saving writes the file whether dirty or not, then clears the dirty flag.
 * Caller should check dirty flag before saving.
 */
int localstore_load(struct localstore *localstore,const char *path);
int localstore_save(struct localstore *localstore,const char *path);

#endif
