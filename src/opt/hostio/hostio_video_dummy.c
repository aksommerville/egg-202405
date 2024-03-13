/* hostio_video_dummy.c
 * Skeletal video driver, can be used as a stub, and commented as an example template.
 */

#include "hostio_internal.h"

/* Object definition.
 */
 
struct hostio_video_dummy {
  struct hostio_video hdr;
  struct hostio_video_fb_description fbdesc;
  void *fb;
};

#define DRIVER ((struct hostio_video_dummy*)driver)

/* Delete.
 */
 
static void _dummy_del(struct hostio_video *driver) {
  if (DRIVER->fb) free(DRIVER->fb);
}

/* Init.
 */
 
static int _dummy_init(struct hostio_video *driver,const struct hostio_video_setup *setup) {

  // Respect setup as applicable:
  //   device, title, iconrgba

  // Try to respect setup if it requests a specific size.
  if ((setup->w>0)&&(setup->h>0)) {
    driver->w=setup->w;
    driver->h=setup->h;
    
  // If a framebuffer size is provided, try to match its aspect ratio.
  // Bonus points if you also produce an integer multiple of the framebuffer size.
  } else if ((setup->fbw>0)&&(setup->fbh>0)) {
    driver->w=setup->fbw;
    driver->h=setup->fbh;
    
  // No size guidance, that's perfectly legal. Make up something sensible.
  } else {
    driver->w=640;
    driver->h=360;
  }
  
  // Try to respect fullscreen if requested.
  if (setup->fullscreen) {
    driver->fullscreen=1;
  }
  
  // If your driver provides a cursor, arrange for it to start hidden.
  driver->cursor_visible=0;
  
  /* If a framebuffer is requested, allocate it and stash the geometry.
   */
  if ((setup->fbw>0)&&(setup->fbh>0)) {
    if ((setup->fbw>4096)||(setup->fbh>4096)) return -1;
    DRIVER->fbdesc.w=setup->fbw;
    DRIVER->fbdesc.h=setup->fbh;
    DRIVER->fbdesc.stride=DRIVER->fbdesc.w<<2;
    DRIVER->fbdesc.pixelsize=32;
    DRIVER->fbdesc.rmask=0x00ff0000;
    DRIVER->fbdesc.gmask=0x0000ff00;
    DRIVER->fbdesc.bmask=0x000000ff;
    DRIVER->fbdesc.amask=0;
    if (*(uint8_t*)&DRIVER->fbdesc.bmask) {
      DRIVER->fbdesc.chorder[0]='b';
      DRIVER->fbdesc.chorder[1]='g';
      DRIVER->fbdesc.chorder[2]='r';
      DRIVER->fbdesc.chorder[3]='x';
    } else {
      DRIVER->fbdesc.chorder[0]='x';
      DRIVER->fbdesc.chorder[1]='r';
      DRIVER->fbdesc.chorder[2]='g';
      DRIVER->fbdesc.chorder[3]='b';
    }
    if (!(DRIVER->fb=calloc(DRIVER->fbdesc.stride,DRIVER->fbdesc.h))) return -1;
  }
  
  return 0;
}

/* Render fences.
 */
 
static int _dummy_fb_describe(struct hostio_video_fb_description *desc,struct hostio_video *driver) {
  if (!DRIVER->fb) return -1; // Framebuffer wasn't requested, too late to ask for it.
  *desc=DRIVER->fbdesc;
  return 0;
}

static void *_dummy_fb_begin(struct hostio_video *driver) {
  // Prepare a framebuffer and return it.
  // Framebuffer geometry must have been previously negotiated.
  return DRIVER->fb;
}

static int _dummy_fb_end(struct hostio_video *driver) {
  // Confirm that fb_begin was called recently, and commit that change.
  if (DRIVER->fb) {
    // Opportunity to capture frames or... something?
  }
  return 0;
}

/* Type definition.
 */
 
const struct hostio_video_type hostio_video_type_dummy={
  .name="dummy",
  .desc="Fake video driver that does nothing.",
  .objlen=sizeof(struct hostio_video_dummy),
  .appointment_only=1, // Normally zero, ie do include in the default list.
  .provides_input=0, // Nonzero if you're backed by a window manager with system keyboard and/or pointer.
  .del=_dummy_del,
  .init=_dummy_init,
  
  // Must implement if (provides_input), and call delegate as appropriate:
  //int update(struct hostio_video *driver)
  
  // Optional, implement if they make sense:
  //void show_cursor(struct hostio_video *driver,int show);
  //void set_fullscreen(struct hostio_video *driver,int fullscreen);
  //void suppress_screensaver(struct hostio_video *driver);
  
  // Must implement at least one set of render fences.
  // We're leaving the GX ones unset, because we really can't pretend about it.
  //.gx_begin=_dummy_gx_begin,
  //.gx_end=_dummy_gx_end,
  .fb_describe=_dummy_fb_describe,
  .fb_begin=_dummy_fb_begin,
  .fb_end=_dummy_fb_end,
};
