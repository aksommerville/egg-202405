#include "webtm_internal.h"

/* Context.
 */
 
struct webtm_js_context {
  struct sr_encoder *dst; // WEAK
  const char *bootstrap_path; // WEAK
  char **donev; // Path of files already visited.
  int donec,donea;
};

static int webtm_js_compile_file(struct webtm_js_context *ctx,const char *path);

static void webtm_js_context_cleanup(struct webtm_js_context *ctx) {
  if (ctx->donev) {
    while (ctx->donec-->0) free(ctx->donev[ctx->donec]);
    free(ctx->donev);
  }
}

/* Count newlines.
 */
 
static int webtm_lineno(const char *src,int srcc) {
  int lineno=1,srcp=0;
  while (srcp<srcc) {
    if (src[srcp++]==0x0a) lineno++;
  }
  return lineno;
}

/* Identifier characters.
 * Including dot even though it's technically not.
 */
 
static inline int webtm_isident(char ch) {
  return (
    ((ch>='a')&&(ch<='z'))||
    ((ch>='A')&&(ch<='Z'))||
    ((ch>='0')&&(ch<='9'))||
    (ch=='_')||
    (ch=='$')||
    (ch=='.')
  );
}

/* Measure token.
 * Whitespace and comments do count as tokens here.
 */
 
static int webtm_js_measure_token(const char *src,int srcc) {
  if (srcc<1) return 0;
  
  // Whitespace.
  if ((unsigned char)src[0]<=0x20) {
    int tokenc=1;
    while ((tokenc<srcc)&&((unsigned char)src[tokenc]<=0x20)) tokenc++;
    return tokenc;
  }
  
  // Line comment.
  if ((srcc>=2)&&(src[0]=='/')&&(src[1]=='/')) {
    int tokenc=1;
    while ((tokenc<srcc)&&(src[tokenc]!=0x0a)) tokenc++;
    return tokenc;
  }
  
  // Block comment.
  if ((srcc>=2)&&(src[0]=='/')&&(src[1]=='*')) {
    int tokenc=2,stopp=srcc-2;
    for (;;) {
      if (tokenc>stopp) return -1;
      if ((src[tokenc]=='*')&&(src[tokenc+1]=='/')) return tokenc+2;
      tokenc++;
    }
  }
  
  // String.
  if ((src[0]=='"')||(src[0]=='`')||(src[0]=='\'')) {
    int tokenc=1;
    for (;;) {
      if (tokenc>=srcc) return -1;
      if (src[tokenc]==src[0]) return tokenc+1;
      if (src[tokenc]=='\\') tokenc+=2;
      else tokenc+=1;
    }
  }
  
  // Number or identifier.
  if (webtm_isident(src[0])) {
    int tokenc=1;
    while ((tokenc<srcc)&&webtm_isident(src[tokenc])) tokenc++;
    return tokenc;
  }
  
  //TODO Do we need to handle regex literals? They're hard to distinguish from binary divide.
  
  // Anything else, pass it thru one byte at a time.
  return 1;
}

/* Nonzero for space and comments.
 * Also for empty, if that ever comes up.
 */
 
static int webtm_js_is_space(const char *src,int srcc) {
  if (srcc<1) return 1;
  if ((unsigned char)src[0]<=0x20) return 1;
  if (srcc<2) return 0;
  if (src[0]!='/') return 0;
  if (src[1]=='/') return 1;
  if (src[1]=='*') return 1;
  return 0;
}

/* Resolve import path.
 * (child) must be a quoted string and must not contain any escapes.
 */
 
static int webtm_js_resolve_path(char *dst,int dsta,struct webtm_js_context *ctx,const char *parent,const char *child,int childc) {
  if ((childc<2)||(child[0]!=child[childc-1])||((child[0]!='"')&&(child[0]!='\'')&&(child[0]!='`'))) return -1;
  child++;
  childc-=2;
  
  // Measure parent path and strip the basename. Retain the trailing slash.
  int parentc=0,slashp=-1;
  for (;parent[parentc];parentc++) {
    if (parent[parentc]=='/') slashp=parentc;
  }
  if (slashp>=0) parentc=slashp+1;
  
  // Pull dot, double-dot, and leading slashes from the front of (child).
  for (;;) {
    if ((childc>=2)&&!memcmp(child,"./",2)) {
      child+=2;
      childc-=2;
    } else if ((childc>=3)&&!memcmp(child,"../",3)) {
      child+=3;
      childc-=3;
      if (parentc) {
        parentc--;
        while (parentc&&(parent[parentc-1]!='/')) parentc--;
      }
    } else if ((childc>=1)&&(child[0]=='/')) {
      child++;
      childc--;
    } else {
      break;
    }
  }
  
  // Concatenate.
  int dstc=parentc+childc;
  if (dstc<=dsta) {
    memcpy(dst,parent,parentc);
    memcpy(dst+parentc,child,childc);
    if (dstc<dsta) dst[dstc]=0;
  }
  return dstc;
}

/* Parse an import statement and enter imported files as needed.
 */
 
static int webtm_js_import(struct webtm_js_context *ctx,const char *parentpath,const char *src,int srcc) {
  const char *childpath=0;
  int childpathc=0;
  int srcp=0;
  for (;;) {
    const char *token=src+srcp;
    int tokenc=webtm_js_measure_token(src+srcp,srcc-srcp);
    if (tokenc<=0) return -1;
    srcp+=tokenc;
    if (webtm_js_is_space(token,tokenc)) continue;
    
    // Terminate at semicolon.
    if ((tokenc==1)&&(token[0]==';')) break;
    
    // Mostly we don't care about the statement. Just find the child path.
    if ((token[0]=='"')||(token[0]=='\'')||(token[0]=='`')) {
      if (childpath) return -1;
      childpath=token;
      childpathc=tokenc;
    }
  }
  if (!childpath) return -1;
  
  char actualpath[1024];
  int actualpathc=webtm_js_resolve_path(actualpath,sizeof(actualpath),ctx,parentpath,childpath,childpathc);
  if ((actualpathc<1)||(actualpathc>=sizeof(actualpath))) return -1;
  
  int err=webtm_js_compile_file(ctx,actualpath);
  if (err<0) return err;
  return srcp;
}

/* Add one file.
 */
 
static int webtm_js_compile_file(struct webtm_js_context *ctx,const char *path) {
  
  int i=ctx->donec;
  while (i-->0) if (!strcmp(ctx->donev[i],path)) {
    return 0;
  }
  if (ctx->donec>=ctx->donea) {
    int na=ctx->donea+32;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(ctx->donev,sizeof(void*)*na);
    if (!nv) return -1;
    ctx->donev=nv;
    ctx->donea=na;
  }
  if (!(ctx->donev[ctx->donec]=strdup(path))) return -1;
  ctx->donec++;
  
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",path);
    return -2;
  }
  int srcp=0;
  while (srcp<srcc) {
    int srcp0=srcp;
    const char *token=src+srcp;
    int tokenc=webtm_js_measure_token(src+srcp,srcc-srcp);
    if (tokenc<=0) {
      fprintf(stderr,"%s:%d: Tokenization error.\n",path,webtm_lineno(src,srcp));
      free(src);
      return -2;
    }
    srcp+=tokenc;
    
    // Skip all whitespace.
    if (webtm_js_is_space(token,tokenc)) continue;
    
    // Imports get handled special.
    if ((tokenc==6)&&!memcmp(token,"import",6)) {
      int err=webtm_js_import(ctx,path,src+srcp,srcc-srcp);
      if (err<0) {
        if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error resolving import.\n",path,webtm_lineno(src,srcp0));
        return -2;
      }
      srcp+=err;
      continue;
    }
    
    // Remove the word "export".
    if ((tokenc==6)&&!memcmp(token,"export",6)) continue;
    
    // If we have adjacent identifier chars in separate tokens, emit a space.
    // This is common; the tokens presumably were separated by space in the input.
    if (ctx->dst->c&&webtm_isident(((char*)ctx->dst->v)[ctx->dst->c-1])&&webtm_isident(token[0])) {
      if (sr_encode_u8(ctx->dst,' ')<0) return -1;
    }
    
    // Prevent "-->", since this text is going to appear in an HTML comment.
    // It's also a common construction in my Javascript: for (let i=count; i-->0; ) ...
    if ((tokenc==1)&&(token[0]=='>')) {
      if (ctx->dst->c>=2) {
        const char *prev=((char*)ctx->dst->v)+ctx->dst->c-2;
        if ((prev[0]=='-')&&(prev[1]=='-')) {
          if (sr_encode_u8(ctx->dst,' ')<0) return -1;
        }
      }
    }
    
    // And finally, emit the token.
    if (sr_encode_raw(ctx->dst,token,tokenc)<0) return -1;
  }
  free(src);
  // Not technically required, but add a newline between files to avoid monstrously long lines.
  if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
  return 0;
}

/* Main.
 */
 
int webtm_js_compile(struct sr_encoder *dst,const char *path) {
  struct webtm_js_context ctx={
    .dst=dst,
    .bootstrap_path=path,
  };
  int err=webtm_js_compile_file(&ctx,path);
  webtm_js_context_cleanup(&ctx);
  return err;
}
