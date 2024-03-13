#include "ossmidi_internal.h"

/* Delete.
 */
 
void ossmidi_del(struct ossmidi *ossmidi) {
  if (!ossmidi) return;
  if (ossmidi->devicev) {
    while (ossmidi->devicec-->0) ossmidi_device_del(ossmidi->devicev[ossmidi->devicec]);
    free(ossmidi->devicev);
  }
  if (ossmidi->pollfdv) free(ossmidi->pollfdv);
  free(ossmidi);
}

/* New.
 */

struct ossmidi *ossmidi_new(const struct ossmidi_delegate *delegate) {
  struct ossmidi *ossmidi=calloc(1,sizeof(struct ossmidi));
  if (!ossmidi) return 0;
  ossmidi->scan=1;
  ossmidi->path="/dev/";
  if (delegate) ossmidi->delegate=*delegate;
  if ((ossmidi->infd=inotify_init())>=0) {
    inotify_add_watch(ossmidi->infd,"/dev",IN_CREATE|IN_ATTRIB|IN_MOVED_TO);
  }
  return ossmidi;
}

/* Trivial accessors.
 */
  
void *ossmidi_get_userdata(const struct ossmidi *ossmidi) {
  return ossmidi->delegate.userdata;
}

void ossmidi_rescan_soon(struct ossmidi *ossmidi) {
  ossmidi->scan=1;
}

/* Public access to device list.
 */

struct ossmidi_device *ossmidi_device_by_index(const struct ossmidi *ossmidi,int p) {
  if (p<0) return 0;
  if (p>=ossmidi->devicec) return 0;
  return ossmidi->devicev[p];
}

struct ossmidi_device *ossmidi_device_by_devid(const struct ossmidi *ossmidi,int devid) {
  struct ossmidi_device **p=ossmidi->devicev;
  int i=ossmidi->devicec;
  for (;i-->0;p++) {
    if ((*p)->devid!=devid) continue;
    return *p;
  }
  return 0;
}

struct ossmidi_device *ossmidi_device_by_fd(const struct ossmidi *ossmidi,int fd) {
  struct ossmidi_device **p=ossmidi->devicev;
  int i=ossmidi->devicec;
  for (;i-->0;p++) {
    if ((*p)->fd!=fd) continue;
    return *p;
  }
  return 0;
}

struct ossmidi_device *ossmidi_device_by_kid(const struct ossmidi *ossmidi,int kid) {
  struct ossmidi_device **p=ossmidi->devicev;
  int i=ossmidi->devicec;
  for (;i-->0;p++) {
    if ((*p)->kid!=kid) continue;
    return *p;
  }
  return 0;
}

/* Disconnect device.
 */

void ossmidi_disconnect(struct ossmidi *ossmidi,struct ossmidi_device *device) {
  if (!ossmidi||!device) return;
  
  /* Our client is allowed to disconnect devices while they're updating.
   * If that happens, close its file and it will self-destruct gracefully at the next update.
   */
  if (device->update_in_progress) {
    device->suppress_cb_disconnect=1;
    if (device->fd>=0) {
      close(device->fd);
      device->fd=-1;
    }
    return;
  }
  
  /* Otherwise, we're free to drop the device immediately.
   */
  int i=ossmidi->devicec;
  while (i-->0) {
    if (ossmidi->devicev[i]!=device) continue;
    ossmidi->devicec--;
    memmove(ossmidi->devicev+i,ossmidi->devicev+i+1,sizeof(void*)*(ossmidi->devicec-i));
    ossmidi_device_del(device);
    return;
  }
}

/* Add new device.
 */
 
struct ossmidi_device *ossmidi_create_device(struct ossmidi *ossmidi) {
  if (ossmidi->devicec>=ossmidi->devicea) {
    int na=ossmidi->devicea+8;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(ossmidi->devicev,sizeof(void*)*na);
    if (!nv) return 0;
    ossmidi->devicev=nv;
    ossmidi->devicea=na;
  }
  struct ossmidi_device *device=ossmidi_device_new();
  if (!device) return 0;
  ossmidi->devicev[ossmidi->devicec++]=device;
  return device;
}
