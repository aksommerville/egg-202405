#include "lowasm_internal.h"

struct menu_time {
  struct menu hdr;
  double starttime;
  double gametime;
  double minupd,maxupd;
};

#define MENU ((struct menu_time*)menu)

static void _time_update(struct menu *menu,double elapsed) {
  MENU->gametime+=elapsed;
  if (elapsed>MENU->maxupd) MENU->maxupd=elapsed;
  if (elapsed<MENU->minupd) MENU->minupd=elapsed;
}

static void _time_render(struct menu *menu) {
  double realtime=egg_time_real()-MENU->starttime;
  lowasm_tiles_begin(lowasm.texid_font,0xffffffff,0xff);
  lowasm_tiles_stringf(8,8,"Game: %f",MENU->gametime);
  lowasm_tiles_stringf(8,16,"Real: %f",realtime);
  lowasm_tiles_stringf(8,24,"Update: %f .. %f",MENU->minupd,MENU->maxupd);
  int year=0,month=0,day=0,hour=0,minute=0,second=0,milli=0;
  egg_time_get(&year,&month,&day,&hour,&minute,&second,&milli);
  lowasm_tiles_stringf(8,32,"Date: %d-%d-%d",year,month,day);
  lowasm_tiles_stringf(8,40,"Time: %d:%d:%d.%d",hour,minute,second,milli);
  lowasm_tiles_end();
}

struct menu *lowasm_menu_new_time() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_time));
  if (!menu) return 0;
  menu->update=_time_update;
  menu->render=_time_render;
  MENU->starttime=egg_time_real();
  MENU->minupd=999.0;
  MENU->maxupd=0.0;
  return menu;
}
