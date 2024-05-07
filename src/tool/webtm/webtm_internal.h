#ifndef WEBTM_INTERNAL_H
#define WEBTM_INTERNAL_H

#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

extern struct webtm {

  const char *exename;
  const char *dstpath;
  const char *jspath;
  const char *htmlpath;
  const char *rompath;
  
  char *htmlsrc;
  int htmlsrcc;
  void *rom;
  int romc;
  
  struct sr_encoder jssrc;
  struct sr_encoder dst;
  
} webtm;

/* Starting from the bootstrap, minify Javascript and resolve imports.
 */
int webtm_js_compile(struct sr_encoder *dst,const char *path);

int webtm_compose_template(struct sr_encoder *dst,const char *html,int htmlc,const char *js,int jsc);
int webtm_compose_bundle(struct sr_encoder *dst,const char *html,int htmlc,const void *rom,int romc);

#endif
