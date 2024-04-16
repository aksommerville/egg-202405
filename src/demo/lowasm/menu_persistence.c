#include "lowasm_internal.h"

struct menu_persistence {
  struct menu hdr;
  int optionp;
  struct egg_draw_tile *tilev;
  int tilec,tilea;
  int vegetablep,fruitp,cityp;
};

#define MENU ((struct menu_persistence*)menu)

static void _persistence_del(struct menu *menu) {
  if (MENU->tilev) free(MENU->tilev);
}

static int persistence_add_tile(struct menu *menu,int x,int y,uint8_t tileid) {
  if (MENU->tilec>=MENU->tilea) {
    int na=MENU->tilea+128;
    if (na>INT_MAX/sizeof(struct egg_draw_tile)) return -1;
    void *nv=realloc(MENU->tilev,sizeof(struct egg_draw_tile)*na);
    if (!nv) return -1;
    MENU->tilev=nv;
    MENU->tilea=na;
  }
  struct egg_draw_tile *tile=MENU->tilev+MENU->tilec++;
  tile->x=x;
  tile->y=y;
  tile->tileid=tileid;
  tile->xform=0;
  return 0;
}

static void persistence_add_row(struct menu *menu,int y,const char *k,const char *v) {
  int x=8;
  for (;*k;k++,x+=8) persistence_add_tile(menu,x,y,*k);
  persistence_add_tile(menu,x,y,':');
  x+=16;
  if (v) {
    for (;*v;v++,x+=8) persistence_add_tile(menu,x,y,*v);
  }
}

static void persistence_rebuild_tiles(struct menu *menu,const char *vegetable,const char *fruit,const char *city) {
  MENU->tilec=0;
  persistence_add_row(menu,10,"Vegetable",vegetable);
  persistence_add_row(menu,18,"Fruit",fruit);
  persistence_add_row(menu,26,"City",city);
}

static void persistence_adjust(struct menu *menu,int d) {
  switch (MENU->optionp) {
    #define _(p,var) case p: MENU->var+=d; if (MENU->var>=3) MENU->var=0; else if (MENU->var<0) MENU->var=2; break;
    _(0,vegetablep)
    _(1,fruitp)
    _(2,cityp)
    #undef _
    default: return;
  }
  const char *vegetable="",*fruit="",*city="";
  switch (MENU->vegetablep) {
    case 0: vegetable="Asparagus"; break;
    case 1: vegetable="Beet"; break;
    case 2: vegetable="Carrot"; break;
  }
  switch (MENU->fruitp) {
    case 0: fruit="Apple"; break;
    case 1: fruit="Banana"; break;
    case 2: fruit="Coconut"; break;
  }
  switch (MENU->cityp) {
    case 0: city="Albuquerque"; break;
    case 1: city="Boston"; break;
    case 2: city="Columbus"; break;
  }
  switch (MENU->optionp) {
    case 0: egg_store_set("vegetable",9,vegetable,strlen(vegetable)); break;
    case 1: egg_store_set("fruit",5,fruit,strlen(fruit)); break;
    case 2: egg_store_set("city",4,city,strlen(city)); break;
  }
  persistence_rebuild_tiles(menu,vegetable,fruit,city);
}

static void _persistence_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_KEY: if (event->v[1]) switch (event->v[0]) {
        case 0x0007004f: persistence_adjust(menu,1); break;
        case 0x00070050: persistence_adjust(menu,-1); break;
        case 0x00070051: MENU->optionp++; if (MENU->optionp>=3) MENU->optionp=0; break;
        case 0x00070052: MENU->optionp--; if (MENU->optionp<0) MENU->optionp=2; break;
      } break;
  }
}

static void _persistence_render(struct menu *menu) {
  egg_draw_rect(1,2,5+MENU->optionp*8,lowasm.screenw-4,9,0x000000ff);
  egg_draw_mode(0,0xffffffff,0xff);
  egg_draw_tile(1,lowasm.texid_font,MENU->tilev,MENU->tilec);
  egg_draw_mode(0,0,0xff);
}

struct menu *lowasm_menu_new_persistence() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_persistence));
  if (!menu) return 0;
  menu->del=_persistence_del;
  menu->event=_persistence_event;
  menu->render=_persistence_render;
  
  char vegetable[16],fruit[16],city[16];
  #define _(tag) { \
    int vc=egg_store_get(tag,sizeof(tag),#tag,sizeof(#tag)-1); \
    if ((vc<0)||(vc>=sizeof(tag))) vc=0; \
    tag[vc]=0; \
  }
  _(vegetable)
  _(fruit)
  _(city)
  #undef _
  persistence_rebuild_tiles(menu,vegetable,fruit,city);
       if (!strcmp(vegetable,"Asparagus")) MENU->vegetablep=0;
  else if (!strcmp(vegetable,"Beet")) MENU->vegetablep=1;
  else if (!strcmp(vegetable,"Carrot")) MENU->vegetablep=2;
       if (!strcmp(fruit,"Apple")) MENU->fruitp=0;
  else if (!strcmp(fruit,"Banana")) MENU->fruitp=1;
  else if (!strcmp(fruit,"Coconut")) MENU->fruitp=2;
       if (!strcmp(city,"Albuquerque")) MENU->cityp=0;
  else if (!strcmp(city,"Boston")) MENU->cityp=1;
  else if (!strcmp(city,"Columbus")) MENU->cityp=2;
  
  return menu;
}
