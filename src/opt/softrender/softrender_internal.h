#ifndef SOFTRENDER_INTERNAL_H
#define SOFTRENDER_INTERNAL_H

#include "softrender.h"
#include "opt/rawimg/rawimg.h"
#include "opt/hostio/hostio_video.h" /* For struct hostio_video_fb_description; linking not necessary. */
#include "egg/egg.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <endian.h> /* TODO MacOS and Windows, this works different. */

// Arbitrary sanity limit. We only enforce when growing the buffer, so may skip beyond this limit a little.
#define SOFTRENDER_TEXTURE_LIMIT 32

#define SOFTRENDER_SIZE_LIMIT 4096

/* Textures' (encfmt) may be replaced by one of these constants, as a usage hint.
 * Compare by identity, content is undefined.
 */
extern const char *softrender_hint_opaque; // 32-bit RGBA but alpha should be ignored, assume opaque.
extern const char *softrender_hint_zeroalpha; // 32-bit RGBA and all pixels are either zero or fully opaque.

struct softrender {
  
  /* texid is index in this list + 1.
   * List may be sparse.
   */
  struct rawimg **texturev;
  int texturec,texturea;

  // Public render mode.
  int xfermode;
  uint32_t tint;
  uint8_t alpha;
  
  void (*fbcvt)(uint32_t *src,int w,int h,int stridewords);
  uint32_t (*pxcvt)(uint32_t src);
};

void softrender_fbcvt_shl8(uint32_t *src,int w,int h,int stridewords);
void softrender_fbcvt_shr8(uint32_t *src,int w,int h,int stridewords);
void softrender_fbcvt_swap02(uint32_t *src,int w,int h,int stridewords);
void softrender_fbcvt_swap13(uint32_t *src,int w,int h,int stridewords);
void softrender_fbcvt_swap(uint32_t *src,int w,int h,int stridewords);

uint32_t softrender_pxcvt_swap(uint32_t src);

#endif
