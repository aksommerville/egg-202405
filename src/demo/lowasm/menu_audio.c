#include "lowasm_internal.h"

struct menu_audio {
  struct menu hdr;
  int optionp;
  int optionc;
  int songid;
  int force,repeat;
  int soundid;
  double trim,pan;
  int *songidv;
  int songidp,songidc,songida;
  int *soundidv;
  int soundidp,soundidc,soundida;
};

#define MENU ((struct menu_audio*)menu)

static void _audio_del(struct menu *menu) {
  if (MENU->songidv) free(MENU->songidv);
  if (MENU->soundidv) free(MENU->soundidv);
}

static void audio_activate(struct menu *menu) {
  switch (MENU->optionp) {
    case 0: break; // Playhead, not interactive
    case 1: egg_audio_play_song(0,MENU->songid,MENU->force,MENU->repeat); break; // Play song.
    case 2: break; // Force, not interactive.
    case 3: break; // Repeat, not interactive.
    case 4: egg_audio_play_sound(0,MENU->soundid,MENU->trim,MENU->pan); break; // Play sound.
    case 5: break; // Trim, not interactive.
    case 6: break; // Pan, not interactive.
  }
}

static void audio_horz(struct menu *menu,int dx) {
  switch (MENU->optionp) {
    #define WRAP(tag) MENU->tag##p+=dx; if (MENU->tag##p>=MENU->tag##c) MENU->tag##p=0; else if (MENU->tag##p<0) MENU->tag##p=MENU->tag##c-1; MENU->tag=MENU->tag##v[MENU->tag##p];
    #define BOOL(tag) MENU->tag=MENU->tag?0:1;
    #define CLAMP(tag,lo,hi) MENU->tag+=dx*0.1; if (MENU->tag<lo) MENU->tag=lo; else if (MENU->tag>hi) MENU->tag=hi;
    case 0: break; // Playhead.
    case 1: WRAP(songid) break;
    case 2: BOOL(force) break;
    case 3: BOOL(repeat) break;
    case 4: WRAP(soundid) break;
    case 5: CLAMP(trim,0.0,1.0) break;
    case 6: CLAMP(pan,-1.0,1.0) break;
    #undef WRAP
    #undef BOOL
    #undef CLAMP
  }
}

static void _audio_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_KEY: if (event->v[1]) switch (event->v[0]) {
        case 0x00070028: audio_activate(menu); break; // enter
        case 0x0007002c: audio_activate(menu); break; // space
        case 0x0007004f: audio_horz(menu,1); break; // arrows...
        case 0x00070050: audio_horz(menu,-1); break;
        case 0x00070051: MENU->optionp++; if (MENU->optionp>=MENU->optionc) MENU->optionp=0; break;
        case 0x00070052: MENU->optionp--; if (MENU->optionp<0) MENU->optionp=MENU->optionc-1; break;
      } break;
  }
}

static void _audio_render(struct menu *menu) {
  const int x0=8,y0=10; // Where the first tile goes.
  if ((MENU->optionp>=0)&&(MENU->optionp<MENU->optionc)) {
    egg_draw_rect(1,2,y0+MENU->optionp*8-5,lowasm.screenw-4,9,0x000000ff);
  }
  lowasm_tiles_begin(lowasm.texid_font,0xffffffff,0xff);
  int y=y0;
  lowasm_tiles_stringf(x0,y,"Playhead: %f",egg_audio_get_playhead()); y+=8;
  lowasm_tiles_stringf(x0,y,"Play song %d",MENU->songid); y+=8;
  lowasm_tiles_stringf(x0,y," Force: %s",MENU->force?"TRUE":"FALSE"); y+=8;
  lowasm_tiles_stringf(x0,y," Repeat: %s",MENU->repeat?"TRUE":"FALSE"); y+=8;
  lowasm_tiles_stringf(x0,y,"Play sound %d",MENU->soundid); y+=8;
  lowasm_tiles_stringf(x0,y," Trim: %f",MENU->trim); y+=8;
  lowasm_tiles_stringf(x0,y," Pan: %f",MENU->pan); y+=8;
  lowasm_tiles_end();
}

static int audio_songidv_append(struct menu *menu,int id) {
  if (MENU->songidc>=MENU->songida) {
    int na=MENU->songida+16;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(MENU->songidv,sizeof(int)*na);
    if (!nv) return -1;
    MENU->songidv=nv;
    MENU->songida=na;
  }
  MENU->songidv[MENU->songidc++]=id;
  return 0;
}

static int audio_soundidv_append(struct menu *menu,int id) {
  if (MENU->soundidc>=MENU->soundida) {
    int na=MENU->soundida+16;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(MENU->soundidv,sizeof(int)*na);
    if (!nv) return -1;
    MENU->soundidv=nv;
    MENU->soundida=na;
  }
  MENU->soundidv[MENU->soundidc++]=id;
  return 0;
}

static int audio_populate_resource_lists(struct menu *menu) {
  audio_songidv_append(menu,0);
  int p=0;
  for (;;p++) {
    int tid=0,qual=0,rid=0;
    egg_res_id_by_index(&tid,&qual,&rid,p);
    if (!tid) break;
    if (qual) continue;
    if (tid==EGG_TID_song) audio_songidv_append(menu,rid);
    else if (tid==EGG_TID_sound) audio_soundidv_append(menu,rid);
  }
  return 0;
}

struct menu *lowasm_menu_new_audio() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_audio));
  if (!menu) return 0;
  menu->del=_audio_del;
  menu->event=_audio_event;
  menu->render=_audio_render;
  MENU->optionc=7;
  MENU->force=1;
  MENU->repeat=1;
  MENU->trim=1.0;
  audio_populate_resource_lists(menu);
  if (MENU->songidc>0) MENU->songid=MENU->songidv[MENU->songidp];
  if (MENU->soundidc>0) MENU->soundid=MENU->soundidv[MENU->soundidp];
  return menu;
}
