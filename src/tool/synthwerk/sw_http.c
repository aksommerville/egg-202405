#include "synthwerk_internal.h"
#include "opt/pcmprintc/pcmprintc.h"
#include "opt/pcmprint/pcmprint.h"

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

/* Compile a sound.
 */
 
static int sw_serve_compile(struct http_xfer *req,struct http_xfer *rsp) {
  int rate=44100; // TODO Configure at argv? Read from a query param?
  const struct sr_encoder *body=http_xfer_get_body(req);
  if (!body) return -1;
  struct pcmprintc *ppc=pcmprintc_new(body->v,body->c,"",0);
  if (!ppc) return -1;
  struct pcmprintc_output output={0};
  if (pcmprintc_next_final(&output,ppc)<0) {
    pcmprintc_del(ppc);
    return http_xfer_set_status(rsp,400,"Failed to compile. Server log may have more detail.");
  }
  struct sr_encoder *dst=http_xfer_get_body(rsp);
  if (!dst) {
    pcmprintc_del(ppc);
    return -1;
  }
  sr_encode_json_object_start(dst,0,0);
  sr_encode_json_int(dst,"rate",4,rate);
  sr_encode_json_base64(dst,"bin",3,output.bin,output.binc);
  struct pcmprint *pp=pcmprint_new(rate,output.bin,output.binc);
  if (pp) {
    while (!pcmprint_update(pp,8192)) ;
    struct pcmprint_pcm *pcm=pcmprint_get_pcm(pp);
    if (pcm) {
      sr_encode_json_preamble(dst,"wave",4);
      sr_encode_u8(dst,'"');
      const float *sample=pcm->v;
      int i=pcm->c;
      for (;i-->0;sample++) {
        int is=(int)((*sample)*32767.0f);
        if (is<-32768) is=-32768;
        else if (is>32767) is=32767;
        char enc[4]={
          "0123456789abcdef"[(is>>12)&15],
          "0123456789abcdef"[(is>> 8)&15],
          "0123456789abcdef"[(is>> 4)&15],
          "0123456789abcdef"[(is    )&15],
        };
        sr_encode_raw(dst,enc,sizeof(enc));
      }
      sr_encode_u8(dst,'"');
    }
    pcmprint_del(pp);
  }
  pcmprintc_del(ppc);
  if (sr_encode_json_end(dst,0)!=0) return -1;
  return http_xfer_set_status(rsp,200,"OK");
}

/* Serve HTTP request, main entry point.
 */
 
int sw_serve(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  return http_dispatch(req,rsp,
    HTTP_METHOD_POST,"/api/compile",sw_serve_compile,
    HTTP_METHOD_GET,"",sw_serve_static
  );
}
