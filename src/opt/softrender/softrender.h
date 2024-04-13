/* softrender.h
 * Software-only implementation of the Egg render API.
 */
 
#ifndef SOFTRENDER_H
#define SOFTRENDER_H

#include <stdint.h>

struct softrender;
struct egg_draw_tile;
struct hostio_video_fb_description;

void softrender_del(struct softrender *softrender);
struct softrender *softrender_new();

void softrender_texture_del(struct softrender *softrender,int texid);
int softrender_texture_new(struct softrender *softrender);

int softrender_texture_load(struct softrender *softrender,int texid,int w,int h,int stride,int fmt,const void *src,int srcc);

void softrender_texture_get_header(int *w,int *h,int *fmt,const struct softrender *softrender,int texid);

void softrender_texture_clear(struct softrender *softrender,int texid);

void softrender_draw_mode(struct softrender *softrender,int xfermode,uint32_t replacement,uint8_t alpha);

void softrender_draw_rect(struct softrender *softrender,int texid,int x,int y,int w,int h,uint32_t pixel);

void softrender_draw_decal(
  struct softrender *softrender,
  int dsttexid,int srctexid,
  int dstx,int dsty,
  int srcx,int srcy,
  int w,int h,
  int xform
);

void softrender_draw_tile(
  struct softrender *softrender,
  int dsttexid,int srctexid,
  const struct egg_draw_tile *v,int c
);

/* Special hooks for the platform side only.
 * We'll prepare texture 1 according to the video driver.
 * Drivers are forbidden to change the framebuffer format after their first reporting of it.
 * Texture 1 is always in an Egg format while rendering, typically RGBA.
 * You must "finalize" before delivering to the driver, then we rewrite it in the driver's format if needed.
 */
int softrender_init_texture_1(struct softrender *softrender,const struct hostio_video_fb_description *desc);
void softrender_set_main(struct softrender *softrender,void *fb);
void softrender_finalize_frame(struct softrender *softrender);

#endif
