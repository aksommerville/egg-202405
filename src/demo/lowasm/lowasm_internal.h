#ifndef LOWASM_INTERNAL_H
#define LOWASM_INTERNAL_H

#include "egg/egg.h"
#include "opt/romr/romr.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define LOWASM_TILE_LIMIT 256
#define LOWASM_DEVID_LIMIT 32

struct menu {
  void (*del)(struct menu *menu);
  void (*update)(struct menu *menu,double elapsed);
  void (*event)(struct menu *menu,const struct egg_event *event);
  void (*render)(struct menu *menu); // Global draws the background and nothing else.
};

extern struct lowasm {
  int screenw,screenh;
  int texid_font,texid_misc;
  
  struct menu **menuv; // Last in the list is the only one active; and we terminate when the list goes empty.
  int menuc,menua;
  
  struct egg_draw_tile tilev[LOWASM_TILE_LIMIT];
  int tilec;
  int tiletexid;
  
  // We have to track all joysticks, because there's no reasonable way to poll for them after the CONNECT event.
  int devidv[LOWASM_DEVID_LIMIT];
  int devidc;
} lowasm;

void lowasm_tiles_begin(int texid,uint32_t tint,uint8_t alpha);
void lowasm_tiles_end();
void lowasm_tile(int x,int y,uint8_t tileid,uint8_t xform);
void lowasm_tiles_string(const char *src,int srcc,int x,int y);
void lowasm_tiles_stringf(int x,int y,const char *fmt,...); // No length or precision, only: %d %f %s %%

void lowasm_menu_pop(struct menu *menu); // whatever's on top, if (menu) null
struct menu *lowasm_menu_push(int objlen);
struct menu *lowasm_menu_new_main();
struct menu *lowasm_menu_new_input();
struct menu *lowasm_menu_new_audio();
struct menu *lowasm_menu_new_video();
struct menu *lowasm_menu_new_storage();
struct menu *lowasm_menu_new_network();
struct menu *lowasm_menu_new_misc();
struct menu *lowasm_menu_new_languages();
struct menu *lowasm_menu_new_joystick();
struct menu *lowasm_menu_new_touch();
struct menu *lowasm_menu_new_mouse();
static inline struct menu *lowasm_menu_new_accelerometer() { return 0; }//TODO Accelerometer menu. These events don't exist yet, as I'm writing this.
struct menu *lowasm_menu_new_keyboard();
struct menu *lowasm_menu_new_xform_clipping();
struct menu *lowasm_menu_new_too_many_sprites();
struct menu *lowasm_menu_new_tint_alpha();
struct menu *lowasm_menu_new_offscreen();
struct menu *lowasm_menu_new_resources();
struct menu *lowasm_menu_new_strings();
struct menu *lowasm_menu_new_persistence();
struct menu *lowasm_menu_new_time();

#endif
