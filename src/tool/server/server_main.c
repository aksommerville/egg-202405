/* server_main.c
 * Tiny HTTP server designed to run as part of Egg's development environment.
 */
 
#include "server_internal.h"
#include <signal.h>
#include <unistd.h>

struct server server={0};

/* Signal.
 */
 
static void server_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(server.sigc)>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals.\n",server.exename);
        exit(1);
      } break;
  }
}

/* Guess content type.
 * Always returns something sensible.
 */
 
static const char *server_guess_content_type(const char *path,const uint8_t *src,int srcc) {
  
  // Take (path)'s word for it, if it expresses an opinion.
  char sfx[16];
  int sfxc=0;
  {
    const char *sfxsrc=0;
    int sfxsrcc=0,pathp=0;
    for (;path[pathp];pathp++) {
      if (path[pathp]=='/') {
        sfxsrc=0;
        sfxsrcc=0;
      } else if (path[pathp]=='.') {
        sfxsrc=path+pathp+1;
        sfxsrcc=0;
      } else if (sfxsrc) {
        sfxsrcc++;
      }
    }
    if (sfxsrcc<=sizeof(sfx)) {
      for (;sfxc<sfxsrcc;sfxc++) {
        if ((sfxsrc[sfxc]>='A')&&(sfxsrc[sfxc]<='Z')) sfx[sfxc]=sfxsrc[sfxc]+0x20;
        else sfx[sfxc]=sfxsrc[sfxc];
      }
    }
  }
  switch (sfxc) {
    case 1: switch (sfx[0]) {
        case 'c': return "text/plain";
        case 'h': return "text/plain";
        case 'm': return "text/plain";
        case 's': return "text/plain";
      } break;
    case 2: {
        if (!memcmp(sfx,"js",2)) return "text/javascript";//TODO is it "application"?
      } break;
    case 3: {
        if (!memcmp(sfx,"css",3)) return "text/css";
        if (!memcmp(sfx,"htm",3)) return "text/html";
        if (!memcmp(sfx,"png",3)) return "image/png";
        if (!memcmp(sfx,"gif",3)) return "image/gif";
        if (!memcmp(sfx,"bmp",3)) return "image/bmp";//not sure of this one
        if (!memcmp(sfx,"ico",3)) return "image/x-icon";
        if (!memcmp(sfx,"jpg",3)) return "image/jpeg";
        if (!memcmp(sfx,"xml",3)) return "application/xml";
        if (!memcmp(sfx,"txt",3)) return "text/plain";
        if (!memcmp(sfx,"mid",3)) return "audio/midi";
        if (!memcmp(sfx,"wav",3)) return "audio/wav";//TODO
        if (!memcmp(sfx,"cxx",3)) return "text/plain";
        if (!memcmp(sfx,"egg",3)) return "application/octet-stream";
      } break;
    case 4: {
        if (!memcmp(sfx,"html",4)) return "text/html";
        if (!memcmp(sfx,"jpeg",4)) return "image/jpeg";
      } break;
    case 5: {
        if (!memcmp(sfx,"rlead",5)) return "image/x-rlead";//TODO what's the right syntax for vendor custom types?
      } break;
  }
  
  // We could look for signatures and such, but why bother. Just name the files right in the first place.
  // If the suffix didn't match, call it text/plain or application/octet-stream based on the first 256 bytes.
  int p=256;
  if (p>srcc) p=srcc;
  while (p-->0) {
    if (src[p]>=0x20) continue;
    if (src[p]==0x0d) continue;
    if (src[p]==0x0a) continue;
    if (src[p]==0x09) continue;
    return "application/octet-stream";
  }
  return "text/plain";
}

/* Serve a regular file verbatim.
 */
 
static int server_serve_file(struct http_xfer *req,struct http_xfer *rsp,const char *path) {
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) return http_xfer_set_status(rsp,404,"Not found");
  if (sr_encode_raw(http_xfer_get_body(rsp),serial,serialc)<0) {
    free(serial);
    return -1;
  }
  http_xfer_add_header(rsp,"Content-Type",12,server_guess_content_type(path,serial,serialc),-1);
  free(serial);
  return http_xfer_set_status(rsp,200,"OK");
}

/* Run make on a file, then serve it.
 * It's OK to block while make runs; we are kind of a single-user deal.
 * Obviously that would be unacceptable in a more realistic setting.
 */
 
static int server_make_then_serve_file(struct http_xfer *req,struct http_xfer *rsp,const char *path) {
  int i=0; for (;path[i];i++) {
    // We're not secure at all, but let's not be ridiculous...
    if ((path[i]=='\'')||(path[i]=='\\')) return http_xfer_set_status(rsp,404,"Not found");
  }
  int pathc=i;
  char *cwd=getcwd(0,0);
  if (!cwd) return http_xfer_set_status(rsp,500,"Internal server error");
  int cwdc=0; while (cwd[cwdc]) cwdc++;
  if ((cwdc<pathc)&&!memcmp(cwd,path,cwdc)) {
    free(cwd);
    const char *lpath=path+cwdc;
    while (lpath[0]=='/') lpath++;
    char cmd[1024];
    int cmdc=snprintf(cmd,sizeof(cmd),"make '%s'",lpath);
    if ((cmdc<1)||(cmdc>=sizeof(cmd))) return http_xfer_set_status(rsp,404,"Not found");
    int err=system(cmd);
    if (err) {
      //TODO Would be nicer to capture make's output and send that as the response body here (still with a 5xx status).
      // If we're doing that, might as well also make it non-blocking.
      return http_xfer_set_status(rsp,599,"Make failed, please see server log.");
    }
  } else {
    free(cwd);
  }
  return server_serve_file(req,rsp,path);
}

/* Return real local path to a file based on htdocs and request path.
 * We do all the validation; if we return (0<n<dsta), go ahead and serve it.
 */
 
static int server_htdocs_path(char *dst,int dsta,const char *root,const char *reqpath,int reqpathc) {
  int rootc=0; while (root[rootc]) rootc++;
  int dstc=snprintf(dst,dsta,"%.*s/%.*s",rootc,root,reqpathc,reqpath);
  if ((dstc<1)||(dstc>=dsta)) return -1;
  char *real=realpath(dst,0);
  if (!real) return -1;
  int realc=0; while (real[realc]) realc++;
  if (realc>=dsta) {
    free(real);
    return -1;
  }
  memcpy(dst,real,realc+1);
  free(real);
  // In real life, we should have realpath'd the roots at startup.
  // Then we could ensure that the final path starts with root, and reject if not.
  // We're a dev server so I'm not going to worry about it.
  // BEWARE: We potentially can serve any file readable by this process, even if outside htdocs.
  return realc;
}

static int server_makeabledir_path(char *dst,int dsta,const char *root,const char *reqpath,int reqpathc) {
  int rootc=0; while (root[rootc]) rootc++;
  int dstc=snprintf(dst,dsta,"%.*s/%.*s",rootc,root,reqpathc,reqpath);
  if ((dstc<1)||(dstc>=dsta)) return -1;
  char *real=realpath(dst,0);
  if (!real) return -1;
  int realc=0; while (real[realc]) realc++;
  if (realc>=dsta) {
    free(real);
    return -1;
  }
  memcpy(dst,real,realc+1);
  free(real);
  return realc;
}

/* Serve HTTP GET, static files. May invoke make first.
 */
 
static int server_cb_serve_static(struct http_xfer *req,struct http_xfer *rsp) {
  const char *reqpath=0;
  int reqpathc=http_xfer_get_path(&reqpath,req);
  while ((reqpathc>1)&&(reqpath[reqpathc-1]=='/')) reqpathc--;
  if ((reqpathc==1)&&(reqpath[0]=='/')) {
    reqpath="/index.html";
    reqpathc=11;
  }
  int i=0; for (;i<server.htdocsc;i++) {
    char path[1024];
    int pathc=server_htdocs_path(path,sizeof(path),server.htdocsv[i],reqpath,reqpathc);
    if ((pathc>0)&&(pathc<sizeof(path))) {
      return server_serve_file(req,rsp,path);
    }
  }
  for (i=0;i<server.makeabledirc;i++) {
    char path[1024];
    int pathc=server_makeabledir_path(path,sizeof(path),server.makeabledirv[i],reqpath,reqpathc);
    if ((pathc>0)&&(pathc<sizeof(path))) {
      return server_make_then_serve_file(req,rsp,path);
    }
  }
  return http_xfer_set_status(rsp,404,"Not found");
}

/* GET /list-games
 * Return a JSON array of strings, all the "*.egg" files in our makeable directories.
 * One level deep only.
 */
 
static int server_cb_list_games_1(const char *path,const char *base,char ftype,void *userdata) {
  struct sr_encoder *dst=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype!='f') return 0;
  int basec=0; while (base[basec]) basec++;
  if ((basec<=4)||memcmp(base+basec-4,".egg",4)) return 0;
  if (sr_encode_json_string(dst,0,0,base,basec)<0) return -1;
  return 0;
}
 
static int server_cb_list_games(struct http_xfer *req,struct http_xfer *rsp) {
  struct sr_encoder *dst=http_xfer_get_body(rsp);
  sr_encode_json_array_start(dst,0,0);
  int i=0; for (;i<server.makeabledirc;i++) {
    dir_read(server.makeabledirv[i],server_cb_list_games_1,dst);
  }
  if (sr_encode_json_end(dst,0)<0) return -1;
  http_xfer_add_header(rsp,"Content-Type",12,"application/json",16);
  return http_xfer_set_status(rsp,200,"OK");
}

/* Dispatch HTTP requests.
 */
 
static int server_cb_serve(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  return http_dispatch(req,rsp,
    HTTP_METHOD_GET,"/list-games",server_cb_list_games,
    HTTP_METHOD_GET,"/**",server_cb_serve_static
  );
}

/* Command line.
 */
 
static int server_arg_port(const char *src) {
  int v=0;
  if ((sr_int_eval(&v,src,-1)<2)||(v<1)||(v>0xffff)) return -1;
  server.port=v;
  return 0;
}

static int server_arg_htdocs(const char *src) {
  if (server.htdocsc>=server.htdocsa) {
    int na=server.htdocsa+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(server.htdocsv,sizeof(void*)*na);
    if (!nv) return -1;
    server.htdocsv=nv;
    server.htdocsa=na;
  }
  if (!(server.htdocsv[server.htdocsc]=strdup(src))) return -1;
  server.htdocsc++;
  return 0;
}

static int server_arg_makeable_dir(const char *src) {
  if (server.makeabledirc>=server.makeabledira) {
    int na=server.makeabledira+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(server.makeabledirv,sizeof(void*)*na);
    if (!nv) return -1;
    server.makeabledirv=nv;
    server.makeabledira=na;
  }
  if (!(server.makeabledirv[server.makeabledirc]=strdup(src))) return -1;
  server.makeabledirc++;
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  server.exename="server";
  if ((argc>=1)&&argv[0]&&argv[0][0]) server.exename=argv[0];
  server.port=8080;
  int argi=1; for (;argi<argc;argi++) {
    const char *arg=argv[argi];
    if (!memcmp(arg,"--port=",7)) {
      if (server_arg_port(arg+7)<0) return 1;
    } else if (!memcmp(arg,"--htdocs=",9)) {
      if (server_arg_htdocs(arg+9)<0) return 1;
    } else if (!memcmp(arg,"--makeable-dir=",15)) {
      if (server_arg_makeable_dir(arg+15)<0) return 1;
    } else {
      server.htdocsc=server.makeabledirc=0; // force Usage
      break;
    }
  }
  if (!server.htdocsc&&!server.makeabledirc) {
    fprintf(stderr,"Usage: %s [--port=8080] [--htdocs=DIR...] [--makeable-dir=DIR...]\n",server.exename);
    return 1;
  }
  
  struct http_context_delegate http_delegate={
    .cb_serve=server_cb_serve,
  };
  if (!(server.http=http_context_new(&http_delegate))) return 1;
  if (http_listen(server.http,1,server.port)<0) {
    fprintf(stderr,"%s: Failed to open TCP server on port %d\n",server.exename,server.port);
    return 1;
  }
  
  fprintf(stderr,"%s: Serving on %d. SIGINT to quit.\n",server.exename,server.port);
  int status=0;
  while (!server.sigc) {
    if (http_update(server.http,100)<0) {
      status=1;
      break;
    }
  }
  
  http_context_del(server.http);
  return status;
}
