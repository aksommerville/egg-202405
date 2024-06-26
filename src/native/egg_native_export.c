/* egg_native_export.c
 * All JS and Wasm hooks live here.
 * Most of the Public API is implemented here.
 * Elsewhere:
 *   egg_native_net.c -- http and ws
 *   egg_native_event.c -- egg_event_next,egg_event_enable
 *   egg_native_input.c -- input
 */
 
#ifndef EGG_ENABLE_VM
  #error "Please define EGG_ENABLE_VM to 0 or 1"
#endif

#include "egg_native_internal.h"
#include "quickjs.h"
#include "wasm_export.h"
#include "opt/strfmt/strfmt.h"
#include "opt/rawimg/rawimg.h"
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#define JSASSERTARGC(expect) if (argc!=expect) return JS_ThrowTypeError(ctx,"%s requires %d arguments, got %d",__func__,expect,argc);
#define JSASSERTARGRANGE(lo,hi) if ((argc<lo)||(argc>hi)) return JS_ThrowTypeError(ctx,"%s requires %d..%d arguments, got %d",__func__,lo,hi,argc);

static void egg_js_dummy_free(JSRuntime *rt,void *opaque,void *ptr) {}
static void egg_js_basic_free(JSRuntime *rt,void *opaque,void *ptr) { if (ptr) free(ptr); }

/* Synthesizer.
 */
 
void egg_audio_play_song(int qual,int songid,int force,int repeat) {
  if (hostio_audio_lock(egg.hostio)>=0) {
    synth_play_song(egg.synth,qual,songid,force,repeat);
    hostio_audio_unlock(egg.hostio);
  }
}

void egg_audio_play_sound(int qual,int soundid,double trim,double pan) {
  if (hostio_audio_lock(egg.hostio)>=0) {
    synth_play_sound(egg.synth,qual,soundid,trim,pan);
    hostio_audio_unlock(egg.hostio);
  }
}

double egg_audio_get_playhead() {
  // Do not lock driver.
  return synth_get_playhead(egg.synth);//TODO also ask the audio driver its buffer position
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
  romr_id_by_index(tid,qual,rid,&egg.romr,index);
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
  if (dsta<1) return 0;
  /* If we were launched with --lang, that's the one and only answer.
   */
  if (egg.lang) {
    dst[0]=egg.lang;
    return 1;
  }
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

#if EGG_ENABLE_VM
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
#endif

/* wasm: log
 */

#if EGG_ENABLE_VM
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
#endif

/* egg_event_next
 */

#if EGG_ENABLE_VM
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
#endif

/* egg_event_enable
 */

#if EGG_ENABLE_VM
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
#endif

/* egg_input_device_get_name
 */
 
#if EGG_ENABLE_VM
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
#endif

/* egg_input_device_get_ids
 */
 
#if EGG_ENABLE_VM
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
#endif

/* egg_input_device_get_button
 */
 
#if EGG_ENABLE_VM
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
#endif

/* egg_input_device_disconnect
 */

#if EGG_ENABLE_VM
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
#endif

/* egg_show_cursor
 */
 
#if EGG_ENABLE_VM
static JSValue egg_js_show_cursor(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t show=1;
  JS_ToInt32(ctx,&show,argv[0]);
  egg_show_cursor(show);
  return JS_NULL;
}

static void egg_wasm_show_cursor(wasm_exec_env_t ee,int show) {
  egg_show_cursor(show);
}
#endif

/* egg_image_get_header
 */
 
void egg_image_get_header(int *w,int *h,int *fmt,int qual,int imageid) {
  const void *serial=0;
  int serialc=romr_get_qualified(&serial,&egg.romr,EGG_TID_image,qual,imageid);
  if (serialc<1) return;
  int pixelsize=0;
  const char *encfmt=rawimg_decode_header(w,h,0,&pixelsize,serial,serialc);
  if (!encfmt) return;
  if (fmt) switch (pixelsize) {
    case 32: *fmt=EGG_TEX_FMT_RGBA; break;
    case 8: *fmt=EGG_TEX_FMT_A8; break;
    case 4: *fmt=EGG_TEX_FMT_RGBA; break; // ICO may use 4-bit pixels; they'll promote to 32 on decode.
    case 1: *fmt=EGG_TEX_FMT_A1; break;
  }
}

#if EGG_ENABLE_VM
static JSValue egg_js_image_get_header(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(2)
  int32_t qual=0,imageid=0;
  JS_ToInt32(ctx,&qual,argv[0]);
  JS_ToInt32(ctx,&imageid,argv[1]);
  int w=0,h=0,fmt=0;
  egg_image_get_header(&w,&h,&fmt,qual,imageid);
  JSValue result=JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,result,"w",JS_NewInt32(ctx,w));
  JS_SetPropertyStr(ctx,result,"h",JS_NewInt32(ctx,h));
  JS_SetPropertyStr(ctx,result,"fmt",JS_NewInt32(ctx,fmt));
  return result;
}

static void egg_wasm_image_get_header(wasm_exec_env_t ee,int *w,int *h,int *fmt,int qual,int imageid) {
  egg_image_get_header(w,h,fmt,qual,imageid);
}
#endif

/* egg_image_decode
 */
 
static int egg_image_minimum_stride(int w,int fmt) {
  if (w<1) return 0;
  if (w>INT_MAX>>2) return 0;
  switch (fmt) {
    case EGG_TEX_FMT_RGBA: return w<<2;
    case EGG_TEX_FMT_A8:
    case EGG_TEX_FMT_Y8: return w;
    case EGG_TEX_FMT_A1:
    case EGG_TEX_FMT_Y1: return (w+7)>>3;
  }
  return 0;
}
 
int egg_image_decode(void *dst,int dsta,int stride,int qual,int imageid) {
  if (stride<1) return -1;
  const void *serial=0;
  int serialc=romr_get_qualified(&serial,&egg.romr,EGG_TID_image,qual,imageid);
  if (serialc<1) return -1;
  struct rawimg *rawimg=rawimg_decode(serial,serialc);
  if (!rawimg) return -1;
  if (rawimg->stride>stride) {
    rawimg_del(rawimg);
    return -1;
  }
  int dstc=rawimg->stride*rawimg->h;
  if (dstc<=dsta) {
    if (stride==rawimg->stride) {
      memcpy(dst,rawimg->v,dstc);
    } else {
      const uint8_t *srcrow=rawimg->v;
      uint8_t *dstrow=dst;
      int i=rawimg->h;
      for (;i-->0;srcrow+=rawimg->stride,dstrow+=stride) {
        memcpy(dstrow,srcrow,rawimg->stride);
      }
    }
  }
  rawimg_del(rawimg);
  return dstc;
}

#if EGG_ENABLE_VM
static JSValue egg_js_image_decode(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(2)
  int32_t qual=0,imageid=0;
  JS_ToInt32(ctx,&qual,argv[0]);
  JS_ToInt32(ctx,&imageid,argv[1]);
  int w=0,h=0,fmt=0;
  egg_image_get_header(&w,&h,&fmt,qual,imageid);
  if ((w<1)||(h<1)) return JS_NULL;
  int stride=egg_image_minimum_stride(w,fmt);
  if (stride<1) return JS_NULL;
  if (stride>INT_MAX/h) return JS_NULL;
  int dsta=stride*h;
  void *dst=malloc(dsta);
  if (!dst) return JS_NULL;
  int dstc=egg_image_decode(dst,dsta,stride,qual,imageid);
  if ((dstc<dsta)||(dstc>dsta)) {
    free(dst);
    return JS_NULL;
  }
  return JS_NewArrayBuffer(ctx,dst,dstc,egg_js_basic_free,0,0);
}

static int egg_wasm_image_decode(wasm_exec_env_t ee,void *dst,int dsta,int stride,int qual,int imageid) {
  return egg_image_decode(dst,dsta,stride,qual,imageid);
}
#endif

/* egg_texture_del
 */
 
#if EGG_ENABLE_VM
static JSValue egg_js_texture_del(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t texid=0;
  JS_ToInt32(ctx,&texid,argv[0]);
  if (egg.render) render_texture_del(egg.render,texid);
  else if (egg.softrender) softrender_texture_del(egg.softrender,texid);
  return JS_NULL;
}

static void egg_wasm_texture_del(wasm_exec_env_t ee,int texid) {
  if (egg.render) render_texture_del(egg.render,texid);
  else if (egg.softrender) softrender_texture_del(egg.softrender,texid);
}
#endif

void egg_texture_del(int texid) {
  if (egg.render) render_texture_del(egg.render,texid);
  else if (egg.softrender) softrender_texture_del(egg.softrender,texid);
}

/* egg_texture_new
 */

#if EGG_ENABLE_VM
static JSValue egg_js_texture_new(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  int texid;
  if (egg.render) texid=render_texture_new(egg.render);
  else if (egg.softrender) texid=softrender_texture_new(egg.softrender);
  return JS_NewInt32(ctx,texid);
}

static int egg_wasm_texture_new(wasm_exec_env_t ee) {
  if (egg.render) return render_texture_new(egg.render);
  if (egg.softrender) return softrender_texture_new(egg.softrender);
  return -1;
}
#endif

int egg_texture_new() {
  if (egg.render) return render_texture_new(egg.render);
  if (egg.softrender) return softrender_texture_new(egg.softrender);
  return -1;
}

/* egg_texture_load_image
 */

int egg_texture_load_image(int texid,int qual,int imageid) {
  const void *serial=0;
  int serialc=romr_get_qualified(&serial,&egg.romr,EGG_TID_image,qual,imageid);
  if (serialc<=0) return -1;
  int err=-1;
  if (egg.render) err=render_texture_load(egg.render,texid,0,0,0,0,serial,serialc);
  else if (egg.softrender) err=softrender_texture_load(egg.softrender,texid,0,0,0,0,serial,serialc);
  return err;
}
 
#if EGG_ENABLE_VM
static JSValue egg_js_texture_load_image(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(3)
  int32_t texid=0,qual=0,imageid=0;
  JS_ToInt32(ctx,&texid,argv[0]);
  JS_ToInt32(ctx,&qual,argv[1]);
  JS_ToInt32(ctx,&imageid,argv[2]);
  int err=egg_texture_load_image(texid,qual,imageid);
  return JS_NewInt32(ctx,err);
}

static int egg_wasm_texture_load_image(wasm_exec_env_t ee,int texid,int qual,int imageid) {
  return egg_texture_load_image(texid,qual,imageid);
}
#endif

/* egg_texture_upload
 */
 
int egg_texture_upload(int texid,int w,int h,int stride,int fmt,const void *src,int srcc) {
  if (egg.render) return render_texture_load(egg.render,texid,w,h,stride,fmt,src,srcc);
  if (egg.softrender) return softrender_texture_load(egg.softrender,texid,w,h,stride,fmt,src,srcc);
  return -1;
}

#if EGG_ENABLE_VM
static JSValue egg_js_texture_upload(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(6)
  int32_t texid=0,w=0,h=0,stride=0,fmt=0;
  JS_ToInt32(ctx,&texid,argv[0]);
  JS_ToInt32(ctx,&w,argv[1]);
  JS_ToInt32(ctx,&h,argv[2]);
  if ((w<1)||(h<1)) return JS_NewInt32(ctx,-1);
  JS_ToInt32(ctx,&stride,argv[3]);
  JS_ToInt32(ctx,&fmt,argv[4]);
  const uint8_t *v=0;
  size_t c=0;
  if (!JS_IsNull(argv[5])) {
    if (!(v=JS_GetArrayBuffer(ctx,&c,argv[5]))) {
      return JS_NewInt32(ctx,-1);
    }
  }
  return JS_NewInt32(ctx,egg_texture_upload(texid,w,h,stride,fmt,v,c));
}

static int egg_wasm_texture_upload(wasm_exec_env_t ee,int texid,int w,int h,int stride,int fmt,int srcaddr,int srcc) {
  void *src=0;
  if (srcaddr) {
    if (srcc<1) return -1;
    if (!(src=wamr_validate_pointer(egg.wamr,1,srcaddr,srcc))) return -1;
  }
  return egg_texture_upload(texid,w,h,stride,fmt,src,srcc);
}
#endif

/* egg_texture_get_header
 */

#if EGG_ENABLE_VM
static JSValue egg_js_texture_get_header(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t texid=0;
  JS_ToInt32(ctx,&texid,argv[0]);
  int w=0,h=0,fmt=0;
  if (egg.render) render_texture_get_header(&w,&h,&fmt,egg.render,texid);
  else if (egg.softrender) softrender_texture_get_header(&w,&h,&fmt,egg.softrender,texid);
  JSValue result=JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,result,"w",JS_NewInt32(ctx,w));
  JS_SetPropertyStr(ctx,result,"h",JS_NewInt32(ctx,h));
  JS_SetPropertyStr(ctx,result,"fmt",JS_NewInt32(ctx,fmt));
  return result;
}

static void egg_wasm_texture_get_header(wasm_exec_env_t ee,int *w,int *h,int *fmt,int texid) {
  if (egg.render) render_texture_get_header(w,h,fmt,egg.render,texid);
  else if (egg.softrender) softrender_texture_get_header(w,h,fmt,egg.softrender,texid);
}
#endif

void egg_texture_get_header(int *w,int *h,int *fmt,int texid) {
  if (egg.render) render_texture_get_header(w,h,fmt,egg.render,texid);
  else if (egg.softrender) softrender_texture_get_header(w,h,fmt,egg.softrender,texid);
}

/* egg_texture_clear
 */

#if EGG_ENABLE_VM
static JSValue egg_js_texture_clear(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t texid=0;
  JS_ToInt32(ctx,&texid,argv[0]);
  if (egg.render) render_texture_clear(egg.render,texid);
  else if (egg.softrender) softrender_texture_clear(egg.softrender,texid);
  return JS_NULL;
}

static void egg_wasm_texture_clear(wasm_exec_env_t ee,int texid) {
  if (egg.render) render_texture_clear(egg.render,texid);
  else if (egg.softrender) softrender_texture_clear(egg.softrender,texid);
}
#endif

void egg_texture_clear(int texid) {
  if (egg.render) render_texture_clear(egg.render,texid);
  else if (egg.softrender) softrender_texture_clear(egg.softrender,texid);
}

/* egg_draw_mode
 */

#if EGG_ENABLE_VM
static JSValue egg_js_draw_mode(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(3)
  int32_t xfermode=0,replacement=0,alpha=0;
  JS_ToInt32(ctx,&xfermode,argv[0]);
  JS_ToInt32(ctx,&replacement,argv[1]);
  JS_ToInt32(ctx,&alpha,argv[2]);
  if (egg.render) render_draw_mode(egg.render,xfermode,replacement,alpha);
  else if (egg.softrender) softrender_draw_mode(egg.softrender,xfermode,replacement,alpha);
  return JS_NULL;
}

static void egg_wasm_draw_mode(wasm_exec_env_t ee,int xfermode,int replacement,int alpha) {
  if (egg.render) render_draw_mode(egg.render,xfermode,replacement,alpha);
  else if (egg.softrender) softrender_draw_mode(egg.softrender,xfermode,replacement,alpha);
}
#endif

void egg_draw_mode(int xfermode,uint32_t tint,uint8_t alpha) {
  if (egg.render) render_draw_mode(egg.render,xfermode,tint,alpha);
  else if (egg.softrender) softrender_draw_mode(egg.softrender,xfermode,tint,alpha);
}

/* egg_draw_rect
 */

#if EGG_ENABLE_VM
static JSValue egg_js_draw_rect(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(6)
  int32_t texid=0,x=0,y=0,w=0,h=0,pixel=0;
  JS_ToInt32(ctx,&texid,argv[0]);
  JS_ToInt32(ctx,&x,argv[1]);
  JS_ToInt32(ctx,&y,argv[2]);
  JS_ToInt32(ctx,&w,argv[3]);
  JS_ToInt32(ctx,&h,argv[4]);
  JS_ToInt32(ctx,&pixel,argv[5]);
  if (egg.render) render_draw_rect(egg.render,texid,x,y,w,h,pixel);
  else if (egg.softrender) softrender_draw_rect(egg.softrender,texid,x,y,w,h,pixel);
  return JS_NULL;
}

static void egg_wasm_draw_rect(wasm_exec_env_t ee,int texid,int x,int y,int w,int h,int pixel) {
  if (egg.render) render_draw_rect(egg.render,texid,x,y,w,h,pixel);
  else if (egg.softrender) softrender_draw_rect(egg.softrender,texid,x,y,w,h,pixel);
}
#endif

void egg_draw_rect(int texid,int x,int y,int w,int h,uint32_t pixel) {
  if (egg.render) render_draw_rect(egg.render,texid,x,y,w,h,pixel);
  else if (egg.softrender) softrender_draw_rect(egg.softrender,texid,x,y,w,h,pixel);
}

/* egg_draw_decal
 */

#if EGG_ENABLE_VM
static JSValue egg_js_draw_decal(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(9)
  int32_t dsttexid=0,srctexid=0,dstx=0,dsty=0,srcx=0,srcy=0,w=0,h=0,xform=0;
  JS_ToInt32(ctx,&dsttexid,argv[0]);
  JS_ToInt32(ctx,&srctexid,argv[1]);
  JS_ToInt32(ctx,&dstx,argv[2]);
  JS_ToInt32(ctx,&dsty,argv[3]);
  JS_ToInt32(ctx,&srcx,argv[4]);
  JS_ToInt32(ctx,&srcy,argv[5]);
  JS_ToInt32(ctx,&w,argv[6]);
  JS_ToInt32(ctx,&h,argv[7]);
  JS_ToInt32(ctx,&xform,argv[8]);
  if (egg.render) render_draw_decal(egg.render,dsttexid,srctexid,dstx,dsty,srcx,srcy,w,h,xform);
  else if (egg.softrender) softrender_draw_decal(egg.softrender,dsttexid,srctexid,dstx,dsty,srcx,srcy,w,h,xform);
  return JS_NULL;
}

static void egg_wasm_draw_decal(
  wasm_exec_env_t ee,
  int dsttexid,int srctexid,
  int dstx,int dsty,
  int srcx,int srcy,
  int w,int h,
  int xform
) {
  if (egg.render) render_draw_decal(egg.render,dsttexid,srctexid,dstx,dsty,srcx,srcy,w,h,xform);
  else if (egg.softrender) softrender_draw_decal(egg.softrender,dsttexid,srctexid,dstx,dsty,srcx,srcy,w,h,xform);
}
#endif

void egg_draw_decal(
  int dsttexid,int srctexid,
  int dstx,int dsty,
  int srcx,int srcy,
  int w,int h,
  int xform
) {
  if (egg.render) render_draw_decal(egg.render,dsttexid,srctexid,dstx,dsty,srcx,srcy,w,h,xform);
  else if (egg.softrender) softrender_draw_decal(egg.softrender,dsttexid,srctexid,dstx,dsty,srcx,srcy,w,h,xform);
}

/* egg_draw_tile
 */

#if EGG_ENABLE_VM
static JSValue egg_js_draw_tile(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(4)
  int32_t dsttexid=0,srctexid=0,c=0;
  JS_ToInt32(ctx,&dsttexid,argv[0]);
  JS_ToInt32(ctx,&srctexid,argv[1]);
  JS_ToInt32(ctx,&c,argv[3]);
  if (c<1) return JS_NULL;
  size_t a=0;
  const void *vtxv=JS_GetArrayBuffer(ctx,&a,argv[2]);
  if (!vtxv||(a<=0)) return JS_NULL;
  a/=6;
  if (c>a) return JS_NULL;
  if (egg.render) render_draw_tile(egg.render,dsttexid,srctexid,vtxv,c);
  else if (egg.softrender) softrender_draw_tile(egg.softrender,dsttexid,srctexid,vtxv,c);
  return JS_NULL;
}

static void egg_wasm_draw_tile(wasm_exec_env_t ee,int dsttexid,int srctexid,int vaddr,int c) {
  if (c<1) return;
  void *vtxv=wamr_validate_pointer(egg.wamr,1,vaddr,c*sizeof(struct egg_draw_tile));
  if (!vtxv) return;
  if (egg.render) render_draw_tile(egg.render,dsttexid,srctexid,vtxv,c);
  else if (egg.softrender) softrender_draw_tile(egg.softrender,dsttexid,srctexid,vtxv,c);
}
#endif

void egg_draw_tile(int dsttexid,int srctexid,const struct egg_draw_tile *vtxv,int c) {
  if (egg.render) render_draw_tile(egg.render,dsttexid,srctexid,vtxv,c);
  else if (egg.softrender) softrender_draw_tile(egg.softrender,dsttexid,srctexid,vtxv,c);
}

/* egg_audio_play_song
 */

#if EGG_ENABLE_VM
static JSValue egg_js_audio_play_song(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(4)
  int32_t qual=0,songid=0,force=0,repeat=0;
  JS_ToInt32(ctx,&qual,argv[0]);
  JS_ToInt32(ctx,&songid,argv[1]);
  JS_ToInt32(ctx,&force,argv[2]);
  JS_ToInt32(ctx,&repeat,argv[3]);
  egg_audio_play_song(qual,songid,force,repeat);
  return JS_NULL;
}
 
static void egg_wasm_audio_play_song(wasm_exec_env_t ee,int qual,int songid,int force,int repeat) {
  egg_audio_play_song(qual,songid,force,repeat);
}
#endif

/* egg_audio_play_sound
 */

#if EGG_ENABLE_VM
static JSValue egg_js_audio_play_sound(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(4)
  int32_t qual=0,soundid=0;
  double trim=0.0,pan=0.0;
  JS_ToInt32(ctx,&qual,argv[0]);
  JS_ToInt32(ctx,&soundid,argv[1]);
  JS_ToFloat64(ctx,&trim,argv[2]);
  JS_ToFloat64(ctx,&pan,argv[3]);
  egg_audio_play_sound(qual,soundid,trim,pan);
  return JS_NULL;
}
 
static void egg_wasm_audio_play_sound(wasm_exec_env_t ee,int qual,int soundid,double trim,double pan) {
  egg_audio_play_sound(qual,soundid,trim,pan);
}
#endif

/* egg_audio_get_playhead
 */
 
#if EGG_ENABLE_VM
static JSValue egg_js_audio_get_playhead(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  return JS_NewFloat64(ctx,egg_audio_get_playhead());
}
 
static double egg_wasm_audio_get_playhead(wasm_exec_env_t ee) {
  return egg_audio_get_playhead();
}
#endif

/* egg_res_get
 */
 
#if EGG_ENABLE_VM
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
#endif

/* egg_res_id_by_index
 */

#if EGG_ENABLE_VM
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
#endif

/* egg_store_set
 */
 
#if EGG_ENABLE_VM
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
#endif

/* get_store_get
 */
 
#if EGG_ENABLE_VM
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
    buf=nv;
  }
}
 
static int egg_wasm_store_get(wasm_exec_env_t ee,char *dst,int dsta,const char *k,int kc) {
  return egg_store_get(dst,dsta,k,kc);
}
#endif

/* egg_store_key_by_index
 */
 
#if EGG_ENABLE_VM
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
#endif

/* egg_http_request
 */
 
#if EGG_ENABLE_VM
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
#endif

/* egg_http_get_status
 */
 
#if EGG_ENABLE_VM
static JSValue egg_js_http_get_status(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  JSASSERTARGC(1)
  int32_t reqid=-1;
  JS_ToInt32(ctx,&reqid,argv[0]);
  return JS_NewInt32(ctx,egg_http_get_status(reqid));
}
 
static int egg_wasm_http_get_status(wasm_exec_env_t ee,int reqid) {
  return egg_http_get_status(reqid);
}
#endif

/* egg_http_get_header
 */
 
#if EGG_ENABLE_VM
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
#endif

/* egg_http_get_body
 */

#if EGG_ENABLE_VM
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
#endif

/* egg_ws_connect
 */

#if EGG_ENABLE_VM
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
#endif

/* egg_ws_disconnect
 */
 
#if EGG_ENABLE_VM
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
#endif

/* egg_ws_get_message
 */
 
#if EGG_ENABLE_VM
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
#endif

/* egg_ws_send
 */
 
#if EGG_ENABLE_VM
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
#endif

/* egg_time_real
 */
 
#if EGG_ENABLE_VM
static JSValue egg_js_time_real(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  return JS_NewFloat64(ctx,egg_time_real());
}
 
static double egg_wasm_time_real(wasm_exec_env_t ee) {
  return egg_time_real();
}
#endif

/* egg_time_get
 */
 
#if EGG_ENABLE_VM
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
#endif

/* egg_get_user_languages
 */

#if EGG_ENABLE_VM
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
#endif

/* egg_request_termination
 */
 
#if EGG_ENABLE_VM
static JSValue egg_js_request_termination(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  egg_request_termination();
  return JS_NULL;
}

static void egg_wasm_request_termination(wasm_exec_env_t ee) {
  egg_request_termination();
}
#endif

/* egg_is_terminable
 * Our answer is always one; don't bother calling the inner implementation.
 */
 
#if EGG_ENABLE_VM
static JSValue egg_js_is_terminable(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  return JS_NewInt32(ctx,1);
}

static int egg_wasm_is_terminable(wasm_exec_env_t ee) {
  return 1;
}
#endif

/* Main export tables.
 */
 
#if EGG_ENABLE_VM
static const JSCFunctionListEntry egg_native_js_exports[]={
  JS_CFUNC_DEF("log",0,egg_js_log),
  JS_CFUNC_DEF("event_next",0,egg_js_event_next),
  JS_CFUNC_DEF("event_enable",0,egg_js_event_enable),
  JS_CFUNC_DEF("input_device_get_name",0,egg_js_input_device_get_name),
  JS_CFUNC_DEF("input_device_get_ids",0,egg_js_input_device_get_ids),
  JS_CFUNC_DEF("input_device_get_button",0,egg_js_input_device_get_button),
  JS_CFUNC_DEF("input_device_disconnect",0,egg_js_input_device_disconnect),
  JS_CFUNC_DEF("show_cursor",0,egg_js_show_cursor),
  JS_CFUNC_DEF("image_get_header",0,egg_js_image_get_header),
  JS_CFUNC_DEF("image_decode",0,egg_js_image_decode),
  JS_CFUNC_DEF("texture_del",0,egg_js_texture_del),
  JS_CFUNC_DEF("texture_new",0,egg_js_texture_new),
  JS_CFUNC_DEF("texture_load_image",0,egg_js_texture_load_image),
  JS_CFUNC_DEF("texture_upload",0,egg_js_texture_upload),
  JS_CFUNC_DEF("texture_get_header",0,egg_js_texture_get_header),
  JS_CFUNC_DEF("texture_clear",0,egg_js_texture_clear),
  JS_CFUNC_DEF("draw_mode",0,egg_js_draw_mode),
  JS_CFUNC_DEF("draw_rect",0,egg_js_draw_rect),
  JS_CFUNC_DEF("draw_decal",0,egg_js_draw_decal),
  JS_CFUNC_DEF("draw_tile",0,egg_js_draw_tile),
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

static int egg_wasm_rand(wasm_exec_env_t ee) {//XXX
  return rand();
}
static int egg_wasm_main_argc_argv(wasm_exec_env_t ee,int argc,int argv) { // Required by WASI, but it won't be called.
  return 1;
}

static NativeSymbol egg_native_wasm_exports[]={
  {"egg_log",egg_wasm_log,"($i)"},
  {"egg_event_next",egg_wasm_event_next,"(*i)i"},
  {"egg_event_enable",egg_wasm_event_enable,"(ii)i"},
  {"egg_input_device_get_name",egg_wasm_input_device_get_name,"(*ii)i"},
  {"egg_input_device_get_ids",egg_wasm_input_device_get_ids,"(***i)"},
  {"egg_input_device_get_button",egg_wasm_input_device_get_button,"(*****ii)"},
  {"egg_input_device_disconnect",egg_wasm_input_device_disconnect,"(i)"},
  {"egg_show_cursor",egg_wasm_show_cursor,"(i)"},
  {"egg_image_get_header",egg_wasm_image_get_header,"(***ii)"},
  {"egg_image_decode",egg_wasm_image_decode,"(*~iii)i"},
  {"egg_texture_del",egg_wasm_texture_del,"(i)"},
  {"egg_texture_new",egg_wasm_texture_new,"()i"},
  {"egg_texture_load_image",egg_wasm_texture_load_image,"(iii)i"},
  {"egg_texture_upload",egg_wasm_texture_upload,"(iiiiiii)i"},
  {"egg_texture_get_header",egg_wasm_texture_get_header,"(***i)"},
  {"egg_texture_clear",egg_wasm_texture_clear,"(i)"},
  {"egg_draw_mode",egg_wasm_draw_mode,"(iii)"},
  {"egg_draw_rect",egg_wasm_draw_rect,"(iiiiii)"},
  {"egg_draw_decal",egg_wasm_draw_decal,"(iiiiiiiii)"},
  {"egg_draw_tile",egg_wasm_draw_tile,"(iiii)"},
  {"egg_audio_play_song",egg_wasm_audio_play_song,"(iiii)"},
  {"egg_audio_play_sound",egg_wasm_audio_play_sound,"(iiFF)"},
  {"egg_audio_get_playhead",egg_wasm_audio_get_playhead,"()F"},
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
  {"egg_time_real",egg_wasm_time_real,"()F"},
  {"egg_time_get",egg_wasm_time_get,"(*******)"},
  {"egg_get_user_languages",egg_wasm_get_user_languages,"(*i)i"},
  {"egg_request_termination",egg_wasm_request_termination,"()"},
  {"egg_is_terminable",egg_wasm_is_terminable,"()i"},
  
  //TODO Figure out what we want to expose from libc, and do it right.
  {"rand",egg_wasm_rand,"()i"},
  {"__main_argc_argv",egg_wasm_main_argc_argv,"(ii)i"},
};
#endif

/* Main entry point, expose all the above thru QuickJS and wasm-micro-runtime.
 */
 
int egg_native_install_runtime_exports() {
  #if EGG_ENABLE_VM
  JSContext *ctx=qjs_get_context(egg.qjs);
  JSValue eggjs=JS_NewObject(ctx);
  const JSCFunctionListEntry *func=egg_native_js_exports;
  int i=sizeof(egg_native_js_exports)/sizeof(egg_native_js_exports[0]);
  for (;i-->0;func++) {
    JS_SetPropertyStr(ctx,eggjs,func->name,JS_NewCFunction(ctx,func->u.func.cfunc.generic,func->name,func->u.func.length));
  }
  JSValue globals=JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx,globals,"egg",eggjs);
  JS_FreeValue(ctx,globals);
  
  if (wamr_set_exports(egg.wamr,egg_native_wasm_exports,sizeof(egg_native_wasm_exports)/sizeof(egg_native_wasm_exports[0]))<0) return -1;
  #endif
  return 0;
}

/* Stubs for wamr and qjs object lifecycle.
 * We're the only place that knows they actually don't exist, in full-native builds.
 */
 
#if !EGG_ENABLE_VM
struct qjs *qjs_new() {
  return (struct qjs*)"";
}
void qjs_del(struct qjs *qjs) {
}
struct wamr *wamr_new() {
  return (struct wamr*)"";
}
void wamr_del(struct wamr *wamr) {
}
#endif
