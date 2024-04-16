#include "lowasm_internal.h"

#define MOVE_SPEED 1

struct menu_xform_clipping {
  struct menu hdr;
  uint8_t xform;
  int offsetx,offsety;
  int dx,dy;
};

#define MENU ((struct menu_xform_clipping*)menu)

static void _xc_event(struct menu *menu,const struct egg_event *event) {
  switch (event->type) {
    case EGG_EVENT_KEY: switch (event->v[0]) {
        case 0x00070028: // Space or Enter: Next transform.
        case 0x0007002c: if (event->v[1]) MENU->xform=(MENU->xform+1)&7; break;
        case 0x0007004f: if (event->v[1]) MENU->dx= 1; else if (MENU->dx>0) MENU->dx=0; break;
        case 0x00070050: if (event->v[1]) MENU->dx=-1; else if (MENU->dx<0) MENU->dx=0; break;
        case 0x00070051: if (event->v[1]) MENU->dy= 1; else if (MENU->dy>0) MENU->dy=0; break;
        case 0x00070052: if (event->v[1]) MENU->dy=-1; else if (MENU->dy<0) MENU->dy=0; break;
      } break;
  }
}

static void _xc_render(struct menu *menu) {
  // Adjust offset if a key is held.
  MENU->offsetx+=MENU->dx*MOVE_SPEED;
  MENU->offsety+=MENU->dy*MOVE_SPEED;
  // Draw a transformed decal nine times: At the corners, edges, and center of the framebuffer.
  const int srcx=16,srcy=32,srcw=32,srch=48;
  int col=0; for (;col<3;col++) {
    int dstx=((col*lowasm.screenw)>>1)+MENU->offsetx;
    if (MENU->xform&EGG_XFORM_SWAP) dstx-=(srch>>1);
    else dstx-=(srcw>>1);
    int row=0; for (;row<3;row++) {
      int dsty=((row*lowasm.screenh)>>1)+MENU->offsety;
      if (MENU->xform&EGG_XFORM_SWAP) dsty-=(srcw>>1);
      else dsty-=(srch>>1);
      egg_draw_decal(1,lowasm.texid_misc,dstx,dsty,srcx,srcy,srcw,srch,MENU->xform);
    }
  }
  // One line of text describing the transform.
  char msg[64];
  int msgc=0;
  msg[msgc++]='0'+MENU->xform;
  if (MENU->xform&EGG_XFORM_XREV) { memcpy(msg+msgc," XREV",5); msgc+=5; }
  if (MENU->xform&EGG_XFORM_YREV) { memcpy(msg+msgc," YREV",5); msgc+=5; }
  if (MENU->xform&EGG_XFORM_SWAP) { memcpy(msg+msgc," SWAP",5); msgc+=5; }
  lowasm_tiles_begin(lowasm.texid_font,0xffffffff,0xc0);
  lowasm_tiles_string(msg,msgc,4,lowasm.screenh-40);
  lowasm_tiles_end();
}

struct menu *lowasm_menu_new_xform_clipping() {
  struct menu *menu=lowasm_menu_push(sizeof(struct menu_xform_clipping));
  if (!menu) return 0;
  menu->event=_xc_event;
  menu->render=_xc_render;
  return menu;
}
