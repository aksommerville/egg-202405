#ifndef PROCESS_INTERNAL_H
#define PROCESS_INTERNAL_H

#include "process.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/socket.h>

struct process {
  int pid,fd;
  int status;
};

struct process_context {
  struct process_delegate delegate;
  struct process *processv;
  int processc,processa;
  struct pollfd *pollfdv;
  int pollfda;
  int busy; // if nonzero, reject any process addition or removal
};

#endif
