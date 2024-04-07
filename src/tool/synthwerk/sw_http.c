#include "synthwerk_internal.h"

/* Guess Content-Type.
 */
 
static const char *sw_guess_content_type(const char *path,int pathc,const void *src,int srcc) {

  /* If we recognize the path suffix, trust it.
   */
  const char *sfxsrc=0;
  int sfxc=0,pathp=0;
  for (;pathp<pathc;pathp++) {
    if (path[pathp]=='/') {
      sfxsrc=0;
      sfxc=0;
    } else if (path[pathp]=='.') {
      sfxsrc=path+pathp+1;
      sfxc=0;
    } else if (sfxsrc) {
      sfxc++;
    }
  }
  char sfx[16];
  if ((sfxc>0)&&(sfxc<=sizeof(sfx))) {
    int i=sfxc; while (i-->0) {
      if ((sfxsrc[i]>='A')&&(sfxsrc[i]<='Z')) sfx[i]=sfxsrc[i]+0x20;
      else sfx[i]=sfxsrc[i];
    }
    switch (sfxc) {
      case 2: {
          if (!memcmp(sfx,"js",2)) return "application/javascript";
        } break;
      case 3: {
          if (!memcmp(sfx,"png",3)) return "image/png";
          if (!memcmp(sfx,"ico",3)) return "image/x-icon";
          if (!memcmp(sfx,"css",3)) return "text/css";
        } break;
      case 4: {
          if (!memcmp(sfx,"html",4)) return "text/html";
        } break;
      case 6: {
          if (!memcmp(src,"eggsnd",6)) return "audio/x-egg-sound";
        } break;
    }
  }
  
  /* If there's an unambiguous signature, go with it.
   */
  if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return "image/png";
  
  /* Anything outside (G0,LF,CR,HT) in the first 256, call it binary. Otherwise text.
   */
  int i=srcc;
  if (i>256) i=256;
  const uint8_t *v=src;
  for (;i-->0;v++) {
    if (*v>=0x7f) return "application/octet-stream";
    if (*v>=0x20) continue;
    if (*v==0x0d) continue;
    if (*v==0x0a) continue;
    if (*v==0x09) continue;
    return "application/octet-stream";
  }
  return "text/plain";
}

/* Get static file.
 */
 
static int sw_serve_static(struct http_xfer *req,struct http_xfer *rsp) {
  const char *htdocs="src/tool/synthwerk/www"; // TODO configurable
  const char *reqpath=0;
  int reqpathc=http_xfer_get_path(&reqpath,req);
  if ((reqpathc<1)||(reqpath[0]!='/')) return http_xfer_set_status(rsp,404,"Not found");
  if (reqpathc==1) {
    reqpath="/index.html";
    reqpathc=11;
  }
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s%.*s",htdocs,reqpathc,reqpath);
  if ((pathc<1)||(pathc>=sizeof(path))) return http_xfer_set_status(rsp,404,"Not found");
  // !!! In real life, we would realpath() and verify it's still under htdocs. !!!
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) return http_xfer_set_status(rsp,404,"Not found");
  sr_encode_raw(http_xfer_get_body(rsp),src,srcc);
  http_xfer_set_header(rsp,"Content-Type",12,sw_guess_content_type(path,pathc,src,srcc),-1);
  return http_xfer_set_status(rsp,200,"OK");
}

/* Serve HTTP request, main entry point.
 */
 
int sw_serve(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  return http_dispatch(req,rsp,
    HTTP_METHOD_GET,"",sw_serve_static
  );
}
