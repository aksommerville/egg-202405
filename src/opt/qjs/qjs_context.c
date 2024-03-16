#include "qjs_internal.h"

/* Delete.
 */
 
static void qjs_function_cleanup(struct qjs *qjs,struct qjs_function *function) {
  JS_FreeValue(qjs->jsctx,function->jsfn);
}

static void qjs_module_cleanup(struct qjs *qjs,struct qjs_module *module) {
  if (module->functionv) {
    struct qjs_function *function=module->functionv;
    int i=module->functionc;
    for (;i-->0;function++) qjs_function_cleanup(qjs,function);
    free(module->functionv);
  }
  JS_FreeValue(qjs->jsctx,module->jsmod);
}

static void qjs_hostmod_cleanup(struct qjs *qjs,struct qjs_hostmod *hostmod) {
  if (hostmod->name) free(hostmod->name);
  if (hostmod->entryv) free(hostmod->entryv);
}

void qjs_del(struct qjs *qjs) {
  if (!qjs) return;
  if (qjs->modulev) {
    struct qjs_module *module=qjs->modulev;
    int i=qjs->modulec;
    for (;i-->0;module++) qjs_module_cleanup(qjs,module);
    free(qjs->modulev);
  }
  if (qjs->hostmodv) {
    struct qjs_hostmod *hostmod=qjs->hostmodv;
    int i=qjs->hostmodc;
    for (;i-->0;hostmod++) qjs_hostmod_cleanup(qjs,hostmod);
    free(qjs->hostmodv);
  }
  if (qjs->jsctx) JS_FreeContext(qjs->jsctx);
  if (qjs->jsrt) JS_FreeRuntime(qjs->jsrt);
  if (qjs->tmpv) free(qjs->tmpv);
  free(qjs);
}

/* Grow temporary array.
 */
 
static int qjs_tmpv_require(struct qjs *qjs,int c) {
  if (c<=qjs->tmpa) return 0;
  if (c>256) return -1;
  int na=(c+8)&~7;
  void *nv=realloc(qjs->tmpv,sizeof(JSValue)*na);
  if (!nv) return -1;
  qjs->tmpv=nv;
  qjs->tmpa=na;
  return 0;
}

/* Check for exceptions.
 */
 
int qjs_check_js_exception(struct qjs *qjs,JSValue v) {
  if (!JS_IsException(v)) return 0;
  JSValue exception=JS_GetException(qjs->jsctx);
  JSValue stack=JS_GetPropertyStr(qjs->jsctx,exception,"stack");
  const char *reprd=JS_ToCString(qjs->jsctx,exception);
  if (reprd) {
    fprintf(stderr,"JS EXCEPTION: %s\n",reprd);
    JS_FreeCString(qjs->jsctx,reprd);
  } else {
    fprintf(stderr,"JS EXCEPTION\n");
  }
  reprd=JS_ToCString(qjs->jsctx,stack);
  if (reprd) {
    fprintf(stderr,"%s",reprd);
    JS_FreeCString(qjs->jsctx,reprd);
  }
  JS_FreeValue(qjs->jsctx,exception);
  JS_FreeValue(qjs->jsctx,stack);
  return -2;
}

/* Our module that provides an exporter.
 */
 
static JSValue qjs_exportModule(JSContext *ctx,JSValueConst this,int argc,JSValueConst *argv) {
  struct qjs *qjs=JS_GetContextOpaque(ctx);
  if (argc!=1) return JS_ThrowTypeError(ctx,"exportModule requires 1 argument");
  if (!qjs->module_loading) return JS_ThrowTypeError(ctx,"exportModule called from outside a module's bootstrap");
  JS_FreeValue(qjs->jsctx,qjs->module_loading->jsmod);
  qjs->module_loading->jsmod=JS_DupValue(qjs->jsctx,argv[0]);
  return JS_NULL;
}

/* Global module loader.
 */
 
static int qjs_dummy_module_init(JSContext *ctx,JSModuleDef *module) {
  struct qjs *qjs=JS_GetContextOpaque(ctx);
  JSAtom nameatom=JS_GetModuleName(ctx,module);
  const char *name=JS_AtomToCString(ctx,nameatom);
  if (!name) return -1;
  struct qjs_hostmod *hostmod=qjs->hostmodv;
  int i=qjs->hostmodc;
  for (;i-->0;hostmod++) {
    if (strcmp(name,hostmod->name)) continue;
    JS_FreeCString(ctx,name);
    return JS_SetModuleExportList(ctx,module,hostmod->entryv,hostmod->entryc);
  }
  JS_FreeCString(ctx,name);
  return -1;
}
 
static JSModuleDef *qjs_module_loader(JSContext *ctx,const char *name,void *opaque) {
  struct qjs *qjs=JS_GetContextOpaque(ctx);
  struct qjs_hostmod *hostmod=qjs->hostmodv;
  int i=qjs->hostmodc;
  for (;i-->0;hostmod++) {
    if (strcmp(name,hostmod->name)) continue;
    JSModuleDef *module=JS_NewCModule(ctx,hostmod->name,qjs_dummy_module_init);
    if (!module) return 0;
    JS_AddModuleExportList(ctx,module,hostmod->entryv,hostmod->entryc);
    return module;
  }
  return 0;
}

/* New.
 */

struct qjs *qjs_new() {
  struct qjs *qjs=calloc(1,sizeof(struct qjs));
  if (!qjs) return 0;
  
  if (
    !(qjs->jsrt=JS_NewRuntime())||
    !(qjs->jsctx=JS_NewContext(qjs->jsrt))
  ) {
    qjs_del(qjs);
    return 0;
  }
  JS_SetContextOpaque(qjs->jsctx,qjs);
  JS_SetModuleLoaderFunc(qjs->jsrt,0,qjs_module_loader,0);
  
  JSValue globals=JS_GetGlobalObject(qjs->jsctx);
  JSValue exportModule=JS_NewCFunction(qjs->jsctx,qjs_exportModule,"exportModule",1);
  JS_SetPropertyStr(qjs->jsctx,globals,"exportModule",exportModule);
  JS_FreeValue(qjs->jsctx,globals);
  
  return qjs;
}

/* Trivial accessors.
 */
 
void *qjs_get_context(struct qjs *qjs) {
  if (!qjs) return 0;
  return qjs->jsctx;
}

/* Set exports.
 */

int qjs_set_exports(struct qjs *qjs,const char *modname,const void *entryv,int entryc) {
  if (qjs->hostmodc>=qjs->hostmoda) {
    int na=qjs->hostmoda+8;
    if (na>INT_MAX/sizeof(struct qjs_hostmod)) return -1;
    void *nv=realloc(qjs->hostmodv,sizeof(struct qjs_hostmod)*na);
    if (!nv) return -1;
    qjs->hostmodv=nv;
    qjs->hostmoda=na;
  }
  struct qjs_hostmod *hostmod=qjs->hostmodv+qjs->hostmodc++;
  memset(hostmod,0,sizeof(struct qjs_hostmod));
  
  if (
    !(hostmod->name=strdup(modname))||
    !(hostmod->entryv=malloc(sizeof(JSCFunctionListEntry)*entryc))
  ) {
    qjs_hostmod_cleanup(qjs,hostmod);
    qjs->hostmodc--;
    return -1;
  }
  memcpy(hostmod->entryv,entryv,sizeof(JSCFunctionListEntry)*entryc);
  hostmod->entryc=entryc;

  return 0;
}

/* Add module.
 */

int qjs_add_module(struct qjs *qjs,int modid,const void *v,int c,const char *refname) {
  int err;
  if (qjs->module_loading) return -1;
  if (qjs->modulec>=qjs->modulea) {
    int na=qjs->modulea+8;
    if (na>INT_MAX/sizeof(struct qjs_module)) return -1;
    void *nv=realloc(qjs->modulev,sizeof(struct qjs_module)*na);
    if (!nv) return -1;
    qjs->modulev=nv;
    qjs->modulea=na;
  }
  struct qjs_module *module=qjs->modulev+qjs->modulec++;
  memset(module,0,sizeof(struct qjs_module));
  module->modid=modid;
  
  // quickjs requires input text to be nul-terminated, and we will not pass that daffy requirement to our consumers.
  char *ztext=malloc(c+1);
  if (!ztext) {
    qjs->modulec--;
    return -1;
  }
  memcpy(ztext,v,c);
  ztext[c]=0;
  
  /* The result when loading with JS_EVAL_TYPE_MODULE is undefined,
   * and any 'exports' in that module are mysteriously unavailable to us.
   * We could use the default JS_EVAL_TYPE_GLOBAL, but then we can't import other Javascript!
   * So we're going to invent our own export function and expose it to clients.
   */
  qjs->module_loading=module;
  JSValue result=JS_Eval(qjs->jsctx,ztext,c,refname,JS_EVAL_TYPE_MODULE|JS_EVAL_FLAG_STRICT);
  qjs->module_loading=0;
  free(ztext);
  if ((err=qjs_check_js_exception(qjs,result))<0) {
    qjs->modulec--;
    return err;
  }
  JS_FreeValue(qjs->jsctx,result);
  
  return 0;
}

/* Add function.
 */
 
int qjs_link_function(struct qjs *qjs,int modid,int fnid,const char *name) {
  struct qjs_module *module=qjs->modulev;
  int modulei=qjs->modulec;
  for (;modulei-->0;module++) {
    if (module->modid!=modid) continue;
    
    if (module->functionc>=module->functiona) {
      int na=module->functiona+8;
      if (na>INT_MAX/sizeof(struct qjs_function)) return -1;
      void *nv=realloc(module->functionv,sizeof(struct qjs_function)*na);
      if (!nv) return -1;
      module->functionv=nv;
      module->functiona=na;
    }
    struct qjs_function *function=module->functionv+module->functionc++;
    memset(function,0,sizeof(struct qjs_function));
    function->fnid=fnid;
    
    JSValue fnobj=JS_GetPropertyStr(qjs->jsctx,module->jsmod,name);
    if (qjs_check_js_exception(qjs,fnobj)<0) {
      module->functionc--;
      return -1;
    }
    if (!JS_IsFunction(qjs->jsctx,fnobj)) {
      module->functionc--;
      return -1;
    }
    function->jsfn=fnobj;
    
    return 0;
  }
  return -1;
}

/* Locate function by id.
 */
 
static struct qjs_function *qjs_function_by_fnid(const struct qjs *qjs,int modid,int fnid) {
  struct qjs_module *module=qjs->modulev;
  int modulei=qjs->modulec;
  for (;modulei-->0;module++) {
    if (module->modid!=modid) continue;
    struct qjs_function *function=module->functionv;
    int functioni=module->functionc;
    for (;functioni-->0;function++) {
      if (function->fnid!=fnid) continue;
      return function;
    }
    return 0;
  }
  return 0;
}

/* Call function.
 */

int qjs_call(struct qjs *qjs,int modid,int fnid,uint32_t *argv,int argc) {
  struct qjs_function *function=qjs_function_by_fnid(qjs,modid,fnid);
  if (!function) return -1;
  
  if (qjs_tmpv_require(qjs,argc?argc:1)<0) return -1;
  JSValue *dst=qjs->tmpv;
  uint32_t *src=argv;
  int argi=argc;
  for (;argi-->0;dst++,src++) {
    *dst=JS_NewInt32(qjs->jsctx,(int32_t)(*src));
  }
      
  JSValue result=JS_Call(qjs->jsctx,function->jsfn,JS_NULL,argc,qjs->tmpv);
  if (qjs_check_js_exception(qjs,result)<0) {
    JS_FreeValue(qjs->jsctx,result);
    return -1;
  }
  JS_ToInt32(qjs->jsctx,(int32_t*)argv,result);
  JS_FreeValue(qjs->jsctx,result);
  return 0;
}

/* Call function with variadic arguments.
 */
 
int qjs_callf(struct qjs *qjs,int modid,int fnid,const char *fmt,...) {
  struct qjs_function *function=qjs_function_by_fnid(qjs,modid,fnid);
  if (!function) return -1;
  
  va_list vargs;
  va_start(vargs,fmt);
  int fmtc=0;
  while (fmt[fmtc]) fmtc++; // Might be >argc but can't be less.
  if (qjs_tmpv_require(qjs,fmtc)<0) return -1;
  JSValue *dst=qjs->tmpv;
  int argc=0;
  while (*fmt) {
    char argtype=*fmt++;
    int len=-1;
    if (argtype=='*') {
      if (!*fmt) return -1;
      len=va_arg(vargs,int);
      argtype=*fmt++;
    }
    switch (argtype) {
      case 'i': *dst=JS_NewInt32(qjs->jsctx,va_arg(vargs,int)); break;
      case 'f': *dst=JS_NewFloat64(qjs->jsctx,va_arg(vargs,double)); break;
      case 's': {
          const char *src=va_arg(vargs,char*);
          if (!src) len=0; else if (len<0) { len=0; while (src[len]) len++; }
          *dst=JS_NewStringLen(qjs->jsctx,src,len);
        } break;
      case 'o': *dst=JS_DupValue(qjs->jsctx,*va_arg(vargs,JSValue*)); break;
      default: {
          while (argc-->0) JS_FreeValue(qjs->jsctx,qjs->tmpv[argc]);
          return -1;
        }
    }
    dst++;
    argc++;
  }
      
  JSValue result=JS_Call(qjs->jsctx,function->jsfn,JS_NULL,argc,qjs->tmpv);
  while (argc-->0) JS_FreeValue(qjs->jsctx,qjs->tmpv[argc]);
  if (qjs_check_js_exception(qjs,result)<0) {
    JS_FreeValue(qjs->jsctx,result);
    return -1;
  }
  int32_t resulti=0;
  JS_ToInt32(qjs->jsctx,&resulti,result);
  JS_FreeValue(qjs->jsctx,result);
  if (resulti>=0) return resulti;
  return 0;
}
