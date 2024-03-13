#ifndef OSSMIDI_INTERNAL_H
#define OSSMIDI_INTERNAL_H

#include "ossmidi.h"
#include "opt/midi/midi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/poll.h>
#include <sys/inotify.h>

struct ossmidi {
  struct ossmidi_delegate delegate;
  const char *path; // Can make non-const, but for now it's constant. Must end with slash.
  int infd;
  struct ossmidi_device **devicev;
  int devicec,devicea;
  struct pollfd *pollfdv;
  int pollfdc,pollfda;
  int scan;
};

struct ossmidi_device {
  int fd;
  int kid;
  int devid;
  struct midi_stream stream;
  int update_in_progress;
  int suppress_cb_disconnect;
};

struct ossmidi_device *ossmidi_create_device(struct ossmidi *ossmidi);

void ossmidi_device_del(struct ossmidi_device *device);
struct ossmidi_device *ossmidi_device_new();
int ossmidi_device_update(struct ossmidi *ossmidi,struct ossmidi_device *device);

#endif
