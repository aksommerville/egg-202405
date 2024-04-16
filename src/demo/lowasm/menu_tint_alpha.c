#include "lowasm_internal.h"

struct menu_tintalpha {
  struct menu hdr;
  int decalx,tilex;
  int y0,rowh;
  struct egg_draw_tile *tilev; // static text labels only
  int tilec,tilea;
};

#define MENU ((struct menu_tintalpha*)menu)

static void _tintalpha_del(struct menu *menu) {
  if (MENU->tilev) free(MENU->tilev);
}

static void _tintalpha_render(struct menu *menu) {
  egg_draw_mode(0,0xffffffff,0xff);
  egg_draw_tile(1,lowasm.texid_font,MENU->tilev,MENU->tilec);
  egg_draw_mode(0,0,0xff);
  const int srcx=144,srcy=16,srcw=16,srch=16;
  struct egg_draw_tile tile={.x=MENU->tilex,.tileid=0x19,.xform=0};
  #define RENDERBOTH(row) { \
    egg_draw_decal(1,lowasm.texid_misc,MENU->decalx-8,MENU->y0+row*MENU->rowh-8,srcx,srcy,srcw,srch,0); \
    tile.y=MENU->y0+row*MENU->rowh; \
    egg_draw_tile(1,lowasm.texid_misc,&tile,1); \
  }
  egg_draw_mode(0,0x00000000,0xff); RENDERBOTH(0) // Natural
  egg_draw_mode(0,0xff0000ff,0xff); RENDERBOTH(1) // Red
  egg_draw_mode(0,0x00ff00ff,0xff); RENDERBOTH(2) // Green
  egg_draw_mode(0,0x0000ffff,0xff); RENDERBOTH(3) // Blue
  egg_draw_mode(0,0xff000080,0xff); RENDERBOTH(4) // Half Red
  egg_draw_mode(0,0x00000000,0x80); RENDERBOTH(5) // 50%
  egg_draw_mode(0,0xff0000ff,0x80); RENDERBOTH(6) // 50%+Red
  #undef RENDERBOTH
  egg_draw_mode(0,0,0xff);
}

static int tintalpha_add_tile(struct menu *menu,int x,int y,uint8_t tileid) {
  if (MENU->tilec>=MENU->tilea) {
    int na=MENU->tilea+64;
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

static void tintalpha_add_label(struct menu *menu,int x,int y,const char *src) {
  for (;*src;src++,x+=8) tintalpha_add_tile(menu,x,y,*src);
}

struct menu *lowasm_menu_new_tint_alpha() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_tintalpha));
  if (!menu) return 0;
  menu->del=_tintalpha_del;
  menu->render=_tintalpha_render;
  
  int colw=lowasm.screenw/3;
  MENU->decalx=colw>>1;
  MENU->tilex=MENU->decalx+colw;
  int lblx=(colw<<1);
  int rowc=8; // 7 test images and 1 header row
  MENU->rowh=20; // 160 total height, and framebuffer is 180, nice fit.
  int hdry=(lowasm.screenh>>1)-((rowc*MENU->rowh)>>1)+(MENU->rowh>>1);
  MENU->y0=hdry+MENU->rowh;
  tintalpha_add_label(menu,MENU->decalx-20,hdry,"Decal");
  tintalpha_add_label(menu,MENU->tilex-20,hdry,"Tile");
  tintalpha_add_label(menu,lblx,MENU->y0+MENU->rowh*0,"Natural");
  tintalpha_add_label(menu,lblx,MENU->y0+MENU->rowh*1,"Red");
  tintalpha_add_label(menu,lblx,MENU->y0+MENU->rowh*2,"Green");
  tintalpha_add_label(menu,lblx,MENU->y0+MENU->rowh*3,"Blue");
  tintalpha_add_label(menu,lblx,MENU->y0+MENU->rowh*4,"Half Red");
  tintalpha_add_label(menu,lblx,MENU->y0+MENU->rowh*5,"50%");
  tintalpha_add_label(menu,lblx,MENU->y0+MENU->rowh*6,"50%+Red");
  
  return menu;
}
