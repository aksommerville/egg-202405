#ifndef DRMFB_INTERNAL_H
#define DRMFB_INTERNAL_H

#include "drmfb.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct drmfb_fb {
  uint32_t fbid;
  int handle;
  int size;
  void *v;
  int stridewords;
};

struct drmfb {
  int fd;
  struct drmfb_mode mode;
  drmModeModeInfo drmmode;
  uint32_t connid,encid,crtcid;
  drmModeCrtcPtr crtc_restore;
  struct drmfb_fb fbv[2];
  int fbp;
  uint32_t pixel_format; // See DRM_FORMAT_* in libdrm/drm_mode.h
};

#endif
