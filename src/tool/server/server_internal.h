#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H

#include "opt/http/http.h"
#include "opt/process/process.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <sys/poll.h>

extern struct server {
  const char *exename;
  struct http_context *http;
  struct process_context *proc;
  volatile int sigc;
  int port;
  char **htdocsv;
  int htdocsc,htdocsa;
  char **makeabledirv;
  int makeabledirc,makeabledira;
  struct pollfd *pollfdv;
  int pollfda;
  struct pending {
    int procfd;
    struct http_xfer *rsp;
    char *path;
  } *pendingv;
  int pendingc,pendinga;
  struct http_websocket **wsv;
  int wsc,wsa;
} server;

#endif
