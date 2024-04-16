#include "lowasm_internal.h"

#define TOPLINE_LIMIT 40 /* 320/8, this is absolutely as far as we can go */
#define EVENT_LIMIT 21 /* (180-8)/8 */

struct menu_joystick {
  struct menu hdr;
  struct device {
    int devid;
    char topline[TOPLINE_LIMIT];
    int toplinec;
    struct event {
      int btnid;
      int value;
    } eventv[EVENT_LIMIT];
    int eventp; // circular buffer; this points to the oldest. (0,0) are invalid
  } *devicev;
  int devicec,devicea;
  int devicep;
  int framec; // for ui animation
};

#define MENU ((struct menu_joystick*)menu)

static void device_cleanup(struct device *device) {
}

static void _joystick_del(struct menu *menu) {
  if (MENU->devicev) {
    while (MENU->devicec-->0) device_cleanup(MENU->devicev+MENU->devicec);
    free(MENU->devicev);
  }
}

static int device_acquire_topline(struct device *device) {
  char *dst=device->topline;
  const int dsta=TOPLINE_LIMIT;
  int dstc=0;
  
  int vid=0,pid=0,version=0;
  egg_input_device_get_ids(&vid,&pid,&version,device->devid);
  if (vid||pid||version) {
    dst[dstc++]="0123456789abcdef"[(vid>>12)&0xf];
    dst[dstc++]="0123456789abcdef"[(vid>> 8)&0xf];
    dst[dstc++]="0123456789abcdef"[(vid>> 4)&0xf];
    dst[dstc++]="0123456789abcdef"[(vid>> 0)&0xf];
    dst[dstc++]=':';
    dst[dstc++]="0123456789abcdef"[(pid>>12)&0xf];
    dst[dstc++]="0123456789abcdef"[(pid>> 8)&0xf];
    dst[dstc++]="0123456789abcdef"[(pid>> 4)&0xf];
    dst[dstc++]="0123456789abcdef"[(pid>> 0)&0xf];
  }
  if (version) { // version is zero often enough that we should skip it when so. space is at a premium here.
    dst[dstc++]=':';
    dst[dstc++]="0123456789abcdef"[(version>>12)&0xf];
    dst[dstc++]="0123456789abcdef"[(version>> 8)&0xf];
    dst[dstc++]="0123456789abcdef"[(version>> 4)&0xf];
    dst[dstc++]="0123456789abcdef"[(version>> 0)&0xf];
  }
  dst[dstc++]=' ';
  
  char tmp[256];
  int tmpc=egg_input_device_get_name(tmp,sizeof(tmp),device->devid);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) tmpc=0;
  if (dstc>dsta-tmpc) tmpc=dsta-dstc;
  memcpy(dst+dstc,tmp,tmpc);
  dstc+=tmpc;
  
  device->toplinec=dstc;
  return 0;
}

static int joystick_add_device(struct menu *menu,int devid) {
  int i=MENU->devicec; while (i-->0) {
    if (MENU->devicev[i].devid==devid) return -1;
  }
  if (MENU->devicec>=MENU->devicea) {
    int na=MENU->devicea+8;
    if (na>INT_MAX/sizeof(struct device)) return -1;
    void *nv=realloc(MENU->devicev,sizeof(struct device)*na);
    if (!nv) return -1;
    MENU->devicev=nv;
    MENU->devicea=na;
  }
  struct device *device=MENU->devicev+MENU->devicec++;
  memset(device,0,sizeof(struct device));
  device->devid=devid;
  device_acquire_topline(device);
  return 0;
}

static void joystick_remove_device(struct menu *menu,int devid) {
  int i=MENU->devicec; while (i-->0) {
    struct device *device=MENU->devicev+i;
    if (device->devid!=devid) continue;
    device_cleanup(device);
    MENU->devicec--;
    memmove(device,device+1,sizeof(struct device)*(MENU->devicec-i));
    if (MENU->devicep&&(i<=MENU->devicep)) MENU->devicep--;
    return;
  }
}

static void joystick_log_event(struct menu *menu,int devid,int btnid,int value) {
  struct device *device=MENU->devicev;
  int i=MENU->devicec;
  for (;i-->0;device++) {
    if (device->devid!=devid) continue;
    struct event *event=device->eventv+device->eventp++;
    if (device->eventp>=EVENT_LIMIT) device->eventp=0;
    event->btnid=btnid;
    event->value=value;
    return;
  }
}

static void _joystick_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_KEY: if (event->v[1]) switch (event->v[0]) {
        case 0x0007004f: if (MENU->devicep<MENU->devicec-1) MENU->devicep++; break;
        case 0x00070050: if (MENU->devicep>0) MENU->devicep--; break;
      } break;
    case EGG_EVENT_CONNECT: joystick_add_device(menu,event->v[0]); break;
    case EGG_EVENT_DISCONNECT: joystick_remove_device(menu,event->v[0]); break;
    case EGG_EVENT_INPUT: joystick_log_event(menu,event->v[0],event->v[1],event->v[2]); break;
  }
}

static void _joystick_render(struct menu *menu) {
  lowasm_tiles_begin(lowasm.texid_font,0xffffffff,0xff);
  if ((MENU->devicep>=0)&&(MENU->devicep<MENU->devicec)) {
    struct device *device=MENU->devicev+MENU->devicep;
    lowasm_tiles_string(device->topline,device->toplinec,4,4);
    int y=16,p=device->eventp,i=EVENT_LIMIT;
    for (;i-->0;p++) {
      if (p>=EVENT_LIMIT) p=0;
      const struct event *event=device->eventv+p;
      if (!event->btnid&&!event->value) continue;
      lowasm_tiles_stringf(16,y,"%d = %d",event->btnid,event->value);
      y+=8;
    }
  }
  MENU->framec++;
  if (MENU->framec&0x10) {
    if (MENU->devicep>0) {
      lowasm_tile(4,lowasm.screenh>>1,'<',0);
    }
    if (MENU->devicep<MENU->devicec-1) {
      lowasm_tile(lowasm.screenw-4,lowasm.screenh>>1,'>',0);
    }
  }
  lowasm_tiles_end();
}

struct menu *lowasm_menu_new_joystick() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_joystick));
  if (!menu) return 0;
  menu->del=_joystick_del;
  menu->event=_joystick_event;
  menu->render=_joystick_render;
  int i=0; for (;i<lowasm.devidc;i++) joystick_add_device(menu,lowasm.devidv[i]);
  return menu;
}
