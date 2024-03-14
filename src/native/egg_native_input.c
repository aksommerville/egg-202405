#include "egg_native_internal.h"

/* Cleanup.
 */
 
static void egg_input_device_cleanup(struct egg_input_device *device) {
  if (device->buttonv) free(device->buttonv);
}

void egg_native_input_cleanup() {
  if (egg.input_devicev) {
    while (egg.input_devicec-->0) egg_input_device_cleanup(egg.input_devicev+egg.input_devicec);
    free(egg.input_devicev);
  }
}

/* Device list.
 */
 
static int egg_input_devicev_search(int devid) {
  int lo=0,hi=egg.input_devicec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct egg_input_device *device=egg.input_devicev+ck;
         if (devid<device->devid) hi=ck;
    else if (devid>device->devid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct egg_input_device *egg_input_devicev_insert(int p,int devid) {
  if ((p<0)||(p>egg.input_devicec)) return 0;
  if (p&&(devid<=egg.input_devicev[p-1].devid)) return 0;
  if ((p<egg.input_devicec)&&(devid>=egg.input_devicev[p].devid)) return 0;
  if (egg.input_devicec>=egg.input_devicea) {
    int na=egg.input_devicea+16;
    if (na>INT_MAX/sizeof(struct egg_input_device)) return 0;
    void *nv=realloc(egg.input_devicev,sizeof(struct egg_input_device)*na);
    if (!nv) return 0;
    egg.input_devicev=nv;
    egg.input_devicea=na;
  }
  struct egg_input_device *device=egg.input_devicev+p;
  memmove(device+1,device,sizeof(struct egg_input_device)*(egg.input_devicec-p));
  egg.input_devicec++;
  memset(device,0,sizeof(struct egg_input_device));
  device->devid=devid;
  return device;
}

static void egg_input_device_remove(int p) {
  if ((p<0)||(p>=egg.input_devicec)) return;
  struct egg_input_device *device=egg.input_devicev+p;
  egg_input_device_cleanup(device);
  egg.input_devicec--;
  memmove(device,device+1,sizeof(struct egg_input_device)*(egg.input_devicec-p));
}

/* Device list, for event unit's consumption.
 */
 
void egg_native_input_add_device(struct hostio_input *driver,int devid) {
  if (devid<1) return;
  int p=egg_input_devicev_search(devid);
  if (p>=0) return;
  p=-p-1;
  struct egg_input_device *device=egg_input_devicev_insert(p,devid);
  if (!device) return;
  device->driver=driver;
}

void egg_native_input_remove_device(int devid) {
  int p=egg_input_devicev_search(devid);
  if (p<0) return;
  egg_input_device_remove(p);
}

/* Device's button list.
 */
 
static struct egg_input_button *egg_input_device_add_button(struct egg_input_device *device) {
  if (device->buttonc>=device->buttona) {
    int na=device->buttona+32;
    if (na>INT_MAX/sizeof(struct egg_input_button)) return 0;
    void *nv=realloc(device->buttonv,sizeof(struct egg_input_button)*na);
    if (!nv) return 0;
    device->buttonv=nv;
    device->buttona=na;
  }
  struct egg_input_button *button=device->buttonv+device->buttonc++;
  memset(button,0,sizeof(struct egg_input_button));
  return button;
}

/* Public: Get device name.
 */

int egg_input_device_get_name(char *dst,int dsta,int devid) {
  if (!dst||(dsta<0)) dsta=0;
  if (dsta) dst[0]=0;
  if (devid<1) return 0;
  int p=egg_input_devicev_search(devid);
  if (p<0) return 0;
  const struct egg_input_device *device=egg.input_devicev+p;
  if (!device->driver) return 0;
  if (!device->driver->type->get_ids) return 0;
  int vid,pid,version;
  const char *src=device->driver->type->get_ids(&vid,&pid,&version,device->driver,devid);
  if (!src) return 0;
  int srcc=0; while (src[srcc]) srcc++;
  if (srcc<=dsta) {
    memcpy(dst,src,srcc);
    if (srcc<dsta) dst[srcc]=0;
  }
  return srcc;
}

/* Public: Get device IDs.
 */

void egg_input_device_get_ids(int *vid,int *pid,int *version,int devid) {
  if (vid) *vid=0;
  if (pid) *pid=0;
  if (version) *version=0;
  if (devid<1) return;
  int p=egg_input_devicev_search(devid);
  if (p<0) return;
  const struct egg_input_device *device=egg.input_devicev+p;
  if (!device->driver) return;
  if (!device->driver->type->get_ids) return;
  device->driver->type->get_ids(vid,pid,version,device->driver,devid);
}

/* Public: Get button.
 * We cache the entire set the first time client asks for one.
 * This is ugly and wasteful, but necessary because I don't want to hand callbacks across the client/platform boundary.
 */
 
static int egg_input_cb_get_button(int btnid,int hidusage,int lo,int hi,int value,void *userdata) {
  struct egg_input_device *device=userdata;
  struct egg_input_button *button=egg_input_device_add_button(device);
  if (!button) return -1;
  button->btnid=btnid;
  button->hidusage=hidusage;
  button->lo=lo;
  button->hi=hi;
  button->value=value;
  return 0;
}

void egg_input_device_get_button(int *btnid,int *hidusage,int *lo,int *hi,int *value,int devid,int index) {
  *btnid=*hidusage=*lo=*hi=*value=0;
  if (devid<1) return;
  if (index<0) return;
  int p=egg_input_devicev_search(devid);
  if (p<0) return;
  struct egg_input_device *device=egg.input_devicev+p;
  if (!device->buttonv) {
    if (device->driver&&device->driver->type->for_each_button) {
      device->driver->type->for_each_button(device->driver,devid,egg_input_cb_get_button,device);
    }
  }
  if (index>=device->buttonc) return;
  const struct egg_input_button *button=device->buttonv+index;
  *btnid=button->btnid;
  *hidusage=button->hidusage;
  *lo=button->lo;
  *hi=button->hi;
  *value=button->value;
}

/* Public: Disconnect device.
 */

void egg_input_device_disconnect(int devid) {
  if (devid<1) return;
  int di=0; for (;di<egg.hostio->inputc;di++) {
    struct hostio_input *driver=egg.hostio->inputv[di];
    if (driver->type->disconnect) driver->type->disconnect(driver,devid);
  }
}
