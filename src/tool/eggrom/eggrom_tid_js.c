#include "eggrom_internal.h"

/* Concatenation context.
 */
 
struct eggrom_js_context {
  struct eggrom_js_res {
    int id; // Made up as we add them, but never changes. Not sorted.
    const char *path; // borrowed from romw
    int pathc;
    const char *src; // borrowed from romw
    int srcc;
    struct eggrom_js_import {
      const char *path; // points into (res->src), ie owned by romw
      int pathc;
      int id; // Resolved ID of the imported file.
    } *importv;
    int importc,importa;
  } *resv;
  int resc,resa;
  struct sr_encoder dst;
};

static void eggrom_js_res_cleanup(struct eggrom_js_res *res) {
  if (res->importv) free(res->importv);
}

static void eggrom_js_context_cleanup(struct eggrom_js_context *ctx) {
  if (ctx->resv) {
    while (ctx->resc-->0) eggrom_js_res_cleanup(ctx->resv+ctx->resc);
    free(ctx->resv);
  }
  sr_encoder_cleanup(&ctx->dst);
}

/* Discover js files in romw and add them to the context.
 */
 
static int eggrom_js_discover_sources(struct eggrom_js_context *ctx) {
  const struct romw_res *rres=eggrom.romw->resv;
  int i=eggrom.romw->resc;
  for (;i-->0;rres++) {
    if (rres->tid!=EGG_TID_js) continue;
    if (ctx->resc>=ctx->resa) {
      int na=ctx->resa+16;
      if (na>INT_MAX/sizeof(struct eggrom_js_res)) return -1;
      void *nv=realloc(ctx->resv,sizeof(struct eggrom_js_res)*na);
      if (!nv) return -1;
      ctx->resv=nv;
      ctx->resa=na;
    }
    struct eggrom_js_res *jres=ctx->resv+ctx->resc++;
    memset(jres,0,sizeof(struct eggrom_js_res));
    jres->path=rres->path;
    jres->pathc=rres->pathc;
    jres->src=rres->serial;
    jres->srcc=rres->serialc;
    jres->id=ctx->resc;
  }
  return 0;
}

/* Return the ID of a file in (ctx) by resolving import path (sfx) against its file (pfx).
 */
 
static int eggrom_js_strip_component(const char *src,int srcc) {
  while (srcc&&(src[srcc-1]=='/')) srcc--;
  while (srcc&&(src[srcc-1]!='/')) srcc--;
  if (!srcc) return -1;
  return srcc;
}
 
static int eggrom_js_resolve_path(struct eggrom_js_context *ctx,const char *pfx,int pfxc,const char *sfx,int sfxc) {
  if ((pfxc=eggrom_js_strip_component(pfx,pfxc))<0) return -1;
  int sfxp=0;
  while (sfxp<sfxc) {
    if (sfx[sfxp]=='/') {
      sfxp++;
      continue;
    }
    const char *component=sfx+sfxp;
    int componentc=0;
    while ((sfxp+componentc<sfxc)&&(sfx[sfxp+componentc]!='/')) componentc++;
    if ((componentc==1)&&(component[0]=='.')) { sfxp+=componentc; continue; }
    if ((componentc==2)&&(component[0]=='.')&&(component[1]=='.')) {
      if ((pfxc=eggrom_js_strip_component(pfx,pfxc))<0) return -1;
      sfxp+=componentc;
      continue;
    }
    break; // Dots may only appear at the beginning of (sfx).
  }
  sfx+=sfxp;
  sfxc-=sfxp;
  // Now the file we're looking for is (pfx) + (sfx) + ".js". (pfx) always ends with a slash.
  if ((pfxc<1)||(pfx[pfxc-1]!='/')) return -1;
  int combinec=pfxc+sfxc;
  const struct eggrom_js_res *res=ctx->resv;
  int i=ctx->resc;
  for (;i-->0;res++) {
    if (res->pathc<combinec) continue;
    if (memcmp(res->path,pfx,pfxc)) continue;
    if (memcmp(res->path+pfxc,sfx,sfxc)) continue;
    const char *term=res->path+combinec;
    int termc=res->pathc-combinec;
    if (
      (termc==0)||
      ((termc==3)&&!memcmp(term,".js",3))
    ) {
      return res->id;
    }
  }
  return -1;
}

/* Read each source file and record every import path.
 * Assumptions (not in line with spec):
 *  - Imports each occupy a single line.
 *  - Each import line contains one quoted string, which is the imported file's relative path.
 *  - Import strings do not contain any escapes.
 *  - Import strings may begin with any count of "." and ".." components, but must not have any in the middle.
 *  - No further imports appear after the first non-import line.
 *  - Imports will not contain block comments before the string.
 *  - No multi-line block comment will contain lines that look like imports.
 */
 
static int eggrom_js_discover_imports(struct eggrom_js_context *ctx) {
  struct eggrom_js_res *res=ctx->resv;
  int i=ctx->resc;
  for (;i-->0;res++) {
    struct sr_decoder decoder={.v=res->src,.c=res->srcc};
    int lineno=0,linec;
    const char *line;
    while ((linec=sr_decode_line(&line,&decoder))>0) {
      lineno++;
      while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
      int stopp=linec-1,searchp=0;
      for (;searchp<stopp;searchp++) {
        if ((line[searchp]=='/')&&(line[searchp+1]=='/')) { linec=searchp; break; }
        if ((line[searchp]=='/')&&(line[searchp+1]=='*')) { linec=searchp; break; }
      }
      while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
      if (!linec) continue;
      if ((linec<7)||memcmp(line,"import ",7)) break; // something that isn't an import or space
      
      int startp=7;
      while ((startp<linec)&&(line[startp]!='\'')&&(line[startp]!='"')) startp++;
      if (startp>=linec) {
        fprintf(stderr,"%s:%d: Expected quoted string in import statement.\n",res->path,lineno);
        return -2;
      }
      stopp=startp+1;
      while ((stopp<linec)&&(line[stopp]!=line[startp])) stopp++;
      if (stopp>=linec) {
        fprintf(stderr,"%s:%d: Malformed import string.\n",res->path,lineno);
        return -2;
      }
      const char *ipath=line+startp+1;
      int ipathc=stopp-startp-1;
      
      if (res->importc>=res->importa) {
        int na=res->importa+8;
        if (na>INT_MAX/sizeof(struct eggrom_js_import)) return -1;
        void *nv=realloc(res->importv,sizeof(struct eggrom_js_import)*na);
        if (!nv) return -1;
        res->importv=nv;
        res->importa=na;
      }
      struct eggrom_js_import *import=res->importv+res->importc++;
      memset(import,0,sizeof(struct eggrom_js_import));
      import->path=ipath;
      import->pathc=ipathc;
      import->id=eggrom_js_resolve_path(ctx,res->path,res->pathc,ipath,ipathc);
      if (import->id<1) {
        fprintf(stderr,"%s:%d: Failed to resolve import '%.*s'\n",res->path,lineno,ipathc,ipath);
        return -2;
      } else if (import->id==res->id) {
        // This isn't necessarily an error, I mean we could still process it. But obviously there has been a mistake.
        fprintf(stderr,"%s:%d: Import '%.*s' resolves to this very file.\n",res->path,lineno,ipathc,ipath);
        return -2;
      }
    }
  }
  return 0;
}

/* Sort resources according to import order.
 * This is not necessarily possible -- circular imports are perfectly legal, as long as they don't refer to each other at the top scope.
 * So we'll operate with a tight panic limit, and stop wherever we end up.
 */
 
static int eggrom_js_add_imports(struct eggrom_js_res *dst,int dstc,struct eggrom_js_context *ctx,const struct eggrom_js_import *import,int importc) {
  for (;importc-->0;import++) {
    struct eggrom_js_res *res=ctx->resv;
    int resi=ctx->resc;
    for (;resi-->0;res++) {
      if (res->id!=import->id) continue;
      res->id=0; // don't let it get picked up as we re-enter
      dstc=eggrom_js_add_imports(dst,dstc,ctx,res->importv,res->importc);
      memcpy(dst+dstc,res,sizeof(struct eggrom_js_res));
      dst[dstc].id=import->id;
      dstc++;
      break;
    }
  }
  return dstc;
}
 
static int eggrom_js_sort_by_import_order(struct eggrom_js_context *ctx) {
  struct eggrom_js_res *nv=malloc(sizeof(struct eggrom_js_res)*ctx->resc);
  if (!nv) return -1;
  int nc=0;
  
  // Remove one from the end of (ctx), recursively add all its imports and blank them from (ctx), until we run out.
  while (ctx->resc>0) {
    ctx->resc--;
    struct eggrom_js_res *res=ctx->resv+ctx->resc;
    if (!res->id) continue; // already visited in a recursive pass
    nc=eggrom_js_add_imports(nv,nc,ctx,res->importv,res->importc);
    memcpy(nv+nc,res,sizeof(struct eggrom_js_res));
    nc++;
  }
  
  free(ctx->resv);
  ctx->resv=nv;
  ctx->resc=nc;
  return 0;
}

/* Combine all source text into (ctx->dst), dropping comments, space, and imports.
 * This is the main thing we're here for.
 */
 
static inline int eggrom_js_isident(char src) {
  if ((src>='a')&&(src<='z')) return 1;
  if ((src>='A')&&(src<='Z')) return 1;
  if ((src>='0')&&(src<='9')) return 1;
  if (src=='_') return 1;
  return 0;
}

static inline int eggrom_js_lineno(const char *src,int srcc) {
  int lineno=1,srcp=0;
  for (;srcp<srcc;srcp++) {
    if (src[srcp]==0x0a) lineno++;
  }
  return lineno;
}
 
static int eggrom_js_reduce_1(struct sr_encoder *dst,const char *src,int srcc,const char *path) {
  int srcp=0;
  while (srcp<srcc) {
  
    // Whitespace.
    if ((unsigned char)src[srcp]<=0x20) {
      srcp++;
      continue;
    }
    
    // Line comment.
    if ((srcp<=srcc-2)&&(src[srcp]=='/')&&(src[srcp+1]=='/')) {
      srcp+=2;
      while ((srcp<srcc)&&(src[srcp]!=0x0a)) srcp++;
      continue;
    }
    
    // Block comment.
    if ((srcp<srcc-2)&&(src[srcp]=='/')&&(src[srcp+1]=='*')) {
      int srcp0=srcp;
      srcp+=2;
      while (1) {
        if (srcp>srcc-2) {
          fprintf(stderr,"%s:%d: Unclosed block comment.\n",path,eggrom_js_lineno(src,srcp0));
          return -2;
        }
        if ((src[srcp]=='*')&&(src[srcp+1]=='/')) {
          srcp+=2;
          break;
        }
        srcp++;
      }
      continue;
    }
    
    // String.
    if ((src[srcp]=='"')||(src[srcp]=='\'')||(src[srcp]=='`')) {
      int srcp0=srcp;
      char quote=src[srcp++];
      while (1) {
        if (srcp>=srcc) {
          fprintf(stderr,"%s:%d: Unclosed string.\n",path,eggrom_js_lineno(src,srcp0));
          return -2;
        }
        if (src[srcp]=='\\') {
          srcp+=2;
        } else {
          if (src[srcp++]==quote) break;
        }
      }
      if (sr_encode_raw(dst,src+srcp0,srcp-srcp0)<0) return -1;
      continue;
    }
    
    // Regex. We can't locate these correctly, because it depends on context (Primary vs Secondary Expression).
    // I'm going to assume that an inline regex will always have an open-paren before it.
    // That is obviously not always true.
    if ((src[srcp]=='/')&&(srcp>0)&&(src[srcp-1]=='(')) {
      int srcp0=srcp++;
      while (1) {
        if (srcp>=srcc) {
          fprintf(stderr,"%s:%d: Unclosed regex.\n",path,eggrom_js_lineno(src,srcp0));
          return -2;
        }
        if (src[srcp]=='/') { srcp++; break; }
        if (src[srcp]=='\\') srcp+=2;
        else srcp++;
      }
      if (sr_encode_raw(dst,src+srcp0,srcp-srcp0)<0) return -1;
      continue;
    }
    
    // Everything else, measure up to whitespace and emit verbatim.
    // Check the last character output -- emit a space if needed to separate identifier chars.
    const char *token=src+srcp;
    int tokenc=0;
    if (eggrom_js_isident(token[0])) {
      while ((srcp<srcc)&&eggrom_js_isident(src[srcp])) { srcp++; tokenc++; }
    } else {
      tokenc=1;
      srcp++;
    }
    
    // Oh wait, except two things: Drop the word "export", and if we see "import", treat it like a line comment.
    if ((tokenc==6)&&!memcmp(token,"export",6)) continue;
    if ((tokenc==6)&&!memcmp(token,"import",6)) {
      while ((srcp<srcc)&&(src[srcp]!=0x0a)) srcp++;
      continue;
    }
    
    if (eggrom_js_isident(token[0])) {
      if ((dst->c>0)&&eggrom_js_isident(((char*)dst->v)[dst->c-1])) {
        if (sr_encode_u8(dst,' ')<0) return -1;
      }
    }
    if (sr_encode_raw(dst,token,tokenc)<0) return -1;
  }
  return 0;
}
 
static int eggrom_js_reduce_and_combine(struct eggrom_js_context *ctx) {
  const struct eggrom_js_res *res=ctx->resv;
  int i=ctx->resc;
  for (;i-->0;res++) {
    int err=eggrom_js_reduce_1(&ctx->dst,res->src,res->srcc,res->path);
    if (err<0) return err;
  }
  return 0;
}

/* Remove all JS resources from romw.
 * !!! Beware, (ctx->resv) will become corrupt !!!
 */
 
static int eggrom_js_remove_sources(struct eggrom_js_context *ctx) {
  int p=eggrom.romw->resc;
  while (p-->0) {
    struct romw_res *res=eggrom.romw->resv+p;
    if (res->tid!=EGG_TID_js) continue;
    romw_res_remove(eggrom.romw,p);
  }
  return 0;
}

/* Add a new resource to romw, from the text we previously generated at (ctx->dst).
 */
 
static int eggrom_js_generate_combined_resource(struct eggrom_js_context *ctx) {
  struct romw_res *res=romw_res_add(eggrom.romw);
  if (!res) return -1;
  res->tid=EGG_TID_js;
  res->qual=0;
  res->rid=1;
  res->serial=ctx->dst.v; // HANDOFF
  res->serialc=ctx->dst.c;
  memset(&ctx->dst,0,sizeof(struct sr_encoder));
  return 0;
}

/* Combine Javascript, in context.
 */
 
static int eggrom_js_combine_inner(struct eggrom_js_context *ctx) {
  int err;
  if ((err=eggrom_js_discover_sources(ctx))<0) return err;
  if (!ctx->resc) return 0;
  if ((err=eggrom_js_discover_imports(ctx))<0) return err;
  if ((err=eggrom_js_sort_by_import_order(ctx))<0) return err;
  if ((err=eggrom_js_reduce_and_combine(ctx))<0) return err;
  if ((err=eggrom_js_remove_sources(ctx))<0) return err;
  if ((err=eggrom_js_generate_combined_resource(ctx))<0) return err;
  return 0;
}

/* Combine Javascript resources, main entry point.
 */
 
int eggrom_js_combine() {
  struct eggrom_js_context ctx={0};
  int err=eggrom_js_combine_inner(&ctx);
  eggrom_js_context_cleanup(&ctx);
  return err;
}
