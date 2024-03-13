#ifndef QJS_INTERNAL_H
#define QJS_INTERNAL_H

#include "qjs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include "quickjs.h"

struct qjs_function {
  int fnid;
  JSValue jsfn;
};

struct qjs_module {
  int modid;
  JSValue jsmod;
  struct qjs_function *functionv;
  int functionc,functiona;
};

struct qjs_hostmod {
  char *name;
  JSCFunctionListEntry *entryv;
  int entryc;
};

struct qjs {
  JSRuntime *jsrt;
  JSContext *jsctx;
  struct qjs_module *modulev;
  int modulec,modulea;
  struct qjs_module *module_loading; // WEAK, present only during the initial run of a module.
  JSValue *tmpv;
  int tmpa;
  struct qjs_hostmod *hostmodv;
  int hostmodc,hostmoda;
};

#endif
