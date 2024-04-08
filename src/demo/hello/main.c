#include "egg/egg.h"
#include <stdlib.h>

#if 1

static int screenw=0,screenh=0;

int texid_dot=0;
int texid_tilesheet=0;
int texid_title=0;
int texid_rotate=0;

#define SPRITEA 500
static struct sprite {
  int16_t x,y;
  int16_t dx,dy;
  uint8_t tileid;
  uint8_t xform;
} spritev[SPRITEA];
static int spritec=0;

void egg_client_quit() {
  egg_log("egg_client_quit");
}

int egg_client_init() {
  egg_log("egg_client_init");
  egg_log("This message has %d variadic arguments (%s:%d:%s).",4,__FILE__,__LINE__,__func__);
  
  egg_texture_get_header(&screenw,&screenh,0,1);
  
  if (!(texid_dot=egg_texture_new())) return -1;
  if (!(texid_tilesheet=egg_texture_new())) return -1;
  if (!(texid_title=egg_texture_new())) return -1;
  if (!(texid_rotate=egg_texture_new())) return -1;
  if (egg_texture_load_image(texid_dot,0,1)<0) {
    egg_log("failed to load image:1");
    return -1;
  }
  if (egg_texture_load_image(texid_tilesheet,0,2)<0) {
    egg_log("failed to load image:2");
    return -1;
  }
  if (egg_texture_load_image(texid_title,0,3)<0) {
    egg_log("failed to load image:3");
    return -1;
  }
  if (egg_texture_load_image(texid_rotate,0,4)<0) {
    egg_log("failed to load image:4");
    return -1;
  }
  
  {
    const int max_speed=3;
    const int speeds_count=(max_speed<<1)+1;
    struct sprite *sprite=spritev;
    int i=SPRITEA;
    for (;i-->0;sprite++) {
      sprite->x=rand()%screenw;
      sprite->y=rand()%screenh;
      switch (rand()%3) {
        case 0: sprite->tileid=0x00; break;
        case 1: sprite->tileid=0x01; break;
        case 2: sprite->tileid=0xff; break;
      }
      sprite->xform=rand()&7;
      do {
        sprite->dx=rand()%speeds_count-max_speed;
        sprite->dy=rand()%speeds_count-max_speed;
      } while (!sprite->dx||!sprite->dy);
    }
    spritec=SPRITEA;
  }
  
  egg_audio_play_song(0,5,0,1);
  
  egg_log("size of sound:0:35=%d",egg_res_get(0,0,7,0,35));
  
  egg_log("%s:%d",__FILE__,__LINE__);
  return 0;
}

/**/
static void on_key(int keycode,int value) {
  if (value) switch (keycode) {
    
    case 0x0007001e: egg_audio_play_sound(0,35,1.0,0.0); break; // 1
    case 0x0007001f: egg_audio_play_sound(0,36,1.0,0.0); break;
    case 0x00070020: egg_audio_play_sound(0,37,1.0,0.0); break;
    case 0x00070021: egg_audio_play_sound(0,38,1.0,0.0); break;
    case 0x00070022: egg_audio_play_sound(0,39,1.0,0.0); break;
    case 0x00070023: egg_audio_play_sound(0,40,1.0,0.0); break;
    case 0x00070024: egg_audio_play_sound(0,41,1.0,0.0); break;
    case 0x00070025: egg_audio_play_sound(0,42,1.0,0.0); break;
    case 0x00070026: egg_audio_play_sound(0,43,1.0,0.0); break;
    case 0x00070027: egg_audio_play_sound(0,44,1.0,0.0); break; // 0
    case 0x00070014: egg_audio_play_sound(0,45,1.0,0.0); break; // q
    case 0x0007001a: egg_audio_play_sound(0,46,1.0,0.0); break; // w
    case 0x00070008: egg_audio_play_sound(0,47,1.0,0.0); break; // e
    case 0x00070015: egg_audio_play_sound(0,48,1.0,0.0); break; // r
    case 0x00070017: egg_audio_play_sound(0,49,1.0,0.0); break; // t
    case 0x0007001c: egg_audio_play_sound(0,50,1.0,0.0); break; // y
    case 0x00070018: egg_audio_play_sound(0,51,1.0,0.0); break; // u
    case 0x0007000c: egg_audio_play_sound(0,52,1.0,0.0); break; // i
    case 0x00070012: egg_audio_play_sound(0,53,1.0,0.0); break; // o
    case 0x00070013: egg_audio_play_sound(0,54,1.0,0.0); break; // p
    case 0x00070004: egg_audio_play_sound(0,55,1.0,0.0); break; // a
    case 0x00070016: egg_audio_play_sound(0,56,1.0,0.0); break; // s
    case 0x00070007: egg_audio_play_sound(0,57,1.0,0.0); break; // d
    case 0x00070009: egg_audio_play_sound(0,58,1.0,0.0); break; // f
    case 0x0007000a: egg_audio_play_sound(0,59,1.0,0.0); break; // g
    case 0x0007000b: egg_audio_play_sound(0,60,1.0,0.0); break; // h
    case 0x0007000d: egg_audio_play_sound(0,61,1.0,0.0); break; // j
    case 0x0007000e: egg_audio_play_sound(0,62,1.0,0.0); break; // k
    case 0x0007000f: egg_audio_play_sound(0,63,1.0,0.0); break; // l
    case 0x00070033: egg_audio_play_sound(0,64,1.0,0.0); break; // ;
    case 0x0007001d: egg_audio_play_sound(0,65,1.0,0.0); break; // z
    case 0x0007001b: egg_audio_play_sound(0,66,1.0,0.0); break; // x
    case 0x00070006: egg_audio_play_sound(0,67,1.0,0.0); break; // c
    case 0x00070019: egg_audio_play_sound(0,68,1.0,0.0); break; // v
    case 0x00070005: egg_audio_play_sound(0,69,1.0,0.0); break; // b
    case 0x00070011: egg_audio_play_sound(0,70,1.0,0.0); break; // n
    case 0x00070010: egg_audio_play_sound(0,71,1.0,0.0); break; // m
    case 0x00070036: egg_audio_play_sound(0,72,1.0,0.0); break; // ,
    case 0x00070037: egg_audio_play_sound(0,73,1.0,0.0); break; // .
    case 0x00070038: egg_audio_play_sound(0,74,1.0,0.0); break; // /
    case 0x0007005f: egg_audio_play_sound(0,75,1.0,0.0); break; // kp7
    case 0x00070060: egg_audio_play_sound(0,76,1.0,0.0); break; // kp8
    case 0x00070061: egg_audio_play_sound(0,77,1.0,0.0); break; // kp9
    case 0x0007005c: egg_audio_play_sound(0,78,1.0,0.0); break; // kp4
    case 0x0007005d: egg_audio_play_sound(0,79,1.0,0.0); break; // kp5
    case 0x0007005e: egg_audio_play_sound(0,80,1.0,0.0); break; // kp6
    case 0x00070059: egg_audio_play_sound(0,81,1.0,0.0); break; // kp1
    case 0x0007005a: egg_audio_play_sound(0,82,1.0,0.0); break; // kp2
    case 0x0007005b: egg_audio_play_sound(0,83,1.0,0.0); break; // kp3
    
    case 0x00070029: egg_request_termination(); break; // ESC
    case 0x0007003a: { // F1
        if (egg_event_enable(EGG_EVENT_MMOTION,EGG_EVTSTATE_QUERY)==EGG_EVTSTATE_ENABLED) {
          egg_event_enable(EGG_EVENT_MMOTION,EGG_EVTSTATE_DISABLED);
          egg_event_enable(EGG_EVENT_MBUTTON,EGG_EVTSTATE_DISABLED);
          egg_event_enable(EGG_EVENT_MWHEEL,EGG_EVTSTATE_DISABLED);
        } else {
          egg_event_enable(EGG_EVENT_MMOTION,EGG_EVTSTATE_ENABLED);
          egg_event_enable(EGG_EVENT_MBUTTON,EGG_EVTSTATE_ENABLED);
          egg_event_enable(EGG_EVENT_MWHEEL,EGG_EVTSTATE_ENABLED);
        }
      } break;
    case 0x0007003b: { // F2
        if (egg_event_enable(EGG_EVENT_TEXT,EGG_EVTSTATE_QUERY)==EGG_EVTSTATE_ENABLED) {
          egg_event_enable(EGG_EVENT_TEXT,EGG_EVTSTATE_DISABLED);
        } else {
          egg_event_enable(EGG_EVENT_TEXT,EGG_EVTSTATE_ENABLED);
        }
      } break;
  }
}

static void on_input_connect(int devid) {
  egg_log("CONNECT %d",devid);
  char name[256];
  int namec=egg_input_device_get_name(name,sizeof(name),devid);
  if ((namec<0)||(namec>sizeof(name))) namec=0;
  int vid=0,pid=0,version=0;
  egg_input_device_get_ids(&vid,&pid,&version,devid);
  egg_log("name='%.*s' vid=%04x pid=%04x version=%04x",namec,name,vid,pid,version);
  int btnix=0; for (;;btnix++) {
    int btnid=0,hidusage=0,lo=0,hi=0,value=0;
    egg_input_device_get_button(&btnid,&hidusage,&lo,&hi,&value,devid,btnix);
    if (!btnid) break;
    egg_log("  %d 0x%08x %d..%d =%d",btnid,hidusage,lo,hi,value);
  }
}

void egg_client_update(double elapsed) {
  //egg_log("egg_client_update %f",elapsed);
  //egg_request_termination();
  struct egg_event eventv[32];
  int eventc=egg_event_next(eventv,sizeof(eventv)/sizeof(eventv[0]));
  const struct egg_event *event=eventv;
  for (;eventc-->0;event++) switch (event->type) {
    case EGG_EVENT_INPUT: egg_log("INPUT %d.%d=%d",event->v[0],event->v[1],event->v[2]); break;
    case EGG_EVENT_CONNECT: on_input_connect(event->v[0]); break;
    case EGG_EVENT_DISCONNECT: egg_log("DISCONNECT %d",event->v[0]); break;
    case EGG_EVENT_HTTP_RSP: {
        egg_log("HTTP_RSP reqid=%d status=%d length=%d zero=%d",event->v[0],event->v[1],event->v[2],event->v[3]);
        char body[1024];
        if (event->v[2]<=sizeof(body)) {
          if (egg_http_get_body(body,sizeof(body),event->v[0])==event->v[2]) {
            egg_log(">> %.*s",event->v[2],body);
          }
        }
      } break;
    case EGG_EVENT_WS_CONNECT: egg_log("WS_CONNECT %d",event->v[0]); break;
    case EGG_EVENT_WS_DISCONNECT: egg_log("WS_DISCONNECT %d",event->v[0]); break;
    case EGG_EVENT_WS_MESSAGE: {
        char msg[1024];
        if (event->v[3]<=sizeof(msg)) {
          if (egg_ws_get_message(msg,sizeof(msg),event->v[0],event->v[1])==event->v[3]) {
            egg_log(">> %.*s",event->v[3],msg);
          }
        }
      } break;
    case EGG_EVENT_MMOTION: egg_log("MMOTION %d,%d",event->v[0],event->v[1]); break;
    case EGG_EVENT_MBUTTON: egg_log("MBUTTON %d=%d @%d,%d",event->v[0],event->v[1],event->v[2],event->v[3]); break;
    case EGG_EVENT_MWHEEL: egg_log("MWHEEL +%d,%d @%d,%d",event->v[0],event->v[1],event->v[2],event->v[3]); break;
    case EGG_EVENT_KEY: on_key(event->v[0],event->v[1]); break;
    case EGG_EVENT_TEXT: egg_log("TEXT U+%x",event->v[0]); break;
    default: egg_log("UNKNOWN EVENT: %d[%d,%d,%d,%d]",event->type,event->v[0],event->v[1],event->v[2],event->v[3]);
  }
  
  {
    struct sprite *sprite=spritev;
    int i=spritec;
    for (;i-->0;sprite++) {
      sprite->x+=sprite->dx;
      sprite->y+=sprite->dy;
      if ((sprite->x<0)&&(sprite->dx<0)) sprite->dx=-sprite->dx;
      else if ((sprite->x>=screenw)&&(sprite->dx>0)) sprite->dx=-sprite->dx;
      if ((sprite->y<0)&&(sprite->dy<0)) sprite->dy=-sprite->dy;
      else if ((sprite->y>=screenh)&&(sprite->dy>0)) sprite->dy=-sprite->dy;
    }
  }
}
/**/

static int tr=0,tg=0,tb=0,ta=0,ga=0;
static int dtr=1,dtg=5,dtb=3,dta=2,dga=1;

void egg_client_render() {
  egg_draw_rect(1,0,0,screenw,screenh,0x008000ff);
  egg_draw_rect(1,10,10,screenw-20,screenh-20,0xff0000ff);
  egg_draw_rect(1,11,11,screenw-22,screenh-22,0x0000ffff);
  egg_draw_rect(1,78,0,screenw,screenh,0xffffff80);
  egg_draw_rect(1,0,0,40,30,0x00000080);
  
  { // Lots of sprites with random xform and motion.
    struct egg_draw_tile vtxv[SPRITEA];
    struct sprite *sprite=spritev;
    struct egg_draw_tile *vtx=vtxv;
    int i=spritec;
    for (;i-->0;sprite++,vtx++) {
      vtx->x=sprite->x;
      vtx->y=sprite->y;
      vtx->tileid=sprite->tileid;
      vtx->xform=sprite->xform;
    }
    egg_draw_tile(1,texid_tilesheet,vtxv,spritec);
  }
  
  #define ANIMATE(prop) prop+=d##prop; if (prop<0) { prop=0; d##prop=-d##prop; } else if (prop>0xff) { prop=0xff; d##prop=-d##prop; }
  ANIMATE(tr)
  ANIMATE(tg)
  ANIMATE(tb)
  ANIMATE(ta)
  ANIMATE(ga)
  #undef ANIMATE
  
  // Game's title, fading in and out via global alpha.
  egg_draw_mode(EGG_XFERMODE_ALPHA,0,ga);
  int titlew=0,titleh=0;
  egg_texture_get_header(&titlew,&titleh,0,texid_title);
  egg_draw_decal(1,texid_title,0,0,0,0,titlew,titleh,0);
  
  // Tiles and a decal with animated tint.
  egg_draw_mode(EGG_XFERMODE_ALPHA,(tr<<24)|(tg<<16)|(tb<<8)|ta,0xff);
  {
    struct egg_draw_tile vtxv[]={
      {100,100,0x00,0},
      {120,100,0x01,0},
      {140,100,0xff,0},
      {160,100,0x02,0},
    };
    egg_draw_tile(1,texid_tilesheet,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  egg_draw_decal(1,texid_dot,70,92,0,0,16,16,0);
  egg_draw_mode(EGG_XFERMODE_ALPHA,0,0xff);
  
  { // Transforms reference, and transformed tiles.
    struct egg_draw_tile vtxv[]={
      { 60,120,0x10,0}, // reference...
      { 90,120,0x11,0},
      {120,120,0x12,0},
      {150,120,0x13,0},
      {180,120,0x14,0},
      {210,120,0x15,0},
      {240,120,0x16,0},
      {270,120,0x17,0}, // ...reference
      { 60,140,0x10,0}, // xform...
      { 90,140,0x10,1},
      {120,140,0x10,2},
      {150,140,0x10,3},
      {180,140,0x10,4},
      {210,140,0x10,5},
      {240,140,0x10,6},
      {270,140,0x10,7}, // ...xform
    };
    egg_draw_tile(1,texid_tilesheet,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  { // Transformed decals.
    egg_draw_decal(1,texid_rotate, 48,150,8,8,24,16,0); // none
    egg_draw_decal(1,texid_rotate, 78,150,8,8,24,16,1); // XREV
    egg_draw_decal(1,texid_rotate,108,150,8,8,24,16,2); // YREV
    egg_draw_decal(1,texid_rotate,138,150,8,8,24,16,3); // XREV|YREV
    egg_draw_decal(1,texid_rotate,168,150,8,8,24,16,4); // SWAP
    egg_draw_decal(1,texid_rotate,198,150,8,8,24,16,5); // SWAP|XREV
    egg_draw_decal(1,texid_rotate,228,150,8,8,24,16,6); // SWAP|YREV
    egg_draw_decal(1,texid_rotate,258,150,8,8,24,16,7); // SWAP|XREV|YREV
  }
}
#endif
