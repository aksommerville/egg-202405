/* lowasm: main.c
 * Lights-On test, WebAssembly edition.
 * Not a great example of how to write Egg games -- better examples will be provided elsewhere.
 * Trying to at least superficially imitate "lojs", which I wrote first.
 * But not exactly.
 */
 
#include "lowasm_internal.h"

struct lowasm lowasm={0};

/* Quit.
 */
 
void egg_client_quit() {
}

/* Init.
 */
 
int egg_client_init() {
  //return -1; // Uncomment to test fatal errors.
  
  int texfmt=0;
  egg_texture_get_header(&lowasm.screenw,&lowasm.screenh,&texfmt,1);
  if ((lowasm.screenw<1)||(lowasm.screenh<1)) {
    egg_log("Invalid size %d,%d for texture 1.",lowasm.screenw,lowasm.screenh);
    return -1;
  }
  
  lowasm.texid_font=egg_texture_new();
  lowasm.texid_misc=egg_texture_new();
  if (egg_texture_load_image(lowasm.texid_font,0,2)<0) return -1;
  if (egg_texture_load_image(lowasm.texid_misc,0,3)<0) return -1;
  
  // Enable every event.
  #define _(tag) { \
    int state=egg_event_enable(EGG_EVENT_##tag,EGG_EVTSTATE_ENABLED); \
    if ((state==EGG_EVTSTATE_ENABLED)||(state==EGG_EVTSTATE_REQUIRED)) egg_log("Event %s enabled.",#tag); \
    else egg_log("Event **%s** failed to enable.",#tag); \
  }
  _(INPUT)
  _(CONNECT)
  _(DISCONNECT)
  _(HTTP_RSP)
  _(WS_CONNECT)
  _(WS_DISCONNECT)
  _(WS_MESSAGE)
  _(MMOTION)
  _(MBUTTON)
  _(MWHEEL)
  _(KEY)
  _(TEXT)
  _(TOUCH)
  #undef _
  
  if (!lowasm_menu_new_main()) return -1;
  
  return 0;
}

/* Update.
 */
 
static void egg_client_event(const struct egg_event *event) {
  switch (event->type) {
  
    case EGG_EVENT_KEY: if (event->v[1]) switch (event->v[0]) {
        case 0x00070029: lowasm_menu_pop(0); break; // Esc
      } break;
      
    case EGG_EVENT_CONNECT: {
        if (lowasm.devidc<LOWASM_DEVID_LIMIT) {
          lowasm.devidv[lowasm.devidc++]=event->v[0];
        }
      } break;
    
    case EGG_EVENT_DISCONNECT: {
        int i=lowasm.devidc;
        while (i-->0) {
          if (lowasm.devidv[i]==event->v[0]) {
            lowasm.devidc--;
            memmove(lowasm.devidv+i,lowasm.devidv+i+1,sizeof(int)*(lowasm.devidc-i));
            break;
          }
        }
      } break;
    
  }
  if (lowasm.menuc>0) {
    struct menu *menu=lowasm.menuv[lowasm.menuc-1];
    if (menu->event) menu->event(menu,event);
  }
}
 
void egg_client_update(double elapsed) {
  struct egg_event eventv[16];
  int eventc;
  while ((eventc=egg_event_next(eventv,16))>0) {
    const struct egg_event *event=eventv;
    int i=eventc;
    for (;i-->0;event++) egg_client_event(event);
    if (eventc<16) break;
  }
  if (lowasm.menuc>0) {
    struct menu *menu=lowasm.menuv[lowasm.menuc-1];
    if (menu->update) menu->update(menu,elapsed);
  }
}

/* Render.
 */
 
void egg_client_render() {
  egg_draw_rect(1,0,0,lowasm.screenw,lowasm.screenh,0x5060c0ff);
  if (lowasm.menuc>0) {
    struct menu *menu=lowasm.menuv[lowasm.menuc-1];
    if (menu->render) menu->render(menu);
  }
}

/* Remove menu.
 */
 
void lowasm_menu_pop(struct menu *menu) {
  if (menu) {
    int i=lowasm.menuc;
    while (i-->0) {
      if (lowasm.menuv[i]==menu) {
        lowasm.menuc--;
        memmove(lowasm.menuv+i,lowasm.menuv+i+1,sizeof(void*)*(lowasm.menuc-i));
        if (menu->del) menu->del(menu);
        free(menu);
        break;
      }
    }
  } else if (lowasm.menuc>0) {
    lowasm.menuc--;
    struct menu *menu=lowasm.menuv[lowasm.menuc];
    if (menu->del) menu->del(menu);
    free(menu);
  }
  if (!lowasm.menuc) {
    egg_request_termination();
  }
}

/* Add menu to stack.
 */
 
struct menu *lowasm_menu_push(int objlen) {
  if (objlen<(int)sizeof(struct menu)) return 0;
  if (lowasm.menuc>=lowasm.menua) {
    int na=lowasm.menua+8;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(lowasm.menuv,sizeof(void*)*na);
    if (!nv) return 0;
    lowasm.menuv=nv;
    lowasm.menua=na;
  }
  struct menu *menu=calloc(1,objlen);
  if (!menu) return 0;
  lowasm.menuv[lowasm.menuc++]=menu;
  return menu;
}

/* Tiles render helper.
 */
 
static void lowasm_tiles_flush() {
  egg_draw_tile(1,lowasm.tiletexid,lowasm.tilev,lowasm.tilec);
  lowasm.tilec=0;
}
 
void lowasm_tiles_begin(int texid,uint32_t tint,uint8_t alpha) {
  if (lowasm.tilec) lowasm_tiles_flush();
  lowasm.tiletexid=texid;
  egg_draw_mode(0,tint,alpha);
}

void lowasm_tiles_end() {
  if (lowasm.tilec) lowasm_tiles_flush();
  egg_draw_mode(0,0,0xff);
}

void lowasm_tile(int x,int y,uint8_t tileid,uint8_t xform) {
  if (lowasm.tilec>=LOWASM_TILE_LIMIT) lowasm_tiles_flush();
  struct egg_draw_tile *vtx=lowasm.tilev+lowasm.tilec++;
  vtx->x=x;
  vtx->y=y;
  vtx->tileid=tileid;
  vtx->xform=xform;
}

void lowasm_tiles_string(const char *src,int srcc,int x,int y) {
  if (!src) return;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  for (;srcc-->0;src++,x+=8) lowasm_tile(x,y,*src,0);
}

void lowasm_tiles_stringf(int x,int y,const char *fmt,...) {
  if (!fmt) return;
  va_list vargs;
  va_start(vargs,fmt);
  #define OUT(ch) { lowasm_tile(x,y,ch,0); x+=8; }
  while (*fmt) {
    if (fmt[0]=='%') switch (fmt[1]) {
      case 'd': {
          fmt+=2;
          int v=va_arg(vargs,int);
          if (v<0) { OUT('-') v=-v; if (v<0) v=INT_MAX; }
          if (v>=1000000000) OUT('0'+(v/1000000000)%10)
          if (v>= 100000000) OUT('0'+(v/ 100000000)%10)
          if (v>=  10000000) OUT('0'+(v/  10000000)%10)
          if (v>=   1000000) OUT('0'+(v/   1000000)%10)
          if (v>=    100000) OUT('0'+(v/    100000)%10)
          if (v>=     10000) OUT('0'+(v/     10000)%10)
          if (v>=      1000) OUT('0'+(v/      1000)%10)
          if (v>=       100) OUT('0'+(v/       100)%10)
          if (v>=        10) OUT('0'+(v/        10)%10)
          OUT('0'+v%10)
        } continue;
      case 'f': {
          fmt+=2;
          double v=va_arg(vargs,double);
          if (v<0.0) { OUT('-') v=-v; }
          int i=(int)(v*1000.0);
          if (i>=1000000000) OUT('0'+(i/1000000000)%10)
          if (i>= 100000000) OUT('0'+(i/ 100000000)%10)
          if (i>=  10000000) OUT('0'+(i/  10000000)%10)
          if (i>=   1000000) OUT('0'+(i/   1000000)%10)
          if (i>=    100000) OUT('0'+(i/    100000)%10)
          if (i>=     10000) OUT('0'+(i/     10000)%10)
          OUT('0'+(i/1000)%10)
          OUT('.')
          OUT('0'+(i/100)%10)
          OUT('0'+(i/10)%10)
          OUT('0'+i%10)
        } continue;
      case 's': {
          fmt+=2;
          const char *v=va_arg(vargs,const char *);
          if (v) {
            for (;*v;v++) OUT(*v)
          }
        } continue;
      case '%': {
          fmt+=2;
          OUT('%')
        } continue;
    }
    OUT(*fmt)
    fmt++;
  }
  #undef OUT
}
