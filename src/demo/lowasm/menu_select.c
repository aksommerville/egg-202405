/* menu_select.c
 * Menus where you pick a text option, which pushes a new menu.
 */

#include "lowasm_internal.h"

struct menu_select {
  struct menu hdr;
  char **optionv;
  int optionc,optiona;
  int optionp;
  void (*activate)(struct menu *menu);
  void (*motion)(struct menu *menu,int dx,int dy);
  int extra[4];
};

#define MENU ((struct menu_select*)menu)

/* Delete.
 */
 
static void _select_del(struct menu *menu) {
  if (MENU->optionv) {
    while (MENU->optionc-->0) free(MENU->optionv[MENU->optionc]);
    free(MENU->optionv);
  }
}

/* Update.
 */
 
static void _select_update(struct menu *menu,double elapsed) {
}

/* Event.
 */
 
static void _select_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_KEY: if (event->v[1]) switch (event->v[0]) {
      // 0x00070029 ESC is handled by the global layer.
      case 0x00070028: if (MENU->activate) MENU->activate(menu); break; // enter
      case 0x0007002c: if (MENU->activate) MENU->activate(menu); break; // space
      case 0x0007004f: if (MENU->motion) MENU->motion(menu,1,0); break; // arrows...
      case 0x00070050: if (MENU->motion) MENU->motion(menu,-1,0); break;
      case 0x00070051: if (MENU->motion) MENU->motion(menu,0,1); else { MENU->optionp++; if (MENU->optionp>=MENU->optionc) MENU->optionp=0; } break;
      case 0x00070052: if (MENU->motion) MENU->motion(menu,0,-1); else { MENU->optionp--; if (MENU->optionp<0) MENU->optionp=MENU->optionc-1; } break;
    }
  }
}

/* Render.
 */
 
static void _select_render(struct menu *menu) {
  if ((MENU->optionp>=0)&&(MENU->optionp<MENU->optionc)) {
    int y=4+MENU->optionp*8-1;
    egg_draw_rect(1,2,y,lowasm.screenw-4,9,0x000000ff);
  }
  lowasm_tiles_begin(lowasm.texid_font,0xffffffff,0xff);
  int y=8;
  int i=MENU->optionc;
  char **option=MENU->optionv;
  for (;i-->0;option++,y+=8) {
    lowasm_tiles_string(*option,-1,8,y);
  }
  lowasm_tiles_end();
}

/* Add option.
 */
 
static int _select_add_option(struct menu *menu,const char *src) {
  int srcc=0; if (src) while (src[srcc]) srcc++;
  if (MENU->optionc>=MENU->optiona) {
    int na=MENU->optiona+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(MENU->optionv,sizeof(void*)*na);
    if (!nv) return -1;
    MENU->optionv=nv;
    MENU->optiona=na;
  }
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  MENU->optionv[MENU->optionc++]=nv;
  return 0;
}

/* Main menu.
 */
 
static void _select_main_activate(struct menu *menu) {
  switch (MENU->optionp) {
    case 0: lowasm_menu_new_input(); break;
    case 1: lowasm_menu_new_audio(); break;
    case 2: lowasm_menu_new_video(); break;
    case 3: lowasm_menu_new_storage(); break;
    case 4: lowasm_menu_new_network(); break;
    case 5: lowasm_menu_new_misc(); break;
  }
}
 
struct menu *lowasm_menu_new_main() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_select));
  if (!menu) return 0;
  menu->del=_select_del;
  menu->update=_select_update;
  menu->event=_select_event;
  menu->render=_select_render;
  MENU->activate=_select_main_activate;
  _select_add_option(menu,"Input");
  _select_add_option(menu,"Audio");
  _select_add_option(menu,"Video");
  _select_add_option(menu,"Storage");
  _select_add_option(menu,"Network");
  _select_add_option(menu,"Misc");
  return menu;
}

/* Input.
 */
 
static void _select_input_activate(struct menu *menu) {
  switch (MENU->optionp) {
    case 0: lowasm_menu_new_joystick(); break;
    case 1: lowasm_menu_new_touch(); break;
    case 2: lowasm_menu_new_mouse(); break;
    case 3: lowasm_menu_new_accelerometer(); break;
    case 4: lowasm_menu_new_keyboard(); break;
  }
}
 
struct menu *lowasm_menu_new_input() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_select));
  if (!menu) return 0;
  menu->del=_select_del;
  menu->update=_select_update;
  menu->event=_select_event;
  menu->render=_select_render;
  MENU->activate=_select_input_activate;
  _select_add_option(menu,"Joystick");
  _select_add_option(menu,"Touch");
  _select_add_option(menu,"Mouse");
  _select_add_option(menu,"Accelerometer");
  _select_add_option(menu,"Keyboard");
  return menu;
}

/* Video.
 */
 
static void _select_video_activate(struct menu *menu) {
  switch (MENU->optionp) {
    case 0: lowasm_menu_new_xform_clipping(); break;
    case 1: lowasm_menu_new_too_many_sprites(); break;
    case 2: lowasm_menu_new_tint_alpha(); break;
    case 3: lowasm_menu_new_offscreen(); break;
  }
}
 
struct menu *lowasm_menu_new_video() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_select));
  if (!menu) return 0;
  menu->del=_select_del;
  menu->update=_select_update;
  menu->event=_select_event;
  menu->render=_select_render;
  MENU->activate=_select_video_activate;
  _select_add_option(menu,"Xform clipping test");
  _select_add_option(menu,"Too many sprites");
  _select_add_option(menu,"Global tint and alpha");
  _select_add_option(menu,"Offscreen render");
  return menu;
}

/* Storage.
 */
 
static void _select_storage_activate(struct menu *menu) {
  switch (MENU->optionp) {
    case 0: lowasm_menu_new_resources(); break;
    case 1: lowasm_menu_new_strings(); break;
    case 2: lowasm_menu_new_persistence(); break;
  }
}
 
struct menu *lowasm_menu_new_storage() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_select));
  if (!menu) return 0;
  menu->del=_select_del;
  menu->update=_select_update;
  menu->event=_select_event;
  menu->render=_select_render;
  MENU->activate=_select_storage_activate;
  _select_add_option(menu,"List resources");
  _select_add_option(menu,"Strings by language");
  _select_add_option(menu,"Persistence");
  return menu;
}

/* Network.
 */
 
static void _select_network_activate(struct menu *menu) {
  switch (MENU->optionp) {
    case 2: {
        int reqid=egg_http_request("GET","http://localhost:8080/index.html",0,0);
        egg_log("HTTP request id %d",reqid);
      } break;
    case 3: break;//TODO connect or disconnect websocket
  }
}

static void _select_network_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_HTTP_RSP: break;//TODO log request id, status, and length
    case EGG_EVENT_WS_CONNECT: break;//TODO update labels
    case EGG_EVENT_WS_DISCONNECT: break;//TODO update labels
  }
  _select_event(menu,event);
}
 
struct menu *lowasm_menu_new_network() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_select));
  if (!menu) return 0;
  menu->del=_select_del;
  menu->update=_select_update;
  menu->event=_select_network_event;
  menu->render=_select_render;
  MENU->activate=_select_network_activate;
  _select_add_option(menu,"Watch log for responses.");
  _select_add_option(menu,"");
  _select_add_option(menu,"HTTP request");
  _select_add_option(menu,"WebSocket (TODO)");
  return menu;
}

/* Misc.
 */
 
static void _select_misc_activate(struct menu *menu) {
  switch (MENU->optionp) {
    case 0: {
        egg_log("Test of log formatting. Next line should say: String Truncate Len c 123 0x7b 123.456");
        egg_log("%s %.8s %.*s %c %d 0x%x %f", "String", "Truncated String", 3, "Length First", 0x63, 123, 123, 123.456);
      } break;
    case 1: lowasm_menu_new_time(); break;
    case 2: lowasm_menu_new_languages(); break;
    case 4: egg_request_termination(); break;
    case 5: break;//TODO call js
  }
}
 
struct menu *lowasm_menu_new_misc() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_select));
  if (!menu) return 0;
  menu->del=_select_del;
  menu->update=_select_update;
  menu->event=_select_event;
  menu->render=_select_render;
  MENU->activate=_select_misc_activate;
  char termlbl[]="Is terminable? N";
  if (egg_is_terminable()) termlbl[15]='Y';
  _select_add_option(menu,"Log");
  _select_add_option(menu,"Time");
  _select_add_option(menu,"Languages");
  _select_add_option(menu,termlbl);
  _select_add_option(menu,"Terminate");
  _select_add_option(menu,"Call JS from Wasm");
  return menu;
}

/* Languages.
 * Nothing to actually select here, just abusing this type because our output is a small static list of strings.
 */
 
struct menu *lowasm_menu_new_languages() {
  int idv[64];
  int idc=egg_get_user_languages(idv,64);
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_select));
  if (!menu) return 0;
  menu->del=_select_del;
  menu->update=_select_update;
  menu->event=_select_event;
  menu->render=_select_render;
  if ((idc<1)||(idc>64)) {
    _select_add_option(menu,"Unexpected lang count");
  } else {
    int i=0; for (;i<idc;i++) {
      int code=idv[i];
      uint8_t a=code>>8,b=code;
      if (!(code&~0xffff)&&(a>='a')&&(a<='z')&&(b>='a')&&(b<='z')) {
        char tmp[4]={' ',a,b,0};
        _select_add_option(menu,tmp);
      } else {
        char tmp[11]="0x00000000";
        tmp[2]="0123456789abcdef"[(code>>28)&0xf];
        tmp[2]="0123456789abcdef"[(code>>24)&0xf];
        tmp[2]="0123456789abcdef"[(code>>20)&0xf];
        tmp[2]="0123456789abcdef"[(code>>16)&0xf];
        tmp[2]="0123456789abcdef"[(code>>12)&0xf];
        tmp[2]="0123456789abcdef"[(code>> 8)&0xf];
        tmp[2]="0123456789abcdef"[(code>> 4)&0xf];
        tmp[2]="0123456789abcdef"[(code>> 0)&0xf];
        _select_add_option(menu,tmp);
      }
    }
  }
  return menu;
}
