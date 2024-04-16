#include "lowasm_internal.h"

#define EVENT_COUNT 22 /* 180/8 */

struct menu_keyboard {
  struct menu hdr;
  struct egg_event eventv[EVENT_COUNT];
  int eventp; // oldest (circular)
};

#define MENU ((struct menu_keyboard*)menu)

static void _keyboard_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_KEY:
    case EGG_EVENT_TEXT: {
        MENU->eventv[MENU->eventp++]=*event;
        if (MENU->eventp>=EVENT_COUNT) MENU->eventp=0;
      } break;
  }
}

static void _keyboard_render(struct menu *menu) {
  int y=6,p=MENU->eventp,i=EVENT_COUNT;
  lowasm_tiles_begin(lowasm.texid_font,0xffffffff,0xff);
  for (;i-->0;p++) {
    if (p>=EVENT_COUNT) p=0;
    const struct egg_event *event=MENU->eventv+p;
    if (!event->type) continue;
    char repr0[9]; // v[0] is either keycode or codepoint, either way we want it hexadecimal
    int repr0c=0;
    if (event->v[0]&0xf0000000) repr0[repr0c++]="0123456789abcdefg"[(event->v[0]>>28)&0xf];
    if (event->v[0]&0xff000000) repr0[repr0c++]="0123456789abcdefg"[(event->v[0]>>24)&0xf];
    if (event->v[0]&0xfff00000) repr0[repr0c++]="0123456789abcdefg"[(event->v[0]>>20)&0xf];
    if (event->v[0]&0xffff0000) repr0[repr0c++]="0123456789abcdefg"[(event->v[0]>>16)&0xf];
    if (event->v[0]&0xfffff000) repr0[repr0c++]="0123456789abcdefg"[(event->v[0]>>12)&0xf];
    if (event->v[0]&0xffffff00) repr0[repr0c++]="0123456789abcdefg"[(event->v[0]>> 8)&0xf];
    if (event->v[0]&0xfffffff0) repr0[repr0c++]="0123456789abcdefg"[(event->v[0]>> 4)&0xf];
                                repr0[repr0c++]="0123456789abcdefg"[(event->v[0]>> 0)&0xf];
    repr0[repr0c]=0;
    switch (event->type) {
      case EGG_EVENT_KEY: lowasm_tiles_stringf(16,y,"key %s=%d",repr0,event->v[1]); break;
      case EGG_EVENT_TEXT: lowasm_tiles_stringf(16,y,"text U+%s",repr0); break;
      default: continue;
    }
    y+=8;
  }
  lowasm_tiles_end();
}

struct menu *lowasm_menu_new_keyboard() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_keyboard));
  if (!menu) return 0;
  menu->event=_keyboard_event;
  menu->render=_keyboard_render;
  return menu;
}
