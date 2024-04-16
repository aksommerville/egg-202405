/* menu_touch.c
 * Coded blindly because our native client doesn't do touch, and web client doesn't provide stdlib yet.
 */

#include "lowasm_internal.h"

#define TILE_COUNT 100

struct menu_touch {
  struct menu hdr;
  struct egg_draw_tile tilev[TILE_COUNT]; // oldest first
  int tilec;
  int framec;
};

#define MENU ((struct menu_touch*)menu)

static void _touch_del(struct menu *menu) {
}

static void touch_add_point(struct menu *menu,int x,int y) {
  if (MENU->tilec) {
    const struct egg_draw_tile *pv=MENU->tilev+MENU->tilec-1;
    if ((x==pv->x)&&(x==pv->y)) return;
    if (MENU->tilec>=TILE_COUNT) {
      memmove(MENU->tilev,MENU->tilev+1,sizeof(egg_draw_tile)*(TILE_COUNT-1));
      MENU->tilec--;
    }
  }
  struct egg_draw_tile *tile=MENU->tilev+MENU->tilec++;
  tile->x=x;
  tile->y=y;
  tile->tileid=0x00; // 0x00..0x0f = newest..oldest
  tile->xform=0;
}

static void _touch_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_TOUCH: {
        // v[0]=id v[1]=state(0,1,2)=(off,on,move). We're not using those.
        touch_add_point(menu,event->v[0],event->v[1]);
      } break;
  }
}

static void _touch_render(struct menu *menu) {
  if (++(MENU->framec)>=3) { // Raise tileid for all tiles every few frames, stopping at 0xf.
    struct egg_draw_tile *tile=MENU->tilev;
    int i=MENU->tilec;
    for (;i-->0;tile++) {
      if (tile->tileid<0xf) tile->tileid++;
    }
  }
  egg_draw_tile(1,lowasm.texid_misc,MENU->tilev,MENU->tilec);
}

struct menu *lowasm_menu_new_touch() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_touch));
  if (!menu) return 0;
  menu->del=_touch_del;
  menu->event=_touch_event;
  menu->render=_touch_render;
  return menu;
}
