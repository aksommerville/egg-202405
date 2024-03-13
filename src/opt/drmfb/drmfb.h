/* drmfb.h
 * Required:
 * Optional: hostio
 * Link:
 *
 * Direct framebuffer via Linux DRM.
 * BEWARE! I have never seen this work on Raspberry Pi 4, despite trying pretty hard.
 * (looks like a shared-memory thing, and not so much a DRM thing?)
 * Strongly recommend "drmgx" instead, that seems to work everywhere.
 */
 
#ifndef DRMFB_H
#define DRMFB_H

#include <stdint.h>

struct drmfb;

void drmfb_del(struct drmfb *drmfb);

/* Creates the context but does not perform any I/O yet.
 * Should only fail if we can determine that DRM is just not available, or out of memory.
 */
struct drmfb *drmfb_new();

struct drmfb_mode {
  int kid; // eg 1 for /dev/dri/card1; using an int so allocation is not a concern.
  int w,h; // pixels
  int rate; // hertz
  int already; // Nonzero if this mode is currently active. Makes a good case for keeping it.
  // Opaque, for internal identification:
  uint32_t connector_id;
  int modep;
};

/* Open every device file we can find, and query each of them for available modes.
 * Iteration ends when your callback returns nonzero.
 */
int drmfb_for_each_mode(
  struct drmfb *drmfb,
  int (*cb)(const struct drmfb_mode *mode,void *userdata),
  void *userdata
);

/* Make the given mode active.
 * You must set a mode before normal use.
 * Parameters are not negotiated. Everything must match exactly.
 * Ignores parameters: already
 */
int drmfb_set_mode(struct drmfb *drmfb,const struct drmfb_mode *mode);

// Null if disconnected.
const struct drmfb_mode *drmfb_get_mode(const struct drmfb *drmfb);

int drmfb_get_fd(const struct drmfb *drmfb);

// See DRM_FORMAT_* in libdrm/drm_fourcc.h
uint32_t drmfb_get_pixel_format(const struct drmfb *drmfb);

/* In the past, this would block until vsync at end.
 * But trying on 2024-03-03, after some time away, there is no blocking.
 * Not sure what's correct here.
 */
void *drmfb_begin(struct drmfb *drmfb);
int drmfb_end(struct drmfb *drmfb);

#endif
