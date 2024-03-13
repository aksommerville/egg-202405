#ifndef WAMR_INTERNAL_H
#define WAMR_INTERNAL_H

#include "wamr.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "wasm_c_api.h"
#include "wasm_export.h"

struct wamr_function {
  int fnid;
  wasm_function_inst_t finst;
};

struct wamr_module {
  int modid;
  wasm_module_t module;
  wasm_module_inst_t instance;
  wasm_exec_env_t ee;
  struct wamr_function *functionv;
  int functionc,functiona;
};

struct wamr {
  struct wamr_module *modulev;
  int modulec,modulea;
};

#endif
