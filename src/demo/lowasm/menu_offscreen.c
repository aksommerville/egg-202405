#include "lowasm_internal.h"

#define TEXW 96
#define TEXH 64

struct menu_offscreen {
  struct menu hdr;
  int texida,texidb;
};

#define MENU ((struct menu_offscreen*)menu)

static void _offscreen_del(struct menu *menu) {
  egg_texture_del(MENU->texida);
  egg_texture_del(MENU->texidb);
}

static void _offscreen_render(struct menu *menu) {

  #define OS1(texid,_tileid) { \
    egg_texture_clear(texid); \
    egg_draw_rect(texid,0,0,4,4,0x000000ff); \
    egg_draw_rect(texid,TEXW-4,0,4,4,0xff0000ff); \
    egg_draw_rect(texid,0,TEXH-4,4,4,0x00ff00ff); \
    egg_draw_rect(texid,TEXW-4,TEXH-4,4,4,0x0000ffff); \
    struct egg_draw_tile tile={.x=TEXW>>1,.y=TEXH>>1,.tileid=_tileid,.xform=0}; \
    egg_draw_tile(texid,lowasm.texid_misc,&tile,1); \
  }
  OS1(MENU->texida,0x1c) // cherry
  OS1(MENU->texidb,0x1d) // coin
  #undef OS1
  
  egg_draw_decal(1,MENU->texida,4,4,0,0,TEXW,TEXH,0);
  egg_draw_decal(1,MENU->texidb,4+TEXW+2,4,0,0,TEXW,TEXH,0);
}

struct menu *lowasm_menu_new_offscreen() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_offscreen));
  if (!menu) return 0;
  menu->del=_offscreen_del;
  menu->render=_offscreen_render;
  MENU->texida=egg_texture_new();
  MENU->texidb=egg_texture_new();
  if ((MENU->texida<1)||(MENU->texidb<1)) {
    _offscreen_del(menu);
    free(menu);
    return 0;
  }
  egg_texture_upload(MENU->texida,TEXW,TEXH,TEXW<<2,EGG_TEX_FMT_RGBA,0,0);
  egg_texture_upload(MENU->texidb,TEXW,TEXH,TEXW<<2,EGG_TEX_FMT_RGBA,0,0);
  return menu;
}
