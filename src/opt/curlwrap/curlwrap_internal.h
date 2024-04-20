#ifndef CURLWRAP_INTERNAL_H
#define CURLWRAP_INTERNAL_H

#include "curlwrap.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <curl/curl.h>

#define CURLWRAP_MAGIC_HTTP 12345
#define CURLWRAP_MAGIC_WS   12346

struct curlwrap {
  struct curlwrap_delegate delegate;
  CURL *multi;
  int id_next; // reqid,wsid,msgid all share this id counter. The context is dead if it exceeds INT_MAX.
  
  struct curlwrap_http {
    int magic;
    int reqid;
    int status; // 0=pending
    char **headerv; // "KEY: VALUE", nul-terminated
    int headerc,headera;
    void *rspbody;
    int rspbodyc,rspbodya;
    char *reqbody;
    int reqbodyc,reqbodyp;
    CURL *removable; // populated only after the request completes, signal that we can remove the http and this easy.
  } **httpv;
  int httpc,httpa;
  
  struct curlwrap_ws {
    int magic;
    int wsid;
    int status; // 0=new, 1=connected, 2=disconnected (they linger after disconnect to deliver final packets)
    int almost; // Nonzero if we detected end of headers. Report connection at the next update. (Can't do it immediately).
    struct curlwrap_ws_msg {
      int msgid;
      int opcode;
      void *v;
      int c;
    } *msgv;
    int msgc,msga;
    CURL *removable;
    struct curlwrap *parent;
    CURL *easy; // Always present
  } **wsv;
  int wsc,wsa;
};

#endif
