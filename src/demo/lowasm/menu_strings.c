#include "lowasm_internal.h"

struct menu_strings {
  struct menu hdr;
  int *langv;
  int langp,langc,langa;
  struct egg_draw_tile *tilev;
  int tilec,tilea;
};

#define MENU ((struct menu_strings*)menu)

static void _strings_del(struct menu *menu) {
  if (MENU->langv) free(MENU->langv);
  if (MENU->tilev) free(MENU->tilev);
}

static void strings_move(struct menu *menu,int d) {
  if (MENU->langc<1) return;
  MENU->langp+=d;
  if (MENU->langp<0) MENU->langp=MENU->langc-1;
  else if (MENU->langp>=MENU->langc) MENU->langp=0;
  int lang=MENU->langv[MENU->langp];
  MENU->tilec=0;
  int p=0,y=14;
  for (;;p++) {
    int tid=0,qual=0,rid=0;
    egg_res_id_by_index(&tid,&qual,&rid,p);
    if (!tid) break;
    if (tid!=EGG_TID_string) continue;
    if (qual!=lang) continue;
    uint8_t serial[256];
    int serialc=egg_res_get(serial,sizeof(serial),EGG_TID_string,qual,rid);
    if (!serialc) continue;
    if ((serialc<0)||(serialc>sizeof(serial))) {
      egg_log("!!! Unexpected size %d for string:%d:%d !!!",serialc,qual,rid);
      continue;
    }
    int na=MENU->tilec+serialc;
    if (na>MENU->tilea) {
      na=(na+256)&~255;
      if (na>INT_MAX/sizeof(struct egg_draw_tile)) return;
      void *nv=realloc(MENU->tilev,sizeof(struct egg_draw_tile)*na);
      if (!nv) return;
      MENU->tilev=nv;
      MENU->tilea=na;
    }
    int x=8;
    int i=serialc;
    const uint8_t *src=serial;
    for (;i-->0;src++,x+=8) { // Strings must be in 8859 or ASCII
      if (*src<=0x20) continue;
      struct egg_draw_tile *tile=MENU->tilev+MENU->tilec++;
      tile->x=x;
      tile->y=y;
      tile->tileid=*src;
      tile->xform=0;
    }
    y+=8;
  }
}

static void _strings_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_KEY: if (event->v[1]) switch (event->v[0]) {
        case 0x0007004f: strings_move(menu,1); break;
        case 0x00070050: strings_move(menu,-1); break;
      } break;
  }
}

static void _strings_render(struct menu *menu) {
  lowasm_tiles_begin(lowasm.texid_font,0xff8080ff,0xff);
  if ((MENU->langp>=0)&&(MENU->langp<MENU->langc)) {
    lowasm_tile(4,4,MENU->langv[MENU->langp]>>8,0);
    lowasm_tile(12,4,MENU->langv[MENU->langp],0);
  } else {
    lowasm_tile(4,4,'!',0);
    lowasm_tile(12,4,'!',0);
  }
  lowasm_tiles_end();
  lowasm_tiles_begin(lowasm.texid_font,0xffff00ff,0xff);
  lowasm_tiles_string("Left/Right to change lang",-1,28,4);
  lowasm_tiles_end();
  egg_draw_tile(1,lowasm.texid_font,MENU->tilev,MENU->tilec);
}

static void strings_discover_languages(struct menu *menu) {
  int p=0;
  for (;;p++) {
    int tid=0,qual=0,rid=0;
    egg_res_id_by_index(&tid,&qual,&rid,p);
    if (!tid) break;
    if (tid!=EGG_TID_string) continue;
    int already=0,i=MENU->langc;
    while (i-->0) {
      if (MENU->langv[i]==qual) {
        already=1;
        break;
      }
    }
    if (already) continue;
    if (MENU->langc>=MENU->langa) {
      int na=MENU->langa+8;
      if (na>INT_MAX/sizeof(int)) return;
      void *nv=realloc(MENU->langv,sizeof(int)*na);
      if (!nv) return;
      MENU->langv=nv;
      MENU->langa=na;
    }
    MENU->langv[MENU->langc++]=qual;
  }
}

struct menu *lowasm_menu_new_strings() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_strings));
  if (!menu) return 0;
  menu->del=_strings_del;
  menu->event=_strings_event;
  menu->render=_strings_render;
  strings_discover_languages(menu);
  strings_move(menu,0);
  return menu;
}
