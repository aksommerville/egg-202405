#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H

#include "opt/http/http.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

extern struct server {
  const char *exename;
  struct http_context *http;
  volatile int sigc;
  int port;
  char **htdocsv;
  int htdocsc,htdocsa;
  char **makeabledirv;
  int makeabledirc,makeabledira;
} server;

#endif
