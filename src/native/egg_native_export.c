#include "egg_native_internal.h"
#include "quickjs.h"
#include "wasm_export.h"
#include "opt/strfmt/strfmt.h"

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
  fprintf(stderr,"%s: %.*s\n",__func__,tmpc,tmp);
  
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
  fprintf(stderr,"%s: %.*s\n",__func__,tmpc,tmp);
}

/* Install exports (both runtimes).
 */
 
static const JSCFunctionListEntry egg_native_js_exports[]={
  JS_CFUNC_DEF("log",0,egg_js_log),
};

//TODO Define public api

static NativeSymbol egg_native_wasm_exports[]={
  {"egg_log",egg_wasm_log,"($i)"},
};
 
int egg_native_install_runtime_exports() {
  if (qjs_set_exports(egg.qjs,"egg",egg_native_js_exports,sizeof(egg_native_js_exports)/sizeof(egg_native_js_exports[0]))<0) return -1;
  if (wamr_set_exports(egg.wamr,egg_native_wasm_exports,sizeof(egg_native_wasm_exports)/sizeof(egg_native_wasm_exports[0]))<0) return -1;
  return 0;
}
