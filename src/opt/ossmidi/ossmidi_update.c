#include "ossmidi_internal.h"

/* Check possible device file.
 * If not MIDI, already open, or error opening, return zero, no big deal.
 */
 
static int ossmidi_try_file(struct ossmidi *ossmidi,const char *base,int basec) {
  if (!base) return 0;
  if (basec<0) { basec=0; while (base[basec]) basec++; }
  
  if ((basec<5)||memcmp(base,"midi",4)) return 0;
  int kid=0,p=4;
  for (;p<basec;p++) {
    int digit=base[p++]-'0';
    if ((digit<0)||(digit>9)) return 0;
    kid*=10;
    kid+=digit;
    if (kid>999999) return 0;
  }
  if (ossmidi_device_by_kid(ossmidi,kid)) return 0;
  
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s%.*s",ossmidi->path,basec,base);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  
  struct ossmidi_device *device=ossmidi_create_device(ossmidi);
  if (!device) return -1;
  
  if ((device->fd=open(path,O_RDONLY))<0) { // TODO should we try for write access? Unsure how that works; all my devices are read-only.
    ossmidi_disconnect(ossmidi,device);
    return 0;
  }
  device->kid=kid;
  
  if (ossmidi->delegate.cb_connect) {
    ossmidi->delegate.cb_connect(ossmidi,device);
    // Beware, delegate is free to disconnect during that call, which deletes (device).
  }
  return 0;
}

/* Scan devices directory.
 */
 
static int ossmidi_scan_now(struct ossmidi *ossmidi) {
  DIR *dir=opendir(ossmidi->path);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    if (ossmidi_try_file(ossmidi,de->d_name,-1)<0) {
      closedir(dir);
      return -1;
    }
  }
  closedir(dir);
  return 0;
}

/* Read the inotify file.
 */
 
static int ossmidi_update_inotify(struct ossmidi *ossmidi) {
  char buf[1024];
  int bufc=read(ossmidi->infd,buf,sizeof(buf));
  if (bufc<=0) {
    close(ossmidi->infd);
    ossmidi->infd=-1;
    return 0;
  }
  int bufp=0;
  while (1) {
    if (bufp>bufc-sizeof(struct inotify_event)) break;
    const struct inotify_event *event=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    if (bufp>bufc-event->len) break;
    const char *base=event->name;
    int basec=0;
    while ((basec<event->len)&&base[basec]) basec++;
    if (ossmidi_try_file(ossmidi,base,basec)<0) return -1;
    bufp+=event->len;
  }
  return 0;
}

/* Update, our poller.
 */
 
int ossmidi_update(struct ossmidi *ossmidi,int toms) {
  int pollfdc=0;
  for (;;) {
    pollfdc=ossmidi_list_files(ossmidi->pollfdv,ossmidi->pollfda,ossmidi);
    if (pollfdc<=ossmidi->pollfda) break;
    int na=(pollfdc+8)&~7;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(ossmidi->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    ossmidi->pollfdv=nv;
    ossmidi->pollfda=na;
  }
  if (pollfdc<0) return -1;
  if (!pollfdc) {
    if (toms>0) usleep(toms*1000);
    return 0;
  }
  int err=poll(ossmidi->pollfdv,pollfdc,toms);
  if (err<=0) return 0;
  const struct pollfd *pollfd=ossmidi->pollfdv;
  int i=pollfdc;
  for (;i-->0;pollfd++) {
    if (!pollfd->revents) continue;
    if (ossmidi_update_file(ossmidi,pollfd->fd)<0) return -1;
  }
  return 1;
}

/* List files, and routine maintenance.
 */
 
int ossmidi_list_files(struct pollfd *pollfdv,int pollfda,struct ossmidi *ossmidi) {

  if (ossmidi->scan) {
    ossmidi->scan=0;
    if (ossmidi_scan_now(ossmidi)<0) return -1;
  }
  
  int i=ossmidi->devicec;
  while (i-->0) {
    struct ossmidi_device *device=ossmidi->devicev[i];
    if (device->fd<0) {
      ossmidi->devicec--;
      memmove(ossmidi->devicev+i,ossmidi->devicev+i+1,sizeof(void*)*(ossmidi->devicec-i));
      if (ossmidi->delegate.cb_disconnect&&!device->suppress_cb_disconnect) {
        ossmidi->delegate.cb_disconnect(ossmidi,device);
      }
      ossmidi_device_del(device);
    }
  }
  
  int pollfdc=0;
  #define ADDFD(nfd) if (nfd>=0) { \
    if (pollfdc<pollfda) { \
      struct pollfd *pollfd=pollfdv+pollfdc; \
      memset(pollfd,0,sizeof(struct pollfd)); \
      pollfd->fd=nfd; \
      pollfd->events=POLLIN|POLLERR|POLLHUP; \
    } \
    pollfdc++; \
  }
  ADDFD(ossmidi->infd)
  struct ossmidi_device **p=ossmidi->devicev;
  for (i=ossmidi->devicec;i-->0;p++) {
    ADDFD((*p)->fd)
  }
  #undef ADDFD
  return pollfdc;
}

/* Update one file.
 */
 
int ossmidi_update_file(struct ossmidi *ossmidi,int fd) {
  if (fd<0) return -1;
  if (fd==ossmidi->infd) return ossmidi_update_inotify(ossmidi);
  struct ossmidi_device *device=ossmidi_device_by_fd(ossmidi,fd);
  if (device) return ossmidi_device_update(ossmidi,device);
  return -1;
}
