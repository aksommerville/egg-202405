#include "lowasm_internal.h"

#define LINES_PER_PAGE 21 /* (180-8)/8 */
#define ROW_LENGTH 40

struct menu_resources {
  struct menu hdr;
  int p; // -1 initially; index of first resource, steps by LINES_PER_PAGE
  char console[ROW_LENGTH*LINES_PER_PAGE];
};

#define MENU ((struct menu_resources*)menu)

static void resources_fetch_and_repr(char *dst,int p) {
  int tid=0,qual=0,rid=0;
  egg_res_id_by_index(&tid,&qual,&rid,p);
  if (!tid&&!qual&&!rid) return;
  if ((tid<=0)||(qual<0)||(rid<=0)) {
    memcpy(dst,"INVALID! 00000000 00000000 00000000",35);
    #define REPRHEX(dstp,v) { \
      dst[dstp+0]="0123456789abcdef"[(v>>28)&0xf]; \
      dst[dstp+1]="0123456789abcdef"[(v>>24)&0xf]; \
      dst[dstp+2]="0123456789abcdef"[(v>>20)&0xf]; \
      dst[dstp+3]="0123456789abcdef"[(v>>16)&0xf]; \
      dst[dstp+4]="0123456789abcdef"[(v>>12)&0xf]; \
      dst[dstp+5]="0123456789abcdef"[(v>> 8)&0xf]; \
      dst[dstp+6]="0123456789abcdef"[(v>> 4)&0xf]; \
      dst[dstp+7]="0123456789abcdef"[(v>> 0)&0xf]; \
    }
    REPRHEX(9,tid)
    REPRHEX(18,qual)
    REPRHEX(27,rid)
    #undef REPRHEX
  } else {
  
    int dstc=0;
    #define REPRDEC(v) { \
      if (v>=1000000000) dst[dstc++]='0'+(v/1000000000)%10; \
      if (v>= 100000000) dst[dstc++]='0'+(v/ 100000000)%10; \
      if (v>=  10000000) dst[dstc++]='0'+(v/  10000000)%10; \
      if (v>=   1000000) dst[dstc++]='0'+(v/   1000000)%10; \
      if (v>=    100000) dst[dstc++]='0'+(v/    100000)%10; \
      if (v>=     10000) dst[dstc++]='0'+(v/     10000)%10; \
      if (v>=      1000) dst[dstc++]='0'+(v/      1000)%10; \
      if (v>=       100) dst[dstc++]='0'+(v/       100)%10; \
      if (v>=        10) dst[dstc++]='0'+(v/        10)%10; \
                         dst[dstc++]='0'+(v           )%10; \
    }
    
    const char *tidsrc=0;
    switch (tid) {
      #define _(tag) case EGG_TID_##tag: tidsrc=#tag; break;
      EGG_TID_FOR_EACH
      #undef _
    }
    if (tidsrc) {
      for (;*tidsrc;tidsrc++) dst[dstc++]=*tidsrc;
    } else {
      REPRDEC(tid)
    }
    dst[dstc++]=':';
    
    uint8_t qhi=qual>>8,qlo=qual;
    if (
      ((tid==EGG_TID_string)||(tid==EGG_TID_image)||(tid==EGG_TID_sound))&&
      (qhi>='a')&&(qhi<='z')&&(qlo>='a')&&(qlo<='z')
    ) {
      dst[dstc++]=qhi;
      dst[dstc++]=qlo;
    } else {
      REPRDEC(qual)
    }
    dst[dstc++]=':';
    
    REPRDEC(rid)
    dst[dstc++]=':';
    dst[dstc++]=' ';
    
    int serialc=egg_res_get(0,0,tid,qual,rid);
    if (serialc>=1<<20) {
      int mb=serialc>>20;
      REPRDEC(mb)
      dst[dstc++]='M';
    } else if (serialc>=1<<10) {
      int kb=serialc>>10;
      REPRDEC(kb)
      dst[dstc++]='k';
    } else {
      REPRDEC(serialc)
    }
  
    #undef REPRDEC
    if (dstc>ROW_LENGTH) {
      egg_log("!!! Row for resource %d:%d:%d (len %d) exceeds limit (%d>%d) !!!",tid,qual,rid,serialc,dstc,ROW_LENGTH);
    }
  }
}

static void resources_move(struct menu *menu,int dx) {
  if (MENU->p<0) {
    if (dx>0) MENU->p=0;
  } else {
    MENU->p+=dx*LINES_PER_PAGE;
  }
  memset(MENU->console,0,sizeof(MENU->console));
  char *dst=MENU->console;
  int resp=MENU->p;
  int i=LINES_PER_PAGE;
  for (;i-->0;resp++,dst+=ROW_LENGTH) {
    resources_fetch_and_repr(dst,resp);
  }
}

static void _resources_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_KEY: if (event->v[1]) switch (event->v[0]) {
        case 0x0007004f: resources_move(menu,1); break;
        case 0x00070050: resources_move(menu,-1); break;
      } break;
  }
}

static void _resources_render(struct menu *menu) {
  lowasm_tiles_begin(lowasm.texid_font,0xffff00ff,0xff);
  lowasm_tiles_string("Left/Right to page thru",-1,4,4);
  lowasm_tiles_end();
  lowasm_tiles_begin(lowasm.texid_font,0xffffffff,0xff);
  const int x0=4,y0=14;
  int x=x0,y=y0,i=sizeof(MENU->console);
  const char *v=MENU->console;
  for (;i-->0;v++) {
    if ((unsigned char)(*v)>0x20) {
      lowasm_tile(x,y,*v,0);
    }
    x+=8;
    if (x>=lowasm.screenw) {
      x=x0;
      y+=8;
    }
  }
  lowasm_tiles_end();
}

struct menu *lowasm_menu_new_resources() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_resources));
  if (!menu) return 0;
  menu->event=_resources_event;
  menu->render=_resources_render;
  MENU->p=-1;
  return menu;
}
