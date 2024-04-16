#include "lowasm_internal.h"

#define EVENT_COUNT 22 /* 180/8 */

struct menu_mouse {
  struct menu hdr;
  struct egg_event eventv[EVENT_COUNT];
  int eventp; // oldest (circular)
};

#define MENU ((struct menu_mouse*)menu)

static void _mouse_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_MMOTION: {
        // Platform may send redundant motion events. Detect and ignore them.
        int p=MENU->eventp-1;
        int i=EVENT_COUNT;
        while (i-->0) {
          if (p<0) p=EVENT_COUNT-1;
          const struct egg_event *pv=MENU->eventv+p;
          if (pv->type!=EGG_EVENT_MMOTION) continue;
          if ((pv->v[0]==event->v[0])&&(pv->v[1]==event->v[1])) return;
          break;
        }
      } // pass
    case EGG_EVENT_MBUTTON:
    case EGG_EVENT_MWHEEL: {
        MENU->eventv[MENU->eventp++]=*event;
        if (MENU->eventp>=EVENT_COUNT) MENU->eventp=0;
      } break;
  }
}

static void _mouse_render(struct menu *menu) {
  int y=6,p=MENU->eventp,i=EVENT_COUNT;
  lowasm_tiles_begin(lowasm.texid_font,0xffffffff,0xff);
  for (;i-->0;p++) {
    if (p>=EVENT_COUNT) p=0;
    const struct egg_event *event=MENU->eventv+p;
    if (!event->type) continue;
    switch (event->type) {
      case EGG_EVENT_MMOTION: lowasm_tiles_stringf(16,y,"move %d,%d",event->v[0],event->v[1]); break;
      case EGG_EVENT_MBUTTON: lowasm_tiles_stringf(16,y,"btn %d=%d @%d,%d",event->v[0],event->v[1],event->v[2],event->v[3]); break;
      case EGG_EVENT_MWHEEL: lowasm_tiles_stringf(16,y,"wheel %d,%d @%d,%d",event->v[0],event->v[1],event->v[2],event->v[3]); break;
      default: continue;
    }
    y+=8;
  }
  lowasm_tiles_end();
}

struct menu *lowasm_menu_new_mouse() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_mouse));
  if (!menu) return 0;
  menu->event=_mouse_event;
  menu->render=_mouse_render;
  return menu;
}
