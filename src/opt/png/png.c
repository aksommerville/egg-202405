#include "png.h"
#include <zlib.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* Cleanup.
 */
 
static void png_chunk_cleanup(struct png_chunk *chunk) {
  if (chunk->v) free(chunk->v);
}
 
void png_image_cleanup(struct png_image *image) {
  if (image->v) free(image->v);
  if (image->chunkv) {
    while (image->chunkc-->0) png_chunk_cleanup(image->chunkv+image->chunkc);
    free(image->chunkv);
  }
}
  
void png_image_del(struct png_image *image) {
  if (!image) return;
  png_image_cleanup(image);
  free(image);
}

/* New image.
 */

struct png_image *png_image_new(int w,int h,int depth,int colortype) {
  if ((w<1)||(w>0x7fff)) return 0;
  if ((h<1)||(h>0x7fff)) return 0;
  int pixelsize=png_calculate_pixel_size(depth,colortype);
  if (pixelsize<1) return 0;
  struct png_image *image=calloc(1,sizeof(struct png_image));
  if (!image) return 0;
  image->w=w;
  image->h=h;
  image->depth=depth;
  image->colortype=colortype;
  image->pixelsize=pixelsize;
  image->stride=png_minimum_stride(image->w,image->pixelsize);
  if (!(image->v=calloc(image->stride,image->h))) {
    png_image_del(image);
    return 0;
  }
  return image;
}

/* Add chunk, copying input.
 */

struct png_chunk *png_image_add_chunk(struct png_image *image,const char chunktype[4],const void *v,int c) {
  if (!image||(c<0)||(c&&!v)) return 0;
  if (image->chunkc>=image->chunka) {
    int na=image->chunka+8;
    if (na>INT_MAX/sizeof(struct png_chunk)) return 0;
    void *nv=realloc(image->chunkv,sizeof(struct png_chunk)*na);
    if (!nv) return 0;
    image->chunkv=nv;
    image->chunka=na;
  }
  struct png_chunk *chunk=image->chunkv+image->chunkc++;
  memset(chunk,0,sizeof(struct png_chunk));
  memcpy(chunk->chunktype,chunktype,sizeof(chunk->chunktype));
  if (c) {
    if (!(chunk->v=malloc(c))) {
      image->chunkc--;
      return 0;
    }
    memcpy(chunk->v,v,c);
    chunk->c=c;
  }
  return chunk;
}

/* Remove chunk.
 */
 
void png_image_remove_chunk(struct png_image *image,int p) {
  if ((p<0)||(p>=image->chunkc)) return;
  struct png_chunk *chunk=image->chunkv+p;
  png_chunk_cleanup(chunk);
  image->chunkc--;
  memmove(chunk,chunk+1,sizeof(struct png_chunk)*(image->chunkc-p));
}

/* Get chunk.
 */

int png_image_get_chunk(void *dstpp,const struct png_image *image,const char chunktype[4],int ix) {
  if (!image) return 0;
  const struct png_chunk *chunk=image->chunkv;
  int i=image->chunkc;
  for (;i-->0;chunk++) {
    if (memcmp(chunk->chunktype,chunktype,4)) continue;
    if (ix--) continue;
    *(void**)dstpp=chunk->v;
    return chunk->c;
  }
  return 0;
}

/* Measurement.
 */

int png_calculate_pixel_size(int depth,int colortype) {
  // Could just turn (colortype) into channel count and multiply by depth,
  // but we're taking this opportunity to validate the depth too.
  // Only PNG-legal combinations will pass.
  switch (colortype) {
    case 0: switch (depth) {
        case 1: return 1;
        case 2: return 2;
        case 4: return 4;
        case 8: return 8;
        case 16: return 16;
      } break;
    case 2: switch (depth) {
        case 8: return 24;
        case 16: return 48;
      } break;
    case 3: switch (depth) {
        case 1: return 1;
        case 2: return 2;
        case 4: return 4;
        case 8: return 8;
      } break;
    case 4: switch (depth) {
        case 8: return 16;
        case 16: return 32;
      } break;
    case 6: switch (depth) {
        case 8: return 32;
        case 16: return 64;
      } break;
  }
  return 0;
}

int png_minimum_stride(int w,int pixelsize) {
  if (w<1) return 0;
  if (pixelsize<1) return 0;
  if (w>(INT_MAX-7)/pixelsize) return 0;
  return (w*pixelsize+7)>>3;
}
