#include "eggrom_internal.h"

/* Friendly names for tid.
 */
 
int eggrom_tid_repr(char *dst,int dsta,uint8_t tid) {
  const char *src=0;
  switch (tid) {
    #define _(tag) case EGG_TID_##tag: src=#tag; break;
    EGG_TID_FOR_EACH
    #undef _
  }
  if (src) {
    int srcc=0; while (src[srcc]) srcc++;
    if (srcc<=dsta) {
      memcpy(dst,src,srcc);
      if (srcc<dsta) dst[srcc]=0;
    }
    return srcc;
  }
  return sr_decuint_repr(dst,dsta,tid,0);
}

int eggrom_tid_eval(const char *src,int srcc,int names_only) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return EGG_TID_##tag;
  EGG_TID_FOR_EACH
  #undef _
  if (names_only) return -1;
  int v;
  if ((sr_int_eval(&v,src,srcc)>=2)&&(v>=0)&&(v<0x100)) return v;
  return -1;
}

/* Friendly names for qualifier.
 * These could take (tid) into account, but for now we don't.
 */

int eggrom_qual_repr(char *dst,int dsta,uint8_t tid,uint16_t qual) {
  uint8_t a=qual>>8,b=qual;
  if ((a>='a')&&(a<='z')&&(b>='a')&&(b<='z')) {
    if (dsta>=2) {
      dst[0]=a;
      dst[1]=b;
      if (dsta>2) dst[2]=0;
    }
    return 2;
  }
  return sr_decuint_repr(dst,dsta,qual,0);
}

int eggrom_qual_eval(const char *src,int srcc,uint8_t tid) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc==2)&&(src[0]>='a')&&(src[0]<='z')&&(src[1]>='a')&&(src[1]<='z')) {
    return (src[0]<<8)|src[1];
  }
  int v;
  if ((sr_int_eval(&v,src,srcc)>=2)&&(v>=0)&&(v<0x10000)) return v;
  return -1;
}

/* Infer (tid) from path suffix.
 * Input must be lowercase.
 */
 
static uint8_t eggrom_tid_from_sfx(const char *sfx,int sfxc) {
  switch (sfxc) {
    case 1: switch (sfx[0]) {
        // one-letter suffix... probably won't have any
      } break;
    case 2: {
        if (!memcmp(sfx,"js",2)) return EGG_TID_js;
      } break;
    case 3: {
        if (!memcmp(sfx,"png",3)) return EGG_TID_image;
        if (!memcmp(sfx,"gif",3)) return EGG_TID_image;
        if (!memcmp(sfx,"jpg",3)) return EGG_TID_image;
        if (!memcmp(sfx,"qoi",3)) return EGG_TID_image;
        if (!memcmp(sfx,"bmp",3)) return EGG_TID_image;
        if (!memcmp(sfx,"ico",3)) return EGG_TID_image;
        if (!memcmp(sfx,"mid",3)) return EGG_TID_song;
        if (!memcmp(sfx,"wav",3)) return EGG_TID_sound;
        // It's tempting to include "txt" => string, but I think "txt" could appear in lots of other ways.
      } break;
    case 4: {
        if (!memcmp(sfx,"wasm",4)) return EGG_TID_wasm;
        if (!memcmp(sfx,"jpeg",4)) return EGG_TID_image;
      } break;
    case 5: {
        if (!memcmp(sfx,"rlead",5)) return EGG_TID_image;
      } break;
  }
  return 0;
}

/* Infer (tid) from raw serial content.
 */
 
static uint8_t eggrom_tid_from_serial(const uint8_t *src,int srcc) {
  if ((srcc>=4)&&!memcmp(src,"\0asm",4)) return EGG_TID_wasm;
  if ((srcc>=7)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return EGG_TID_image;
  if ((srcc>=6)&&!memcmp(src,"GIF87a",6)) return EGG_TID_image;
  if ((srcc>=6)&&!memcmp(src,"GIF89a",6)) return EGG_TID_image;
  if ((srcc>=4)&&!memcmp(src,"qoif",4)) return EGG_TID_image;
  if ((srcc>=2)&&!memcmp(src,"\xbb\xad",2)) return EGG_TID_image; // rlead
  if ((srcc>=8)&&!memcmp(src,"MThd\0\0\0\6",8)) return EGG_TID_song;
  if ((srcc>=12)&&!memcmp(src,"RIFF",4)&&!memcmp(src+8,"WAVE",4)) return EGG_TID_sound;
  return 0;
}

/* Infer (tid,rid,qual,name) from (path,serial,tid?).
 */
 
int eggrom_infer_ids(struct romw_res *res) {
  if (!res) return -1;
  
  /* Split dirname and basename, and grab (tid) and (qual) from directory names if present.
   */
  const char *path=res->path;
  int dirc=0;
  const char *base=path;
  int basec=0;
  if (path) {
    // Directories may name a (tid), and below that may name a (qual).
    int tid_from_path=0;
    int qual_from_path=0;
    int pathp=0;
    for (;pathp<res->pathc;pathp++) {
      if (path[pathp]=='/') {
        int subtid=eggrom_tid_eval(base,basec,1);
        if (subtid>0) {
          tid_from_path=subtid;
          qual_from_path=0;
        } else if (tid_from_path) {
          int subqual=eggrom_qual_eval(base,basec,tid_from_path);
          if (subqual>0) {
            qual_from_path=subqual;
          }
        }
        base=path+pathp+1;
        basec=0;
      } else {
        basec++;
      }
    }
    if (tid_from_path&&!res->tid) {
      res->tid=tid_from_path;
    }
    if (qual_from_path&&!res->qual) {
      res->qual=qual_from_path;
    }
  }
  
  /* A basename of "metadata" and no determinable type, becomes metadata:0:1.
   */
  if (!res->tid&&(basec==8)&&!memcmp(base,"metadata",8)) {
    res->tid=EGG_TID_metadata;
    res->qual=0;
    res->rid=1;
    return 0;
  }
  
  /* If we haven't got a (tid) yet, check the suffix and serial, maybe we can guess it.
   * Suffix takes precedence.
   */
  if (!res->tid) {
    char sfx[16];
    int basep=0;
    while ((basep<basec)&&(base[basep++]!='.')) ;
    int sfxc=basec-basep;
    if (sfxc>sizeof(sfx)) {
      sfxc=0;
    } else {
      int i=sfxc; while (i-->0) {
        sfx[i]=base[basep+i];
        if ((sfx[i]>='A')&&(sfx[i]<='Z')) sfx[i]+=0x20;
      }
    }
    if (!(res->tid=eggrom_tid_from_sfx(sfx,sfxc))) {
      if (!(res->tid=eggrom_tid_from_serial(res->serial,res->serialc))) {
        return -1;
      }
    }
  }
  
  /* When the type is string and qualifier unset, expect basename to be a language code.
   * That becomes the qualifier, and (rid) remains zero -- This is a multi-resource file.
   */
  if ((res->tid==EGG_TID_string)&&!res->qual&&(basec==2)&&(base[0]>='a')&&(base[0]<='z')&&(base[1]>='a')&&(base[1]<='z')) {
    res->qual=(base[0]<<8)|base[1];
    
  /* Other resources we expect basename to be: ID [-NAME] [.FORMAT]
   * Or: NAME [.FORMAT]
   */
  } else if (basec>0) {
    int basep=0;
    if ((base[basep]>='0')&&(base[basep]<='9')) {
      res->rid=0;
      while ((basep<basec)&&(base[basep]>='0')&&(base[basep]<='9')) {
        int digit=base[basep++]-'0';
        if (res->rid>6553) return -1;
        res->rid*=10;
        if (res->rid>65535-digit) return -1;
        res->rid+=digit;
      }
      if ((basep<basec)&&(base[basep]=='-')) {
        basep++;
        const char *name=base+basep;
        int namec=0;
        while ((basep<basec)&&(base[basep]!='.')) { basep++; namec++; }
        if (romw_res_set_name(res,name,namec)<0) return -1;
      }
      if ((basep<basec)&&(base[basep]!='.')) return -1;
    } else {
      const char *name=base+basep;
      int namec=0;
      while ((basep<basec)&&(base[basep]!='.')) { basep++; namec++; }
      if (romw_res_set_name(res,name,namec)<0) return -1;
    }
  }
  return 0;
}
