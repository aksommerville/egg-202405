/* egg_native_export.c
 * All JS and Wasm hooks live here.
 * Most of the Public API is implemented here.
 * Elsewhere:
 *   egg_native_net.c -- http and ws
 *   egg_native_event.c -- egg_event_next,egg_event_enable
 *   egg_native_input.c -- input
 */

#include "egg_native_internal.h"
#include "quickjs.h"
#include "wasm_export.h"
#include "opt/strfmt/strfmt.h"
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include "GLES2/gl2.h"

#define JSASSERTARGC(expect) if (argc!=expect) return JS_ThrowTypeError(ctx,"%s requires %d arguments, got %d",__func__,expect,argc);
#define JSASSERTARGRANGE(lo,hi) if ((argc<lo)||(argc>hi)) return JS_ThrowTypeError(ctx,"%s requires %d..%d arguments, got %d",__func__,lo,hi,argc);

static void egg_js_dummy_free(JSRuntime *rt,void *opaque,void *ptr) {}
static void egg_js_basic_free(JSRuntime *rt,void *opaque,void *ptr) { if (ptr) free(ptr); }

/* Video driver.
 */
 
void egg_video_get_size(int *w,int *h) {
  if (egg.hostio->video) {
    if (w) *w=egg.hostio->video->w;
    if (h) *h=egg.hostio->video->h;
  } else {
    if (w) *w=0;
    if (h) *h=0;
  }
}

/* Synthesizer.
 */
 
void egg_audio_play_song(int songid,int force,int repeat) {
  fprintf(stderr,"%s %d %d %d\n",__func__,songid,force,repeat);//TODO
}

void egg_audio_play_sound(int soundid,double trim,double pan) {
  fprintf(stderr,"%s %d %f %f\n",__func__,soundid,trim,pan);//TODO
}

int egg_audio_get_playhead() {
  fprintf(stderr,"%s\n",__func__);//TODO
  return -1;
}

/* Resource store.
 */
 
int egg_res_get(void *dst,int dsta,int tid,int qual,int rid) {
  const void *src=0;
  int srcc=romr_get_qualified(&src,&egg.romr,tid,qual,rid);
  if (srcc<0) return 0;
  if (srcc<=dsta) memcpy(dst,src,srcc);
  return srcc;
}

void egg_res_id_by_index(int *tid,int *qual,int *rid,int index) {
  if (tid) *tid=0;
  if (qual) *qual=0;
  if (rid) *rid=0;
  if (index<0) return;
  const struct romr_bucket *bucket=egg.romr.bucketv;
  int bucketi=egg.romr.bucketc;
  for (;bucketi-->0;bucket++) {
    if (index<bucket->resc) {
      *tid=bucket->tid;
      *qual=bucket->qual;
      *rid=index+1;
      return;
    }
    index-=bucket->resc;
  }
}

/* Persistent store.
 */
 
int egg_store_set(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  return localstore_set(&egg.localstore,k,kc,v,vc);
}

int egg_store_get(char *dst,int dsta,const char *k,int kc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  const char *src=0;
  int srcc=localstore_get(&src,&egg.localstore,k,kc);
  if (srcc<0) srcc=0;
  if (srcc<=dsta) {
    memcpy(dst,src,srcc);
    if (srcc<dsta) dst[srcc]=0;
  }
  return srcc;
}

int egg_store_key_by_index(char *dst,int dsta,int index) {
  const char *k=0;
  int kc=0;
  if ((index>=0)&&(index<egg.localstore.entryc)) {
    k=egg.localstore.entryv[index].k;
    kc=egg.localstore.entryv[index].kc;
  }
  if (kc<=dsta) {
    memcpy(dst,k,kc);
    if (kc<dsta) dst[kc]=0;
  }
  return kc;
}

/* Real time.
 */
 
double egg_time_real() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

void egg_time_get(int *year,int *month,int *day,int *hour,int *minute,int *second,int *milli) {
  time_t now=0;
  time(&now);
  struct tm tm={0};
  localtime_r(&now,&tm);
  if (year) *year=1900+tm.tm_year;
  if (month) *month=1+tm.tm_mon;
  if (day) *day=tm.tm_mday;
  if (hour) *hour=tm.tm_hour;
  if (minute) *minute=tm.tm_min;
  if (second) *second=tm.tm_sec;
  if (milli) {
    struct timeval tv={0};
    gettimeofday(&tv,0);
    *milli=tv.tv_usec/1000;
  }
}

/* User langauges.
 */

int egg_get_user_languages(int *dst,int dsta) {
  /* POSIX systems typically have LANG as the single preferred locale, which starts with a language code.
   * There can also be LANGUAGE, which is multiple language codes separated by colons.
   */
  int dstc=0;
  const char *src;
  if (src=getenv("LANG")) {
    if ((src[0]>='a')&&(src[0]<='z')&&(src[1]>='a')&&(src[1]<='z')) {
      if (dstc<dsta) dst[dstc++]=(src[0]<<8)|src[1];
    }
  }
  if (dstc>=dsta) return dstc;
  if (src=getenv("LANGUAGE")) {
    int srcp=0;
    while (src[srcp]&&(dstc<dsta)) {
      const char *token=src+srcp;
      int tokenc=0;
      while (src[srcp]&&(src[srcp++]!=':')) tokenc++;
      if ((tokenc>=2)&&(token[0]>='a')&&(token[0]<='z')&&(token[1]>='a')&&(token[1]<='z')) {
        int lang=(token[0]<<8)|token[1];
        int already=0,i=dstc;
        while (i-->0) if (dst[i]==lang) { already=1; break; }
        if (!already) dst[dstc++]=lang;
      }
    }
  }
  //TODO I'm sure there are other mechanisms for MacOS and Windows. Find those.
  return dstc;
}

/* Termination.
 */
 
void egg_request_termination() {
  egg.terminate=1;
}

int egg_is_terminable() {
  return 1;
}

/* Log.
 * This is the troublemaker call, it's the only variadic function we have.
 * So the native, JS, and Wasm implementations are all kind of involved.
 */
 
void egg_log(const char *fmt,...) {
  if (!fmt||!fmt[0]) return;
  va_list vargs;
  va_start(vargs,fmt);
  char tmp[256];
  int tmpc=0,panic=100;
  struct strfmt strfmt={.fmt=fmt};
  while ((tmpc<sizeof(tmp))&&!strfmt_is_finished(&strfmt)&&(panic-->0)) {
    int err=strfmt_more(tmp+tmpc,sizeof(tmp)-tmpc,&strfmt);
    if (err>0) tmpc+=err;
    switch (strfmt.awaiting) {
      case 'i': { int v=va_arg(vargs,int); strfmt_provide_i(&strfmt,v); } break;
      case 'l': { int64_t v=va_arg(vargs,int64_t); strfmt_provide_l(&strfmt,v); } break;
      case 'f': { double v=va_arg(vargs,double); strfmt_provide_f(&strfmt,v); } break;
      case 'p': { void *v=va_arg(vargs,void*); strfmt_provide_p(&strfmt,v); } break;
    }
  }
  fprintf(stderr,"GAME: %.*s\n",tmpc,tmp);
}

/* js: log
 */

static JSValue egg_js_log(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  if (argc<1) return JS_NULL;
  const char *fmt=JS_ToCString(ctx,argv[0]);
  if (!fmt) return JS_NULL;
  
  int argp=1;
  char tmp[256];
  int tmpc=0,panic=100;
  struct strfmt strfmt={.fmt=fmt};
  while ((tmpc<sizeof(tmp))&&!strfmt_is_finished(&strfmt)&&(panic-->0)) {
    int err=strfmt_more(tmp+tmpc,sizeof(tmp)-tmpc,&strfmt);
    if (err>0) tmpc+=err;
    switch (strfmt.awaiting) {
      case 'i': {
          int32_t v=0;
          if (argp<argc) JS_ToInt32(ctx,&v,argv[argp++]);
          strfmt_provide_i(&strfmt,v);
        } break;
      case 'l': {
          int64_t v=0;
          if (argp<argc) JS_ToInt64(ctx,&v,argv[argp++]);
          strfmt_provide_l(&strfmt,v); 
        } break;
      case 'f': {
          double v=0.0;
          if (argp<argc) JS_ToFloat64(ctx,&v,argv[argp++]);
          strfmt_provide_f(&strfmt,v);
        } break;
      case 'p': {
          const char *str=0;
          if (argp<argc) str=JS_ToCString(ctx,argv[argp++]);
          strfmt_provide_p(&strfmt,str);
          if (str) JS_FreeCString(ctx,str);
        } break;
    }
  }
  fprintf(stderr,"GAME: %.*s\n",tmpc,tmp);
  
  JS_FreeCString(ctx,fmt);
  return JS_NULL;
}

/* wasm: log
 */
 
static void egg_wasm_log(wasm_exec_env_t ee,const char *fmt,uint32_t vargs) {
  if (!fmt) return;
  char tmp[256];
  int tmpc=0,panic=100;
  struct strfmt strfmt={.fmt=fmt};
  while ((tmpc<sizeof(tmp))&&!strfmt_is_finished(&strfmt)&&(panic-->0)) {
    int err=strfmt_more(tmp+tmpc,sizeof(tmp)-tmpc,&strfmt);
    if (err>0) tmpc+=err;
    switch (strfmt.awaiting) {
      case 'i': {
          int32_t *vp=wamr_validate_pointer(egg.wamr,1,vargs,4);
          vargs+=4;
          strfmt_provide_i(&strfmt,vp?*vp:0);
        } break;
      case 'l': {
          if (vargs&4) vargs+=4;
          int64_t *vp=wamr_validate_pointer(egg.wamr,1,vargs,8);
          vargs+=8;
          strfmt_provide_l(&strfmt,vp?*vp:0); 
        } break;
      case 'f': {
          if (vargs&4) vargs+=4;
          double *vp=wamr_validate_pointer(egg.wamr,1,vargs,8);
          vargs+=8;
          strfmt_provide_f(&strfmt,vp?*vp:0.0);
        } break;
      case 'p': {
          uint32_t *pointer=wamr_validate_pointer(egg.wamr,1,vargs,4);
          vargs+=4;
          const char *string=pointer?wamr_validate_pointer(egg.wamr,1,*pointer,0):0;
          strfmt_provide_p(&strfmt,string);
        } break;
    }
  }
  fprintf(stderr,"GAME: %.*s\n",tmpc,tmp);
}

/* egg_event_next
 */

static JSValue egg_js_event_next(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSValue dst=JS_NewArray(ctx);
  int dstp=0;
  for (;;) {
    struct egg_event buf[16];
    int bufc=egg_event_next(buf,sizeof(buf)/sizeof(buf[0]));
    if (bufc<1) break;
    const struct egg_event *srcevent=buf;
    for (;bufc-->0;srcevent++) {
      JSValue dstevent=JS_NewObject(ctx);
      JS_SetPropertyStr(ctx,dstevent,"eventType",JS_NewInt32(ctx,srcevent->type));
      JS_SetPropertyStr(ctx,dstevent,"v0",JS_NewInt32(ctx,srcevent->v[0]));
      JS_SetPropertyStr(ctx,dstevent,"v1",JS_NewInt32(ctx,srcevent->v[1]));
      JS_SetPropertyStr(ctx,dstevent,"v2",JS_NewInt32(ctx,srcevent->v[2]));
      JS_SetPropertyStr(ctx,dstevent,"v3",JS_NewInt32(ctx,srcevent->v[3]));
      JS_SetPropertyUint32(ctx,dst,dstp++,dstevent);
    }
  }
  return dst;
}

static int egg_wasm_event_next(wasm_exec_env_t ee,struct egg_event *eventv,int eventa) {
  return egg_event_next(eventv,eventa);
}

/* egg_event_enable
 */

static JSValue egg_js_event_enable(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(2)
  int32_t type=0,state=0;
  JS_ToInt32(ctx,&type,argv[0]);
  JS_ToInt32(ctx,&state,argv[1]);
  int result=egg_event_enable(type,state);
  return JS_NewInt32(ctx,result);
}

static int egg_wasm_event_enable(wasm_exec_env_t ee,int evttype,int evtstate) {
  return egg_event_enable(evttype,evtstate);
}

/* egg_input_device_get_name
 */
 
static JSValue egg_js_input_device_get_name(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t devid=0;
  JS_ToInt32(ctx,&devid,argv[0]);
  char tmp[256];
  int tmpc=egg_input_device_get_name(tmp,sizeof(tmp),devid);
  if ((tmpc<0)||(tmpc>=sizeof(tmp))) tmpc=0;
  return JS_NewStringLen(ctx,tmp,tmpc);
}

static int egg_wasm_input_device_get_name(wasm_exec_env_t ee,char *dst,int dsta,int devid) {
  return egg_input_device_get_name(dst,dsta,devid);
}

/* egg_input_device_get_ids
 */
 
static JSValue egg_js_input_device_get_ids(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t devid=0;
  JS_ToInt32(ctx,&devid,argv[0]);
  int vid=0,pid=0,version=0;
  egg_input_device_get_ids(&vid,&pid,&version,devid);
  JSValue result=JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,result,"vid",JS_NewInt32(ctx,vid));
  JS_SetPropertyStr(ctx,result,"pid",JS_NewInt32(ctx,pid));
  JS_SetPropertyStr(ctx,result,"version",JS_NewInt32(ctx,version));
  return result;
}

static void egg_wasm_input_device_get_ids(wasm_exec_env_t ee,int *vid,int *pid,int *version,int devid) {
  return egg_input_device_get_ids(vid,pid,version,devid);
}

/* egg_input_device_get_button
 */
 
static JSValue egg_js_input_device_get_button(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(2)
  int32_t devid=0,index=-1;
  JS_ToInt32(ctx,&devid,argv[0]);
  JS_ToInt32(ctx,&index,argv[1]);
  int btnid=0,hidusage=0,lo=0,hi=0,value=0;
  egg_input_device_get_button(&btnid,&hidusage,&lo,&hi,&value,devid,index);
  if (!btnid) return JS_NULL;
  JSValue result=JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,result,"btnid",JS_NewInt32(ctx,btnid));
  JS_SetPropertyStr(ctx,result,"hidusage",JS_NewInt32(ctx,hidusage));
  JS_SetPropertyStr(ctx,result,"lo",JS_NewInt32(ctx,lo));
  JS_SetPropertyStr(ctx,result,"hi",JS_NewInt32(ctx,hi));
  JS_SetPropertyStr(ctx,result,"value",JS_NewInt32(ctx,value));
  return result;
}
 
static void egg_wasm_input_device_get_button(wasm_exec_env_t ee,int *btnid,int *hidusage,int *lo,int *hi,int *value,int devid,int index) {
  egg_input_device_get_button(btnid,hidusage,lo,hi,value,devid,index);
}

/* egg_input_device_disconnect
 */
 
static JSValue egg_js_input_device_disconnect(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t devid=0;
  JS_ToInt32(ctx,&devid,argv[0]);
  egg_input_device_disconnect(devid);
  return JS_NULL;
}
 
static void egg_wasm_input_device_disconnect(wasm_exec_env_t ee,int devid) {
  egg_input_device_disconnect(devid);
}

/* egg_video_get_size
 */
 
static JSValue egg_js_video_get_size(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  int w=0,h=0;
  egg_video_get_size(&w,&h);
  JSValue dst=JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx,dst,0,JS_NewInt32(ctx,w));
  JS_SetPropertyUint32(ctx,dst,1,JS_NewInt32(ctx,h));
  return dst;
}
 
static void egg_wasm_video_get_size(wasm_exec_env_t ee,int *w,int *h) {
  egg_video_get_size(w,h);
}

/* egg_audio_play_song
 */
 
static JSValue egg_js_audio_play_song(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(3)
  int32_t songid=0,force=0,repeat=0;
  JS_ToInt32(ctx,&songid,argv[0]);
  JS_ToInt32(ctx,&force,argv[1]);
  JS_ToInt32(ctx,&repeat,argv[2]);
  egg_audio_play_song(songid,force,repeat);
  return JS_NULL;
}
 
static void egg_wasm_audio_play_song(wasm_exec_env_t ee,int songid,int force,int repeat) {
  egg_audio_play_song(songid,force,repeat);
}

/* egg_audio_play_sound
 */
 
static JSValue egg_js_audio_play_sound(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(3)
  int32_t soundid=0;
  double trim=0.0,pan=0.0;
  JS_ToInt32(ctx,&soundid,argv[0]);
  JS_ToFloat64(ctx,&trim,argv[1]);
  JS_ToFloat64(ctx,&pan,argv[2]);
  egg_audio_play_sound(soundid,trim,pan);
  return JS_NULL;
}
 
static void egg_wasm_audio_play_sound(wasm_exec_env_t ee,int soundid,double trim,double pan) {
  egg_audio_play_sound(soundid,trim,pan);
}

/* egg_audio_get_playhead
 */
 
static JSValue egg_js_audio_get_playhead(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  return JS_NewInt32(ctx,egg_audio_get_playhead());
}
 
static int egg_wasm_audio_get_playhead(wasm_exec_env_t ee) {
  return egg_audio_get_playhead();
}

/* egg_res_get
 */
 
static JSValue egg_js_res_get(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(3)
  int32_t tid=0,qual=0,rid=0;
  JS_ToInt32(ctx,&tid,argv[0]);
  JS_ToInt32(ctx,&qual,argv[1]);
  JS_ToInt32(ctx,&rid,argv[2]);
  if ((tid&~0xff)||(qual&~0xffff)||(rid&~0xffff)) return JS_NULL;
  // Not going thru egg_res_get(), to avoid an extra copy or length detection.
  const void *src=0;
  int srcc=romr_get_qualified(&src,&egg.romr,tid,qual,rid);
  if (srcc<1) return JS_NULL;
  return JS_NewArrayBufferCopy(ctx,src,srcc);
}
 
static int egg_wasm_res_get(wasm_exec_env_t ee,void *dst,int dsta,int tid,int qual,int rid) {
  return egg_res_get(dst,dsta,tid,qual,rid);
}

/* egg_res_id_by_index
 */
 
static JSValue egg_js_res_id_by_index(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t p=-1;
  JS_ToInt32(ctx,&p,argv[0]);
  int tid=0,qual=0,rid=0;
  egg_res_id_by_index(&tid,&qual,&rid,p);
  JSValue result=JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,result,"tid",JS_NewInt32(ctx,tid));
  JS_SetPropertyStr(ctx,result,"qual",JS_NewInt32(ctx,qual));
  JS_SetPropertyStr(ctx,result,"rid",JS_NewInt32(ctx,rid));
  return result;
}
 
static void egg_wasm_res_id_by_index(wasm_exec_env_t ee,int *tid,int *qual,int *rid,int index) {
  egg_res_id_by_index(tid,qual,rid,index);
}

/* egg_store_set
 */
 
static JSValue egg_js_store_set(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(2)
  size_t kc=0,vc=0;
  const char *k=JS_ToCStringLen(ctx,&kc,argv[0]);
  const char *v=JS_ToCStringLen(ctx,&vc,argv[1]);
  int err=egg_store_set(k,kc,v,vc);
  if (k) JS_FreeCString(ctx,k);
  if (v) JS_FreeCString(ctx,v);
  return JS_NewInt32(ctx,err);
}
 
static int egg_wasm_store_set(wasm_exec_env_t ee,const char *k,int kc,const char *v,int vc) {
  return egg_store_set(k,kc,v,vc);
}

/* get_store_get
 */
 
static JSValue egg_js_store_get(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  size_t kc=0;
  const char *k=JS_ToCStringLen(ctx,&kc,argv[0]);
  if (!k) return JS_NewInt32(ctx,0);
  int bufa=1024;
  char *buf=malloc(bufa);
  if (!buf) {
    JS_FreeCString(ctx,k);
    return JS_NewInt32(ctx,0);
  }
  for (;;) {
    int bufc=egg_store_get(buf,bufa,k,kc);
    if (bufc<=bufa) {
      JSValue result=JS_NewStringLen(ctx,buf,bufc);
      free(buf);
      JS_FreeCString(ctx,k);
      return result;
    }
    void *nv=realloc(buf,bufa=bufc+1);
    if (!nv) {
      free(buf);
      JS_FreeCString(ctx,k);
      return JS_NewInt32(ctx,0);
    }
  }
}
 
static int egg_wasm_store_get(wasm_exec_env_t ee,char *dst,int dsta,const char *k,int kc) {
  return egg_store_get(dst,dsta,k,kc);
}

/* egg_store_key_by_index
 */
 
static JSValue egg_js_store_key_by_index(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t p=-1;
  JS_ToInt32(ctx,&p,argv[0]);
  char tmp[256];
  int tmpc=egg_store_key_by_index(tmp,sizeof(tmp),p);
  if ((tmpc<1)||(tmpc>sizeof(tmp))) tmpc=0;
  return JS_NewStringLen(ctx,tmp,tmpc);
}
 
static int egg_wasm_store_key_by_index(wasm_exec_env_t ee,char *dst,int dsta,int index) {
  return egg_store_key_by_index(dst,dsta,index);
}

/* egg_http_request
 */
 
static JSValue egg_js_http_request(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGRANGE(2,3)
  const char *method=JS_ToCString(ctx,argv[0]);
  const char *path=JS_ToCString(ctx,argv[1]);
  const void *body=0;
  size_t bodyc=0;
  int freebody=0;
  if (argc>=3) {
    if (JS_IsString(argv[2])) {
      if (body=JS_ToCStringLen(ctx,&bodyc,argv[2])) freebody=1;
    } else {
      body=JS_GetArrayBuffer(ctx,&bodyc,argv[2]);
    }
  }
  int reqid=egg_http_request(method,path,body,bodyc);
  if (method) JS_FreeCString(ctx,method);
  if (path) JS_FreeCString(ctx,path);
  if (freebody) JS_FreeCString(ctx,body);
  return JS_NewInt32(ctx,reqid);
}
 
static int egg_wasm_http_request(wasm_exec_env_t ee,const char *method,const char *url,const void *body,int bodyc) {
  return egg_http_request(method,url,body,bodyc);
}

/* egg_http_get_status
 */
 
static JSValue egg_js_http_get_status(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t reqid=-1;
  JS_ToInt32(ctx,&reqid,argv[0]);
  return JS_NewInt32(ctx,egg_http_get_status(reqid));
}
 
static int egg_wasm_http_get_status(wasm_exec_env_t ee,int reqid) {
  return egg_http_get_status(reqid);
}

/* egg_http_get_header
 */
 
static JSValue egg_js_http_get_header(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(2)
  int32_t reqid=-1;
  size_t kc=0;
  JS_ToInt32(ctx,&reqid,argv[0]);
  const char *k=JS_ToCStringLen(ctx,&kc,argv[1]);
  char tmp[1024];
  int tmpc=egg_http_get_header(tmp,sizeof(tmp),reqid,k,kc);
  if ((tmpc<0)||tmpc>sizeof(tmp)) tmpc=0;
  if (k) JS_FreeCString(ctx,k);
  return JS_NewStringLen(ctx,tmp,tmpc);
}
 
static int egg_wasm_http_get_header(wasm_exec_env_t ee,char *dst,int dsta,int reqid,const char *k,int kc) {
  return egg_http_get_header(dst,dsta,reqid,k,kc);
}

/* egg_http_get_body
 */
 
static JSValue egg_js_http_get_body(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t reqid=-1;
  JS_ToInt32(ctx,&reqid,argv[0]);
  if (reqid<1) return JS_NULL;
  int bufa=8192;
  void *buf=malloc(bufa);
  if (!buf) return JS_NULL;
  for (;;) {
    int bufc=egg_http_get_body(buf,bufa,reqid);
    if (bufc<0) bufc=0;
    if (bufc<=bufa) {
      return JS_NewArrayBuffer(ctx,buf,bufc,egg_js_basic_free,0,0);
    }
    void *nv=realloc(buf,bufa=bufc+1);
    if (!nv) {
      free(buf);
      return JS_NULL;
    }
    buf=nv;
  }
}
 
static int egg_wasm_http_get_body(wasm_exec_env_t ee,void *dst,int dsta,int reqid) {
  return egg_http_get_body(dst,dsta,reqid);
}

/* egg_ws_connect
 */
 
static JSValue egg_js_ws_connect(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  const char *url=JS_ToCString(ctx,argv[0]);
  if (!url) return JS_NewInt32(ctx,-1);
  int wsid=egg_ws_connect(url);
  JS_FreeCString(ctx,url);
  return JS_NewInt32(ctx,wsid);
}
 
static int egg_wasm_ws_connect(wasm_exec_env_t ee,const char *url) {
  return egg_ws_connect(url);
}

/* egg_ws_disconnect
 */
 
static JSValue egg_js_ws_disconnect(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t wsid=-1;
  JS_ToInt32(ctx,&wsid,argv[0]);
  egg_ws_disconnect(wsid);
  return JS_NULL;
}
 
static void egg_wasm_ws_disconnect(wasm_exec_env_t ee,int wsid) {
  egg_ws_disconnect(wsid);
}

/* egg_ws_get_message
 */
 
static JSValue egg_js_ws_get_message(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(2)
  int32_t wsid=-1,msgid=-1;
  JS_ToInt32(ctx,&wsid,argv[0]);
  JS_ToInt32(ctx,&msgid,argv[1]);
  int bufa=1024;
  char *buf=malloc(bufa);
  if (!buf) return JS_NewStringLen(ctx,"",0);
  for (;;) {
    int bufc=egg_ws_get_message(buf,bufa,wsid,msgid);
    if (bufc<=bufa) {
      JSValue result=JS_NewStringLen(ctx,buf,bufc);
      free(buf);
      return result;
    }
    void *nv=realloc(buf,bufa=bufc+1);
    if (!nv) {
      free(buf);
      return JS_NewStringLen(ctx,"",0);
    }
    buf=nv;
  }
}
 
static int egg_wasm_ws_get_message(wasm_exec_env_t ee,void *dst,int dsta,int wsid,int msgid) {
  return egg_ws_get_message(dst,dsta,wsid,msgid);
}

/* egg_ws_send
 */
 
static JSValue egg_js_ws_send(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(3)
  int32_t wsid=-1,opcode=1;
  JS_ToInt32(ctx,&wsid,argv[0]);
  JS_ToInt32(ctx,&opcode,argv[1]);
  size_t srcc=0;
  const char *src=JS_ToCStringLen(ctx,&srcc,argv[2]);
  egg_ws_send(wsid,opcode,src,srcc);
  if (src) JS_FreeCString(ctx,src);
  return JS_NULL;
}
 
static void egg_wasm_ws_send(wasm_exec_env_t ee,int wsid,int opcode,const void *v,int c) {
  egg_ws_send(wsid,opcode,v,c);
}

/* egg_time_real
 */
 
static JSValue egg_js_time_real(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  return JS_NewFloat64(ctx,egg_time_real());
}
 
static double egg_wasm_time_real(wasm_exec_env_t ee) {
  return egg_time_real();
}

/* egg_time_get
 */
 
static JSValue egg_js_time_get(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  int year=0,month=0,day=0,hour=0,minute=0,second=0,milli=0;
  egg_time_get(&year,&month,&day,&hour,&minute,&second,&milli);
  JSValue result=JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,result,"year",JS_NewInt32(ctx,year));
  JS_SetPropertyStr(ctx,result,"month",JS_NewInt32(ctx,month));
  JS_SetPropertyStr(ctx,result,"day",JS_NewInt32(ctx,day));
  JS_SetPropertyStr(ctx,result,"hour",JS_NewInt32(ctx,hour));
  JS_SetPropertyStr(ctx,result,"minute",JS_NewInt32(ctx,minute));
  JS_SetPropertyStr(ctx,result,"second",JS_NewInt32(ctx,second));
  JS_SetPropertyStr(ctx,result,"milli",JS_NewInt32(ctx,milli));
  return result;
}
 
static void egg_wasm_time_get(wasm_exec_env_t ee,int *year,int *month,int *day,int *hour,int *minute,int *second,int *milli) {
  egg_time_get(year,month,day,hour,minute,second,milli);
}

/* egg_get_user_languages
 */
 
static JSValue egg_js_get_user_languages(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  int tmp[32];
  int tmpc=egg_get_user_languages(tmp,sizeof(tmp)/sizeof(tmp[0]));
  if ((tmpc<0)||(tmpc>sizeof(tmp))) tmpc=0;
  JSValue result=JS_NewArray(ctx);
  int i=0; for (;i<tmpc;i++) {
    JS_SetPropertyUint32(ctx,result,i,JS_NewInt32(ctx,tmp[i]));
  }
  return result;
}
 
static int egg_wasm_get_user_languages(wasm_exec_env_t ee,int *dst,int dsta) {
  return egg_get_user_languages(dst,dsta);
}

/* egg_request_termination
 */
 
static JSValue egg_js_request_termination(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  egg_request_termination();
  return JS_NULL;
}

static void egg_wasm_request_termination(wasm_exec_env_t ee) {
  egg_request_termination();
}

/* egg_is_terminable
 * Our answer is always one; don't bother calling the inner implementation.
 */
 
static JSValue egg_js_is_terminable(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  return JS_NewInt32(ctx,1);
}

static int egg_wasm_is_terminable(wasm_exec_env_t ee) {
  return 1;
}

/* The entire OpenGL ES 2.0 API, with god damn wasm_exec_env_t in front.
 *!!!TODO!!! Probably some of these formal parameters are the wrong size.
 */
 
static void egg_glActiveTexture(wasm_exec_env_t ee,GLenum texture) {
  glActiveTexture(texture);
}

static void egg_glAttachShader(wasm_exec_env_t ee,GLuint program, GLuint shader) {
  glAttachShader(program,shader);
}

static void egg_glBindAttribLocation(wasm_exec_env_t ee,GLuint program, GLuint index, const GLchar *name) {
  glBindAttribLocation(program, index,name);
}

static void egg_glBindBuffer(wasm_exec_env_t ee,GLenum target, GLuint buffer) {
  glBindBuffer(target, buffer);
}

static void egg_glBindFramebuffer(wasm_exec_env_t ee,GLenum target, GLuint framebuffer) {
  glBindFramebuffer( target, framebuffer);
}

static void egg_glBindRenderbuffer(wasm_exec_env_t ee,GLenum target, GLuint renderbuffer) {
  glBindRenderbuffer(target, renderbuffer);
}

static void egg_glBindTexture(wasm_exec_env_t ee,GLenum target, GLuint texture) {
  glBindTexture(target, texture);
}

static void egg_glBlendColor(wasm_exec_env_t ee,GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
  glBlendColor(red, green, blue, alpha);
}

static void egg_glBlendEquation(wasm_exec_env_t ee,GLenum mode) {
  glBlendEquation(mode);
}

static void egg_glBlendEquationSeparate(wasm_exec_env_t ee,GLenum modeRGB, GLenum modeAlpha) {
  glBlendEquationSeparate( modeRGB, modeAlpha);
}

static void egg_glBlendFunc(wasm_exec_env_t ee,GLenum sfactor, GLenum dfactor) {
  glBlendFunc(sfactor,  dfactor);
}

static void egg_glBlendFuncSeparate(wasm_exec_env_t ee,GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
  glBlendFuncSeparate( sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

static void egg_glBufferData(wasm_exec_env_t ee,GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
  glBufferData(target, size, data, usage);
}

static void egg_glBufferSubData(wasm_exec_env_t ee,GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
  glBufferSubData( target, offset, size,data);
}

static GLenum egg_glCheckFramebufferStatus(wasm_exec_env_t ee,GLenum target) {
  return glCheckFramebufferStatus(target);
}

static void egg_glClear(wasm_exec_env_t ee,GLbitfield mask) {
  glClear(mask);
}

static void egg_glClearColor(wasm_exec_env_t ee,GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
  glClearColor( red, green, blue, alpha);
}

static void egg_glClearDepthf(wasm_exec_env_t ee,GLfloat d) {
  glClearDepthf(d);
}

static void egg_glClearStencil(wasm_exec_env_t ee,GLint s) {
  glClearStencil( s);
}

static void egg_glColorMask(wasm_exec_env_t ee,GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
  glColorMask( red, green, blue, alpha);
}

static void egg_glCompileShader(wasm_exec_env_t ee,GLuint shader) {
  glCompileShader(shader);
}

static void egg_glCompressedTexImage2D(wasm_exec_env_t ee,GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data) {
  glCompressedTexImage2D( target, level,  internalformat, width, height, border, imageSize,data);
}

static void egg_glCompressedTexSubImage2D(wasm_exec_env_t ee,GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data) {
  glCompressedTexSubImage2D(target,  level, xoffset, yoffset, width, height, format, imageSize, data);
}

static void egg_glCopyTexImage2D(wasm_exec_env_t ee,GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
  glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

static void egg_glCopyTexSubImage2D(wasm_exec_env_t ee,GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
  glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

static GLuint egg_glCreateProgram(wasm_exec_env_t ee) {
  return glCreateProgram();
}

static GLuint egg_glCreateShader(wasm_exec_env_t ee,GLenum type) {
  return glCreateShader( type);
}

static void egg_glCullFace(wasm_exec_env_t ee,GLenum mode) {
  glCullFace(mode);
}

static void egg_glDeleteBuffers(wasm_exec_env_t ee,GLsizei n, const GLuint *buffers) {
  glDeleteBuffers(n,buffers);
}

static void egg_glDeleteFramebuffers(wasm_exec_env_t ee,GLsizei n, const GLuint *framebuffers) {
  glDeleteFramebuffers( n,framebuffers);
}

static void egg_glDeleteProgram(wasm_exec_env_t ee,GLuint program) {
  glDeleteProgram(program);
}

static void egg_glDeleteRenderbuffers(wasm_exec_env_t ee,GLsizei n, const GLuint *renderbuffers) {
  glDeleteRenderbuffers( n,renderbuffers);
}

static void egg_glDeleteShader(wasm_exec_env_t ee,GLuint shader) {
  glDeleteShader(shader);
}

static void egg_glDeleteTextures(wasm_exec_env_t ee,GLsizei n, const GLuint *textures) {
  glDeleteTextures( n,textures);
}

static void egg_glDepthFunc(wasm_exec_env_t ee,GLenum func) {
  glDepthFunc(func);
}

static void egg_glDepthMask(wasm_exec_env_t ee,GLboolean flag) {
  glDepthMask(flag);
}

static void egg_glDepthRangef(wasm_exec_env_t ee,GLfloat n, GLfloat f) {
  glDepthRangef(n, f);
}

static void egg_glDetachShader(wasm_exec_env_t ee,GLuint program, GLuint shader) {
  glDetachShader( program, shader);
}

static void egg_glDisable(wasm_exec_env_t ee,GLenum cap) {
  glDisable(cap);
}

static void egg_glDisableVertexAttribArray(wasm_exec_env_t ee,GLuint index) {
  glDisableVertexAttribArray( index);
}

static void egg_glDrawArrays(wasm_exec_env_t ee,GLenum mode, GLint first, GLsizei count) {
  glDrawArrays(mode, first, count);
}

static void egg_glDrawElements(wasm_exec_env_t ee,GLenum mode, GLsizei count, GLenum type, const void *indices) {
  glDrawElements(mode, count, type,indices);
}

static void egg_glEnable(wasm_exec_env_t ee,GLenum cap) {
  glEnable(cap);
}

static void egg_glEnableVertexAttribArray(wasm_exec_env_t ee,GLuint index) {
  glEnableVertexAttribArray( index);
}

static void egg_glFinish(wasm_exec_env_t ee) {
  glFinish();
}

static void egg_glFlush(wasm_exec_env_t ee) {
  glFlush();
}

static void egg_glFramebufferRenderbuffer(wasm_exec_env_t ee,GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
  glFramebufferRenderbuffer( target, attachment,  renderbuffertarget,  renderbuffer);
}

static void egg_glFramebufferTexture2D(wasm_exec_env_t ee,GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
  glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

static void egg_glFrontFace(wasm_exec_env_t ee,GLenum mode) {
  glFrontFace(mode);
}

static void egg_glGenBuffers(wasm_exec_env_t ee,GLsizei n, GLuint *buffers) {
  glGenBuffers( n, buffers);
}

static void egg_glGenerateMipmap(wasm_exec_env_t ee,GLenum target) {
  glGenerateMipmap(target);
}

static void egg_glGenFramebuffers(wasm_exec_env_t ee,GLsizei n, GLuint *framebuffers) {
  glGenFramebuffers( n,framebuffers);
}

static void egg_glGenRenderbuffers(wasm_exec_env_t ee,GLsizei n, GLuint *renderbuffers) {
  glGenRenderbuffers( n,renderbuffers);
}

static void egg_glGenTextures(wasm_exec_env_t ee,GLsizei n, GLuint *textures) {
  glGenTextures(n,textures);
}

static void egg_glGetActiveAttrib(wasm_exec_env_t ee,GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {
  glGetActiveAttrib( program, index, bufSize,length,size,type,name);
}

static void egg_glGetActiveUniform(wasm_exec_env_t ee,GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {
  glGetActiveUniform(program, index, bufSize, length, size, type, name);
}

static void egg_glGetAttachedShaders(wasm_exec_env_t ee,GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders) {
  glGetAttachedShaders( program,  maxCount,count,shaders);
}

static GLint egg_glGetAttribLocation(wasm_exec_env_t ee,GLuint program, const GLchar *name) {
  return glGetAttribLocation(program, name);
}

static void egg_glGetBooleanv(wasm_exec_env_t ee,GLenum pname, GLboolean *data) {
  glGetBooleanv( pname,data);
}

static void egg_glGetBufferParameteriv(wasm_exec_env_t ee,GLenum target, GLenum pname, GLint *params) {
  glGetBufferParameteriv( target, pname,params);
}

static GLenum egg_glGetError(wasm_exec_env_t ee) {
  return glGetError();
}

static void egg_glGetFloatv(wasm_exec_env_t ee,GLenum pname, GLfloat *data) {
  glGetFloatv( pname,data);
}

static void egg_glGetFramebufferAttachmentParameteriv(wasm_exec_env_t ee,GLenum target, GLenum attachment, GLenum pname, GLint *params) {
  glGetFramebufferAttachmentParameteriv( target, attachment, pname,params);
}

static void egg_glGetIntegerv(wasm_exec_env_t ee,GLenum pname, GLint *data) {
  glGetIntegerv(pname, data);
}

static void egg_glGetProgramiv(wasm_exec_env_t ee,GLuint program, GLenum pname, GLint *params) {
  glGetProgramiv( program,  pname,params);
}

static void egg_glGetProgramInfoLog(wasm_exec_env_t ee,GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
  glGetProgramInfoLog(program, bufSize,length,infoLog);
}

static void egg_glGetRenderbufferParameteriv(wasm_exec_env_t ee,GLenum target, GLenum pname, GLint *params) {
  glGetRenderbufferParameteriv(target, pname,params);
}

static void egg_glGetShaderiv(wasm_exec_env_t ee,GLuint shader, GLenum pname, GLint *params) {
  glGetShaderiv(shader, pname, params);
}

static void egg_glGetShaderInfoLog(wasm_exec_env_t ee,GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
  glGetShaderInfoLog( shader, bufSize,length,infoLog);
}

static void egg_glGetShaderPrecisionFormat(wasm_exec_env_t ee,GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision) {
  glGetShaderPrecisionFormat( shadertype, precisiontype,range,precision);
}

static void egg_glGetShaderSource(wasm_exec_env_t ee,GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source) {
  glGetShaderSource(shader, bufSize, length, source);
}

static const GLubyte *egg_glGetString (wasm_exec_env_t ee,GLenum name) {
  return glGetString(name);
}

static void egg_glGetTexParameterfv(wasm_exec_env_t ee,GLenum target, GLenum pname, GLfloat *params) {
  glGetTexParameterfv( target, pname,params);
}

static void egg_glGetTexParameteriv(wasm_exec_env_t ee,GLenum target, GLenum pname, GLint *params) {
  glGetTexParameteriv(target, pname, params);
}

static void egg_glGetUniformfv(wasm_exec_env_t ee,GLuint program, GLint location, GLfloat *params) {
  glGetUniformfv(program, location,params);
}

static void egg_glGetUniformiv(wasm_exec_env_t ee,GLuint program, GLint location, GLint *params) {
  glGetUniformiv(program, location, params);
}

static GLint egg_glGetUniformLocation(wasm_exec_env_t ee,GLuint program, const GLchar *name) {
  return glGetUniformLocation( program,name);
}

static void egg_glGetVertexAttribfv(wasm_exec_env_t ee,GLuint index, GLenum pname, GLfloat *params) {
  glGetVertexAttribfv( index, pname,params);
}

static void egg_glGetVertexAttribiv(wasm_exec_env_t ee,GLuint index, GLenum pname, GLint *params) {
  glGetVertexAttribiv(index, pname, params);
}

static void *egg_singleGetVertexAttribPointerv(wasm_exec_env_t ee,GLuint index, GLenum pname) {
  void *result=0;
  glGetVertexAttribPointerv( index, pname,&result);
  return result;
}

static void egg_glHint(wasm_exec_env_t ee,GLenum target, GLenum mode) {
  glHint(target, mode);
}

static GLboolean egg_glIsBuffer(wasm_exec_env_t ee,GLuint buffer) {
  return glIsBuffer( buffer);
}

static GLboolean egg_glIsEnabled(wasm_exec_env_t ee,GLenum cap) {
  return glIsEnabled( cap);
}

static GLboolean egg_glIsFramebuffer(wasm_exec_env_t ee,GLuint framebuffer) {
  return glIsFramebuffer( framebuffer);
}

static GLboolean egg_glIsProgram(wasm_exec_env_t ee,GLuint program) {
  return glIsProgram(program);
}

static GLboolean egg_glIsRenderbuffer(wasm_exec_env_t ee,GLuint renderbuffer) {
  return glIsRenderbuffer( renderbuffer);
}

static GLboolean egg_glIsShader(wasm_exec_env_t ee,GLuint shader) {
  return glIsShader( shader);
}

static GLboolean egg_glIsTexture(wasm_exec_env_t ee,GLuint texture) {
  return glIsTexture( texture);
}

static void egg_glLineWidth(wasm_exec_env_t ee,GLfloat width) {
  glLineWidth( width);
}

static void egg_glLinkProgram(wasm_exec_env_t ee,GLuint program) {
  glLinkProgram( program);
}

static void egg_glPixelStorei(wasm_exec_env_t ee,GLenum pname, GLint param) {
  glPixelStorei( pname, param);
}

static void egg_glPolygonOffset(wasm_exec_env_t ee,GLfloat factor, GLfloat units) {
  glPolygonOffset( factor, units);
}

static void egg_glReadPixels(wasm_exec_env_t ee,GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) {
  glReadPixels(x, y, width, height, format, type,pixels);
}

static void egg_glReleaseShaderCompiler(wasm_exec_env_t ee) {
  glReleaseShaderCompiler();
}

static void egg_glRenderbufferStorage(wasm_exec_env_t ee,GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  glRenderbufferStorage(target, internalformat, width, height);
}

static void egg_glSampleCoverage(wasm_exec_env_t ee,GLfloat value, GLboolean invert) {
  glSampleCoverage( value, invert);
}

static void egg_glScissor(wasm_exec_env_t ee,GLint x, GLint y, GLsizei width, GLsizei height) {
  glScissor(x, y, width, height);
}

static void egg_glShaderBinary(wasm_exec_env_t ee,GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length) {
  glShaderBinary( count,shaders, binaryFormat,binary,length);
}

static void egg_singleShaderSource(wasm_exec_env_t ee,GLuint shader, GLsizei count, const GLchar *string, const GLint length) {
  glShaderSource(shader, count, &string, &length);
}

static void egg_glStencilFunc(wasm_exec_env_t ee,GLenum func, GLint ref, GLuint mask) {
  glStencilFunc( func, ref, mask);
}

static void egg_glStencilFuncSeparate(wasm_exec_env_t ee,GLenum face, GLenum func, GLint ref, GLuint mask) {
  glStencilFuncSeparate(face, func, ref, mask);
}

static void egg_glStencilMask(wasm_exec_env_t ee,GLuint mask) {
  glStencilMask(mask);
}

static void egg_glStencilMaskSeparate(wasm_exec_env_t ee,GLenum face, GLuint mask) {
  glStencilMaskSeparate( face, mask);
}

static void egg_glStencilOp(wasm_exec_env_t ee,GLenum fail, GLenum zfail, GLenum zpass) {
  glStencilOp(fail, zfail, zpass);
}

static void egg_glStencilOpSeparate(wasm_exec_env_t ee,GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
  glStencilOpSeparate( face,  sfail, dpfail, dppass);
}

static void egg_glTexImage2D(wasm_exec_env_t ee,GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) {
  glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

static void egg_glTexParameterf(wasm_exec_env_t ee,GLenum target, GLenum pname, GLfloat param) {
  glTexParameterf(target, pname, param);
}

static void egg_glTexParameterfv(wasm_exec_env_t ee,GLenum target, GLenum pname, const GLfloat *params) {
  glTexParameterfv(target, pname, params);
}

static void egg_glTexParameteri(wasm_exec_env_t ee,GLenum target, GLenum pname, GLint param) {
  glTexParameteri(target, pname, param);
}

static void egg_glTexParameteriv(wasm_exec_env_t ee,GLenum target, GLenum pname, const GLint *params) {
  glTexParameteriv(target, pname, params);
}

static void egg_glTexSubImage2D(wasm_exec_env_t ee,GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) {
  glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

static void egg_glUniform1f(wasm_exec_env_t ee,GLint location, GLfloat v0) {
  glUniform1f(location, v0);
}

static void egg_glUniform1fv(wasm_exec_env_t ee,GLint location, GLsizei count, const GLfloat *value) {
  glUniform1fv( location, count,value);
}

static void egg_glUniform1i(wasm_exec_env_t ee,GLint location, GLint v0) {
  glUniform1i(location, v0);
}

static void egg_glUniform1iv(wasm_exec_env_t ee,GLint location, GLsizei count, const GLint *value) {
  glUniform1iv( location, count,value);
}

static void egg_glUniform2f(wasm_exec_env_t ee,GLint location, GLfloat v0, GLfloat v1) {
  glUniform2f(location, v0, v1);
}

static void egg_glUniform2fv(wasm_exec_env_t ee,GLint location, GLsizei count, const GLfloat *value) {
  glUniform2fv( location, count,value);
}

static void egg_glUniform2i(wasm_exec_env_t ee,GLint location, GLint v0, GLint v1) {
  glUniform2i(location, v0, v1);
}

static void egg_glUniform2iv(wasm_exec_env_t ee,GLint location, GLsizei count, const GLint *value) {
  glUniform2iv( location, count,value);
}

static void egg_glUniform3f(wasm_exec_env_t ee,GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
  glUniform3f(location, v0, v1, v2);
}

static void egg_glUniform3fv(wasm_exec_env_t ee,GLint location, GLsizei count, const GLfloat *value) {
  glUniform3fv( location, count,value);
}

static void egg_glUniform3i(wasm_exec_env_t ee,GLint location, GLint v0, GLint v1, GLint v2) {
  glUniform3i(location, v0, v1, v2);
}

static void egg_glUniform3iv(wasm_exec_env_t ee,GLint location, GLsizei count, const GLint *value) {
  glUniform3iv( location, count,value);
}

static void egg_glUniform4f(wasm_exec_env_t ee,GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
  glUniform4f(location, v0, v1, v2, v3);
}

static void egg_glUniform4fv(wasm_exec_env_t ee,GLint location, GLsizei count, const GLfloat *value) {
  glUniform4fv( location, count,value);
}

static void egg_glUniform4i(wasm_exec_env_t ee,GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
  glUniform4i(location, v0, v1, v2, v3);
}

static void egg_glUniform4iv(wasm_exec_env_t ee,GLint location, GLsizei count, const GLint *value) {
  glUniform4iv( location, count,value);
}

static void egg_glUniformMatrix2fv(wasm_exec_env_t ee,GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
  glUniformMatrix2fv(location, count, transpose, value);
}

static void egg_glUniformMatrix3fv(wasm_exec_env_t ee,GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
  glUniformMatrix3fv( location, count, transpose,value);
}

static void egg_glUniformMatrix4fv(wasm_exec_env_t ee,GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
  glUniformMatrix4fv(location, count, transpose, value);
}

static void egg_glUseProgram(wasm_exec_env_t ee,GLuint program) {
  glUseProgram( program);
}

static void egg_glValidateProgram(wasm_exec_env_t ee,GLuint program) {
  glValidateProgram( program);
}

static void egg_glVertexAttrib1f(wasm_exec_env_t ee,GLuint index, GLfloat x) {
  glVertexAttrib1f( index, x);
}

static void egg_glVertexAttrib1fv(wasm_exec_env_t ee,GLuint index, const GLfloat *v) {
  glVertexAttrib1fv( index,v);
}

static void egg_glVertexAttrib2f(wasm_exec_env_t ee,GLuint index, GLfloat x, GLfloat y) {
  glVertexAttrib2f(index, x, y);
}

static void egg_glVertexAttrib2fv(wasm_exec_env_t ee,GLuint index, const GLfloat *v) {
  glVertexAttrib2fv(index, v);
}

static void egg_glVertexAttrib3f(wasm_exec_env_t ee,GLuint index, GLfloat x, GLfloat y, GLfloat z) {
  glVertexAttrib3f( index, x, y, z);
}

static void egg_glVertexAttrib3fv(wasm_exec_env_t ee,GLuint index, const GLfloat *v) {
  glVertexAttrib3fv(index, v);
}

static void egg_glVertexAttrib4f(wasm_exec_env_t ee,GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  glVertexAttrib4f( index, x, y, z, w);
}

static void egg_glVertexAttrib4fv(wasm_exec_env_t ee,GLuint index, const GLfloat *v) {
  glVertexAttrib4fv(index, v);
}

static void egg_glVertexAttribPointer(wasm_exec_env_t ee,GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
  glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

static void egg_glViewport(wasm_exec_env_t ee,GLint x, GLint y, GLsizei width, GLsizei height) {
  glViewport(x, y, width, height);
}

/* Install exports (both runtimes).
 */
 
static const JSCFunctionListEntry egg_native_js_exports[]={
  JS_CFUNC_DEF("log",0,egg_js_log),
  JS_CFUNC_DEF("event_next",0,egg_js_event_next),
  JS_CFUNC_DEF("event_enable",0,egg_js_event_enable),
  JS_CFUNC_DEF("input_device_get_name",0,egg_js_input_device_get_name),
  JS_CFUNC_DEF("input_device_get_ids",0,egg_js_input_device_get_ids),
  JS_CFUNC_DEF("input_device_get_button",0,egg_js_input_device_get_button),
  JS_CFUNC_DEF("input_device_disconnect",0,egg_js_input_device_disconnect),
  JS_CFUNC_DEF("video_get_size",0,egg_js_video_get_size),
  JS_CFUNC_DEF("audio_play_song",0,egg_js_audio_play_song),
  JS_CFUNC_DEF("audio_play_sound",0,egg_js_audio_play_sound),
  JS_CFUNC_DEF("audio_get_playhead",0,egg_js_audio_get_playhead),
  JS_CFUNC_DEF("res_get",0,egg_js_res_get),
  JS_CFUNC_DEF("res_id_by_index",0,egg_js_res_id_by_index),
  JS_CFUNC_DEF("store_set",0,egg_js_store_set),
  JS_CFUNC_DEF("store_get",0,egg_js_store_get),
  JS_CFUNC_DEF("store_key_by_index",0,egg_js_store_key_by_index),
  JS_CFUNC_DEF("http_request",0,egg_js_http_request),
  JS_CFUNC_DEF("http_get_status",0,egg_js_http_get_status),
  JS_CFUNC_DEF("http_get_header",0,egg_js_http_get_header),
  JS_CFUNC_DEF("http_get_body",0,egg_js_http_get_body),
  JS_CFUNC_DEF("ws_connect",0,egg_js_ws_connect),
  JS_CFUNC_DEF("ws_disconnect",0,egg_js_ws_disconnect),
  JS_CFUNC_DEF("ws_get_message",0,egg_js_ws_get_message),
  JS_CFUNC_DEF("ws_send",0,egg_js_ws_send),
  JS_CFUNC_DEF("time_real",0,egg_js_time_real),
  JS_CFUNC_DEF("time_get",0,egg_js_time_get),
  JS_CFUNC_DEF("get_user_languages",0,egg_js_get_user_languages),
  JS_CFUNC_DEF("request_termination",0,egg_js_request_termination),
  JS_CFUNC_DEF("is_terminable",0,egg_js_is_terminable),
};

static NativeSymbol egg_native_wasm_exports[]={
  {"egg_log",egg_wasm_log,"($i)"},
  {"egg_event_next",egg_wasm_event_next,"(*i)i"},
  {"egg_event_enable",egg_wasm_event_enable,"(ii)i"},
  {"egg_input_device_get_name",egg_wasm_input_device_get_name,"(*ii)i"},
  {"egg_input_device_get_ids",egg_wasm_input_device_get_ids,"(***i)"},
  {"egg_input_device_get_button",egg_wasm_input_device_get_button,"(*****ii)"},
  {"egg_input_device_disconnect",egg_wasm_input_device_disconnect,"(i)"},
  {"egg_video_get_size",egg_wasm_video_get_size,"(**)"},
  {"egg_audio_play_song",egg_wasm_audio_play_song,"(iii)"},
  {"egg_audio_play_sound",egg_wasm_audio_play_sound,"(iff)"},
  {"egg_audio_get_playhead",egg_wasm_audio_get_playhead,"()i"},
  {"egg_res_get",egg_wasm_res_get,"(*~iii)i"},
  {"egg_res_id_by_index",egg_wasm_res_id_by_index,"(***i)"},
  {"egg_store_set",egg_wasm_store_set,"(*~*~)i"},
  {"egg_store_get",egg_wasm_store_get,"(*~*~)i"},
  {"egg_store_key_by_index",egg_wasm_store_key_by_index,"(*~i)i"},
  {"egg_http_request",egg_wasm_http_request,"($$*~)i"},
  {"egg_http_get_status",egg_wasm_http_get_status,"(i)i"},
  {"egg_http_get_header",egg_wasm_http_get_header,"(*~i*~)i"},
  {"egg_http_get_body",egg_wasm_http_get_body,"(*~i)i"},
  {"egg_ws_connect",egg_wasm_ws_connect,"($)i"},
  {"egg_ws_disconnecct",egg_wasm_ws_disconnect,"(i)"},
  {"egg_ws_get_message",egg_wasm_ws_get_message,"(*~ii)i"},
  {"egg_ws_send",egg_wasm_ws_send,"(ii*~)"},
  {"egg_time_real",egg_wasm_time_real,"()f"},
  {"egg_time_get",egg_wasm_time_get,"(*******)"},
  {"egg_get_user_languages",egg_wasm_get_user_languages,"(*i)i"},
  {"egg_request_termination",egg_wasm_request_termination,"()"},
  {"egg_is_terminable",egg_wasm_is_terminable,"()i"},
  
  {"glActiveTexture",egg_glActiveTexture,"(i)"},
  {"glAttachShader",egg_glAttachShader,"(ii)"},
  {"glBindAttribLocation",egg_glBindAttribLocation,"(ii$)"},
  {"glBindBuffer",egg_glBindBuffer,"(ii)"},
  {"glBindFramebuffer",egg_glBindFramebuffer,"(ii)"},
  {"glBindRenderbuffer",egg_glBindRenderbuffer,"(ii)"},
  {"glBindTexture",egg_glBindTexture,"(ii)"},
  {"glBlendColor",egg_glBlendColor,"(ffff)"},
  {"glBlendEquation",egg_glBlendEquation,"(i)"},
  {"glBlendEquationSeparate",egg_glBlendEquationSeparate,"(ii)"},
  {"glBlendFunc",egg_glBlendFunc,"(ii)"},
  {"glBlendFuncSeparate",egg_glBlendFuncSeparate,"(iiii)"},
  {"glBufferData",egg_glBufferData,"(ii*i)"},
  {"glBufferSubData",egg_glBufferSubData,"(iii*)"},
  {"glCheckFramebufferStatus",egg_glCheckFramebufferStatus,"(i)i"},
  {"glClear",egg_glClear,"(i)"},
  {"glClearColor",egg_glClearColor,"(ffff)"},
  {"glClearDepthf",egg_glClearDepthf,"(f)"},
  {"glClearStencil",egg_glClearStencil,"(i)"},
  {"glColorMask",egg_glColorMask,"(iiii)"},
  {"glCompileShader",egg_glCompileShader,"(i)"},
  {"glCompressedTexImage2D",egg_glCompressedTexImage2D,"(iiiiiii*)"},
  {"glCompressedTexSubImage2D",egg_glCompressedTexSubImage2D,"(iiiiiiii*)"},
  {"glCopyTexImage2D",egg_glCopyTexImage2D,"(iiiiiiii)"},
  {"glCopyTexSubImage2D",egg_glCopyTexSubImage2D,"(iiiiiiii)"},
  {"glCreateProgram",egg_glCreateProgram,"()i"},
  {"glCreateShader",egg_glCreateShader,"(i)i"},
  {"glCullFace",egg_glCullFace,"(i)"},
  {"glDeleteBuffers",egg_glDeleteBuffers,"(i*)"},
  {"glDeleteFramebuffers",egg_glDeleteFramebuffers,"(i*)"},
  {"glDeleteProgram",egg_glDeleteProgram,"(i)"},
  {"glDeleteRenderbuffers",egg_glDeleteRenderbuffers,"(i*)"},
  {"glDeleteShader",egg_glDeleteShader,"(i)"},
  {"glDeleteTextures",egg_glDeleteTextures,"(i*)"},
  {"glDepthFunc",egg_glDepthFunc,"(i)"},
  {"glDepthMask",egg_glDepthMask,"(i)"},
  {"glDepthRangef",egg_glDepthRangef,"(ff)"},
  {"glDetachShader",egg_glDetachShader,"(ii)"},
  {"glDisable",egg_glDisable,"(i)"},
  {"glDisableVertexAttribArray",egg_glDisableVertexAttribArray,"(i)"},
  {"glDrawArrays",egg_glDrawArrays,"(iii)"},
  {"glDrawElements",egg_glDrawElements,"(iii*)"},
  {"glEnable",egg_glEnable,"(i)"},
  {"glEnableVertexAttribArray",egg_glEnableVertexAttribArray,"(i)"},
  {"glFinish",egg_glFinish,"()"},
  {"glFlush",egg_glFlush,"()"},
  {"glFramebufferRenderbuffer",egg_glFramebufferRenderbuffer,"(iiii)"},
  {"glFramebufferTexture2D",egg_glFramebufferTexture2D,"(iiiii)"},
  {"glFrontFace",egg_glFrontFace,"(i)"},
  {"glGenBuffers",egg_glGenBuffers,"(i*)"},
  {"glGenerateMipmap",egg_glGenerateMipmap,"(i)"},
  {"glGenFramebuffers",egg_glGenFramebuffers,"(i*)"},
  {"glGenRenderbuffers",egg_glGenRenderbuffers,"(i*)"},
  {"glGenTextures",egg_glGenTextures,"(i*)"},
  {"glGetActiveAttrib",egg_glGetActiveAttrib,"(iii****)"},
  {"glGetActiveUniform",egg_glGetActiveUniform,"(iii****)"},
  {"glGetAttachedShaders",egg_glGetAttachedShaders,"(ii**)"},
  {"glGetAttribLocation",egg_glGetAttribLocation,"(i$)i"},
  {"glGetBooleanv",egg_glGetBooleanv,"(i*)"},
  {"glGetBufferParameteriv",egg_glGetBufferParameteriv,"(ii*)"},
  {"glGetError",egg_glGetError,"()i"},
  {"glGetFloatv",egg_glGetFloatv,"(i*)"},
  {"glGetFramebufferAttachmentParameteriv",egg_glGetFramebufferAttachmentParameteriv,"(iii*)"},
  {"glGetIntegerv",egg_glGetIntegerv,"(i*)"},
  {"glGetProgramiv",egg_glGetProgramiv,"(ii*)"},
  {"glGetProgramInfoLog",egg_glGetProgramInfoLog,"(ii**)"},
  {"glGetRenderbufferParameteriv",egg_glGetRenderbufferParameteriv,"(ii*)"},
  {"glGetShaderiv",egg_glGetShaderiv,"(ii*)"},
  {"glGetShaderInfoLog",egg_glGetShaderInfoLog,"(ii**)"},
  {"glGetShaderPrecisionFormat",egg_glGetShaderPrecisionFormat,"(ii**)"},
  {"glGetShaderSource",egg_glGetShaderSource,"(ii**)"},
  {"glGetTexParameterfv",egg_glGetTexParameterfv,"(ii*)"},
  {"glGetTexParameteriv",egg_glGetTexParameteriv,"(ii*)"},
  {"glGetUniformfv",egg_glGetUniformfv,"(ii*)"},
  {"glGetUniformiv",egg_glGetUniformiv,"(ii*)"},
  {"glGetUniformLocation",egg_glGetUniformLocation,"(i*)i"},
  {"glGetVertexAttribfv",egg_glGetVertexAttribfv,"(ii*)"},
  {"glGetVertexAttribiv",egg_glGetVertexAttribiv,"(ii*)"},
  {"singleGetVertexAttribPointerv",egg_singleGetVertexAttribPointerv,"(ii)*"},
  {"glHint",egg_glHint,"(ii)"},
  {"glIsBuffer",egg_glIsBuffer,"(i)i"},
  {"glIsEnabled",egg_glIsEnabled,"(i)i"},
  {"glIsFramebuffer",egg_glIsFramebuffer,"(i)i"},
  {"glIsProgram",egg_glIsProgram,"(i)i"},
  {"glIsRenderbuffer",egg_glIsRenderbuffer,"(i)i"},
  {"glIsShader",egg_glIsShader,"(i)i"},
  {"glIsTexture",egg_glIsTexture,"(i)i"},
  {"glLineWidth",egg_glLineWidth,"(f)"},
  {"glLinkProgram",egg_glLinkProgram,"(i)"},
  {"glPixelStorei",egg_glPixelStorei,"(ii)"},
  {"glPolygonOffset",egg_glPolygonOffset,"(ff)"},
  {"glReadPixels",egg_glReadPixels,"(iiiiii*)"},
  {"glReleaseShaderCompiler",egg_glReleaseShaderCompiler,"()"},
  {"glRenderbufferStorage",egg_glRenderbufferStorage,"(iiii)"},
  {"glSampleCoverage",egg_glSampleCoverage,"(fi)"},
  {"glScissor",egg_glScissor,"(iiii)"},
  {"glShaderBinary",egg_glShaderBinary,"(i*i*i)"},
  {"singleShaderSource",egg_singleShaderSource,"(ii*i)"},
  {"glStencilFunc",egg_glStencilFunc,"(iii)"},
  {"glStencilFuncSeparate",egg_glStencilFuncSeparate,"(iiii)"},
  {"glStencilMask",egg_glStencilMask,"(i)"},
  {"glStencilMaskSeparate",egg_glStencilMaskSeparate,"(ii)"},
  {"glStencilOp",egg_glStencilOp,"(iii)"},
  {"glStencilOpSeparate",egg_glStencilOpSeparate,"(iiii)"},
  {"glTexImage2D",egg_glTexImage2D,"(iiiiiiii*)"},
  {"glTexParameterf",egg_glTexParameterf,"(iif)"},
  {"glTexParameterfv",egg_glTexParameterfv,"(ii*)"},
  {"glTexParameteri",egg_glTexParameteri,"(iii)"},
  {"glTexParameteriv",egg_glTexParameteriv,"(ii*)"},
  {"glTexSubImage2D",egg_glTexSubImage2D,"(iiiiiiii*)"},
  {"glUniform1f",egg_glUniform1f,"(if)"},
  {"glUniform1fv",egg_glUniform1fv,"(ii*)"},
  {"glUniform1i",egg_glUniform1i,"(ii)"},
  {"glUniform1iv",egg_glUniform1iv,"(ii*)"},
  {"glUniform2f",egg_glUniform2f,"(iff)"},
  {"glUniform2fv",egg_glUniform2fv,"(ii*)"},
  {"glUniform2i",egg_glUniform2i,"(iii)"},
  {"glUniform2iv",egg_glUniform2iv,"(ii*)"},
  {"glUniform3f",egg_glUniform3f,"(ifff)"},
  {"glUniform3fv",egg_glUniform3fv,"(ii*)"},
  {"glUniform3i",egg_glUniform3i,"(iiii)"},
  {"glUniform3iv",egg_glUniform3iv,"(ii*)"},
  {"glUniform4f",egg_glUniform4f,"(iffff)"},
  {"glUniform4fv",egg_glUniform4fv,"(ii*)"},
  {"glUniform4i",egg_glUniform4i,"(iiiii)"},
  {"glUniform4iv",egg_glUniform4iv,"(ii*)"},
  {"glUniformMatrix2fv",egg_glUniformMatrix2fv,"(iii*)"},
  {"glUniformMatrix3fv",egg_glUniformMatrix3fv,"(iii*)"},
  {"glUniformMatrix4fv",egg_glUniformMatrix4fv,"(iii*)"},
  {"glUseProgram",egg_glUseProgram,"(i)"},
  {"glValidateProgram",egg_glValidateProgram,"(i)"},
  {"glVertexAttrib1f",egg_glVertexAttrib1f,"(if)"},
  {"glVertexAttrib1fv",egg_glVertexAttrib1fv,"(i*)"},
  {"glVertexAttrib2f",egg_glVertexAttrib2f,"(iff)"},
  {"glVertexAttrib2fv",egg_glVertexAttrib2fv,"(i*)"},
  {"glVertexAttrib3f",egg_glVertexAttrib3f,"(ifff)"},
  {"glVertexAttrib3fv",egg_glVertexAttrib3fv,"(i*)"},
  {"glVertexAttrib4f",egg_glVertexAttrib4f,"(iffff)"},
  {"glVertexAttrib4fv",egg_glVertexAttrib4fv,"(i*)"},
  {"glVertexAttribPointer",egg_glVertexAttribPointer,"(iiiii*)"},
  {"glViewport",egg_glViewport,"(iiii)"},
};
 
int egg_native_install_runtime_exports() {
  if (qjs_set_exports(egg.qjs,"egg",egg_native_js_exports,sizeof(egg_native_js_exports)/sizeof(egg_native_js_exports[0]))<0) return -1;
  if (wamr_set_exports(egg.wamr,egg_native_wasm_exports,sizeof(egg_native_wasm_exports)/sizeof(egg_native_wasm_exports[0]))<0) return -1;
  return 0;
}
