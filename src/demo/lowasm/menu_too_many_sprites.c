#include "lowasm_internal.h"

#define ANIM_FRAME_TIME 0.250
#define SPRITE_LIMIT 16384
#define TOP_SPEED 200 /* px/s, but it's actually half of this */

struct menu_tms {
  struct menu hdr;
  struct sprite {
    double x,y;
    double dx,dy;
    uint8_t tileidv[4];
    int tileidp,tileidc;
    uint8_t xform;
  } *spritev;
  int spritec,spritea;
  struct egg_draw_tile *tilev;
  int tilea;
  double animclock;
};

#define MENU ((struct menu_tms*)menu)

static void _tms_del(struct menu *menu) {
  if (MENU->spritev) free(MENU->spritev);
  if (MENU->tilev) free(MENU->tilev);
}

// Resizes both (spritev) and (tilev).
static void tms_set_sprite_count(struct menu *menu,int nc) {
  if ((nc<1)||(nc>SPRITE_LIMIT)) return;
  if (nc<MENU->spritec) {
    MENU->spritec=nc;
    return;
  }
  if (nc>MENU->spritea) {
    void *nv=realloc(MENU->spritev,sizeof(struct sprite)*nc);
    if (!nv) return;
    MENU->spritev=nv;
    MENU->spritea=nc;
  }
  if (nc>MENU->tilea) {
    void *nv=realloc(MENU->tilev,sizeof(struct egg_draw_tile)*nc);
    if (!nv) return;
    MENU->tilev=nv;
    MENU->tilea=nc;
  }
  while (MENU->spritec<nc) {
    struct sprite *sprite=MENU->spritev+MENU->spritec++;
    sprite->x=rand()%lowasm.screenw;
    sprite->y=rand()%lowasm.screenh;
    sprite->dx=(rand()%TOP_SPEED)-(TOP_SPEED>>1);
    sprite->dy=(rand()%TOP_SPEED)-(TOP_SPEED>>1);
    switch (rand()&7) {
      case 0: sprite->tileidv[0]=0x14; sprite->tileidv[1]=0x15; sprite->tileidc=2; break;
      case 1: sprite->tileidv[0]=0x16; sprite->tileidv[1]=0x17; sprite->tileidv[2]=0x16; sprite->tileidv[3]=0x18; sprite->tileidc=4; break;
      case 2: sprite->tileidv[0]=0x1a; sprite->tileidv[1]=0x1a; sprite->tileidv[2]=0x19; sprite->tileidc=3; break;
      case 3: sprite->tileidv[0]=0x1b; sprite->tileidc=1; break;
      case 4: sprite->tileidv[0]=0x1c; sprite->tileidc=1; break;
      case 5: sprite->tileidv[0]=0x1d; sprite->tileidc=1; break;
      case 6: sprite->tileidv[0]=0x1e; sprite->tileidc=1; break;
      default: sprite->tileidv[0]=0x1f; sprite->tileidc=1; break;
    }
    sprite->tileidp=0;
    sprite->xform=rand()&7;
  }
}

static void _tms_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_KEY: if (event->v[1]) switch (event->v[0]) {
        case 0x0007004f: tms_set_sprite_count(menu,MENU->spritec<<1); break;
        case 0x00070050: tms_set_sprite_count(menu,MENU->spritec>>1); break;
      } break;
  }
}

static void _tms_update(struct menu *menu,double elapsed) {
  struct sprite *sprite=MENU->spritev;
  int i=MENU->spritec;
  for (;i-->0;sprite++) {
    sprite->x+=sprite->dx*elapsed;
    if (
      ((sprite->x<0.0)&&(sprite->dx<0.0))||
      ((sprite->x>lowasm.screenw)&&(sprite->dx>0.0))
    ) sprite->dx=-sprite->dx;
    sprite->y+=sprite->dy*elapsed;
    if (
      ((sprite->y<0.0)&&(sprite->dy<0.0))||
      ((sprite->y>lowasm.screenh)&&(sprite->dy>0.0))
    ) sprite->dy=-sprite->dy;
  }
  if ((MENU->animclock-=elapsed)<=0.0) {
    MENU->animclock=ANIM_FRAME_TIME;
    for (sprite=MENU->spritev,i=MENU->spritec;i-->0;sprite++) {
      sprite->tileidp++;
      if (sprite->tileidp>=sprite->tileidc) sprite->tileidp=0;
    }
  }
}

static void _tms_render(struct menu *menu) {
  const struct sprite *sprite=MENU->spritev;
  struct egg_draw_tile *tile=MENU->tilev;
  int i=MENU->spritec;
  for (;i-->0;sprite++,tile++) {
    tile->x=(int)sprite->x;
    tile->y=(int)sprite->y;
    tile->tileid=sprite->tileidv[sprite->tileidp];
    tile->xform=sprite->xform;
  }
  egg_draw_tile(1,lowasm.texid_misc,MENU->tilev,MENU->spritec);
  lowasm_tiles_begin(lowasm.texid_font,0xffffffff,0xc0);
  lowasm_tiles_stringf(6,lowasm.screenh-6,"%d",MENU->spritec);
  lowasm_tiles_end();
}

struct menu *lowasm_menu_new_too_many_sprites() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_tms));
  if (!menu) return 0;
  menu->del=_tms_del;
  menu->event=_tms_event;
  menu->update=_tms_update;
  menu->render=_tms_render;
  tms_set_sprite_count(menu,32);
  return menu;
}
