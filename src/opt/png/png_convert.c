#include "png.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

/* Abstract functions for reading plain integers from a packed row.
 * This is an inefficient way to go about it, but the alternative is a ton of highly specific this-to-that converters.
 * These plain integer functions should only be used for index, but we do supply 16+ bit pixels just in case.
 */
 
typedef int (*png_ird_fn)(const uint8_t *src,int x);
typedef void (*png_iwr_fn)(uint8_t *dst,int x,int v);

static int png_ird_noop(const uint8_t *src,int x) { return 0; }
static void png_iwr_noop(uint8_t *dst,int x,int v) {}

static int png_ird_1(const uint8_t *src,int x) { return (src[x>>3]&(0x80>>(x&7)))?1:0; }
static void png_iwr_1(uint8_t *dst,int x,int v) { if (v&1) dst[x>>3]|=0x80>>(x&7); else dst[x>>3]&=~(0x80>>(x&7)); }

static int png_ird_2(const uint8_t *src,int x) { return (src[x>>2]>>((3-(x&3))<<1))&3; }
static void png_iwr_2(uint8_t *dst,int x,int v) {
  int shift=(3-(x&3))<<1;
  uint8_t mask=3<<shift;
  dst[x>>2]=(dst[x>>2]&~mask)|((v&3)<<shift);
}

static int png_ird_4(const uint8_t *src,int x) { if (x&1) return src[x>>1]&15; return src[x>>1]>>4; }
static void png_iwr_4(uint8_t *dst,int x,int v) {
  if (x&1) dst[x>>1]=(dst[x>>1]&0xf0)|(v&15);
  else dst[x>>1]=(dst[x>>1]&0x0f)|(v<<4);
}

static int png_ird_8(const uint8_t *src,int x) { return src[x]; }
static void png_iwr_8(uint8_t *dst,int x,int v) { dst[x]=v; }

static int png_ird_16(const uint8_t *src,int x) { x*=2; return (src[x]<<8)|src[x+1]; }
static void png_iwr_16(uint8_t *dst,int x,int v) { x*=2; dst[x]=v>>8; dst[x+1]=v; }

static int png_ird_24(const uint8_t *src,int x) { x*=3; return (src[x]<<16)|(src[x+1]<<8)|src[x+2]; }
static void png_iwr_24(uint8_t *dst,int x,int v) { x*=3; dst[x]=v>>16; dst[x+1]=v>>8; dst[x+2]=v; }

static int png_ird_32(const uint8_t *src,int x) { x*=4; return (src[x]<<24)|(src[x+1]<<16)|(src[x+2]<<8)|src[x+3]; }
static void png_iwr_32(uint8_t *dst,int x,int v) { x*=4; dst[x]=v>>24; dst[x+1]=v>>16; dst[x+2]=v>>8; dst[x+3]=v; }

static int png_ird_48(const uint8_t *src,int x) { x*=6; return (src[x]<<16)|(src[x+2]<<8)|src[x+4]; }
static void png_iwr_48(uint8_t *dst,int x,int v) { x*=6; dst[x]=dst[x+1]=v>>16; dst[x+2]=dst[x+3]=v>>8; dst[x+4]=dst[x+5]=v; }

static int png_ird_64(const uint8_t *src,int x) { x*=8; return (src[x]<<24)|(src[x+2]<<16)|(src[x+4]<<8)|src[x+6]; }
static void png_iwr_64(uint8_t *dst,int x,int v) { x*=8; dst[x]=dst[x+1]=v>>24; dst[x+2]=dst[x+3]=v>>16; dst[x+4]=dst[x+5]=v>>8; dst[x+6]=dst[x+7]=v; }

static png_ird_fn png_ird_get(int pixelsize) {
  switch (pixelsize) {
    case 1: return png_ird_1;
    case 2: return png_ird_2;
    case 4: return png_ird_4;
    case 8: return png_ird_8;
    case 16: return png_ird_16;
    case 24: return png_ird_24;
    case 32: return png_ird_32;
    case 48: return png_ird_48;
    case 64: return png_ird_64;
  }
  return png_ird_noop;
}

static png_iwr_fn png_iwr_get(int pixelsize) {
  switch (pixelsize) {
    case 1: return png_iwr_1;
    case 2: return png_iwr_2;
    case 4: return png_iwr_4;
    case 8: return png_iwr_8;
    case 16: return png_iwr_16;
    case 24: return png_iwr_24;
    case 32: return png_iwr_32;
    case 48: return png_iwr_48;
    case 64: return png_iwr_64;
  }
  return png_iwr_noop;
}

/* Abstract functions for reading and writing canonical RGBA.
 * Index is treated as Gray.
 */
 
struct png_rgba { uint8_t r,g,b,a; };
 
typedef struct png_rgba (*png_rgbard_fn)(const uint8_t *src,int x);
typedef void (*png_rgbawr_fn)(uint8_t *dst,int x,struct png_rgba rgba);

static struct png_rgba png_rgbard_noop(const uint8_t *src,int x) {
  return (struct png_rgba){0,0,0,0};
}
static void png_rgbawr_noop(uint8_t *dst,int x,struct png_rgba v) {
}

static struct png_rgba png_rgbard_y1(const uint8_t *src,int x) {
  return (src[x>>3]&(0x80>>(x&7)))?(struct png_rgba){0xff,0xff,0xff,0xff}:(struct png_rgba){0,0,0,0xff};
}
static void png_rgbawr_y1(uint8_t *dst,int x,struct png_rgba v) {
  if (v.r+v.g+v.b>=384) dst[x>>3]|=0x80>>(x&7);
  else dst[x>>3]&=~(0x80>>(x&7));
}

static struct png_rgba png_rgbard_y2(const uint8_t *src,int x) {
  uint8_t y=png_ird_2(src,x);
  y|=y<<2;
  y|=y<<4;
  return (struct png_rgba){y,y,y,0xff};
}
static void png_rgbawr_y2(uint8_t *dst,int x,struct png_rgba v) {
  uint8_t y=((v.r+v.g+v.b)/3)>>6;
  png_iwr_2(dst,x,y);
}

static struct png_rgba png_rgbard_y4(const uint8_t *src,int x) {
  uint8_t y=png_ird_4(src,x);
  y|=y<<4;
  return (struct png_rgba){y,y,y,0xff};
}
static void png_rgbawr_y4(uint8_t *dst,int x,struct png_rgba v) {
  uint8_t y=((v.r+v.g+v.b)/3)>>4;
  png_iwr_4(dst,x,y);
}

static struct png_rgba png_rgbard_y8(const uint8_t *src,int x) {
  uint8_t y=src[x];
  return (struct png_rgba){y,y,y,0xff};
}
static void png_rgbawr_y8(uint8_t *dst,int x,struct png_rgba v) {
  dst[x]=(v.r+v.g+v.b)/3;
}

static struct png_rgba png_rgbard_y16(const uint8_t *src,int x) {
  uint8_t y=src[x<<1];
  return (struct png_rgba){y,y,y,0xff};
}
static void png_rgbawr_y16(uint8_t *dst,int x,struct png_rgba v) {
  x<<=1;
  dst[x]=dst[x+1]=(v.r+v.g+v.b)/3;
}

static struct png_rgba png_rgbard_rgb8(const uint8_t *src,int x) {
  src+=x*3;
  return (struct png_rgba){src[0],src[1],src[2],0xff};
}
static void png_rgbawr_rgb8(uint8_t *dst,int x,struct png_rgba v) {
  dst+=x*3;
  dst[0]=v.r;
  dst[1]=v.g;
  dst[2]=v.b;
}

static struct png_rgba png_rgbard_rgb16(const uint8_t *src,int x) {
  src+=x*6;
  return (struct png_rgba){src[0],src[2],src[4],0xff};
}
static void png_rgbawr_rgb16(uint8_t *dst,int x,struct png_rgba v) {
  dst+=x*6;
  dst[0]=dst[1]=v.r;
  dst[2]=dst[3]=v.g;
  dst[4]=dst[5]=v.b;
}

static struct png_rgba png_rgbard_ya8(const uint8_t *src,int x) {
  x<<=1;
  return (struct png_rgba){src[0],src[0],src[0],src[1]};
}
static void png_rgbawr_ya8(uint8_t *dst,int x,struct png_rgba v) {
  x<<=1;
  dst[0]=(v.r+v.g+v.b)/3;
  dst[1]=v.a;
}

static struct png_rgba png_rgbard_ya16(const uint8_t *src,int x) {
  x<<=2;
  return (struct png_rgba){src[0],src[0],src[0],src[2]};
}
static void png_rgbawr_ya16(uint8_t *dst,int x,struct png_rgba v) {
  x<<=2;
  dst[0]=dst[1]=(v.r+v.g+v.b)/3;
  dst[2]=dst[3]=v.a;
}

static struct png_rgba png_rgbard_rgba8(const uint8_t *src,int x) {
  x<<=2;
  return (struct png_rgba){src[0],src[1],src[2],src[3]};
}
static void png_rgbawr_rgba8(uint8_t *dst,int x,struct png_rgba v) {
  x<<=2;
  dst[0]=v.r;
  dst[1]=v.g;
  dst[2]=v.b;
  dst[3]=v.a;
}

static struct png_rgba png_rgbard_rgba16(const uint8_t *src,int x) {
  x<<=3;
  return (struct png_rgba){src[0],src[2],src[4],src[6]};
}
static void png_rgbawr_rgba16(uint8_t *dst,int x,struct png_rgba v) {
  x<<=3;
  dst[0]=dst[1]=v.r;
  dst[2]=dst[3]=v.g;
  dst[4]=dst[5]=v.b;
  dst[6]=dst[7]=v.a;
}

static png_rgbard_fn png_rgbard_get(int depth,int colortype) {
  switch (colortype) {
    case 0: case 3: switch (depth) {
        case 1: return png_rgbard_y1;
        case 2: return png_rgbard_y2;
        case 4: return png_rgbard_y4;
        case 8: return png_rgbard_y8;
        case 16: return png_rgbard_y16;
      } break;
    case 2: switch (depth) {
        case 8: return png_rgbard_rgb8;
        case 16: return png_rgbard_rgba16;
      } break;
    case 4: switch (depth) {
        case 8: return png_rgbard_ya8;
        case 16: return png_rgbard_ya16;
      } break;
    case 6: switch (depth) {
        case 8: return png_rgbard_rgba8;
        case 16: return png_rgbard_rgba16;
      } break;
  }
  return png_rgbard_noop;
}

static png_rgbawr_fn png_rgbawr_get(int depth,int colortype) {
  switch (colortype) {
    case 0: case 3: switch (depth) {
        case 1: return png_rgbawr_y1;
        case 2: return png_rgbawr_y2;
        case 4: return png_rgbawr_y4;
        case 8: return png_rgbawr_y8;
        case 16: return png_rgbawr_y16;
      } break;
    case 2: switch (depth) {
        case 8: return png_rgbawr_rgb8;
        case 16: return png_rgbawr_rgba16;
      } break;
    case 4: switch (depth) {
        case 8: return png_rgbawr_ya8;
        case 16: return png_rgbawr_ya16;
      } break;
    case 6: switch (depth) {
        case 8: return png_rgbawr_rgba8;
        case 16: return png_rgbawr_rgba16;
      } break;
  }
  return png_rgbawr_noop;
}

/* Color table lookup and reverse lookup.
 * (pltec) is in entries, not bytes.
 */
 
static struct png_rgba png_plte_lookup(const uint8_t *plte,int pltec,const uint8_t *trns,int trnsc,int ix) {
  if ((ix<0)||(ix>=pltec)) return (struct png_rgba){0,0,0,0};
  plte+=ix*3;
  uint8_t a=0xff;
  if (ix<trnsc) a=trns[ix];
  return (struct png_rgba){plte[0],plte[1],plte[2],a};
}

static int png_plte_reverse(const uint8_t *plte,int pltec,const uint8_t *trns,int trnsc,struct png_rgba rgba) {
  int best=0;
  int bestscore=INT_MAX;
  int ix=0; for (;ix<pltec;ix++) {
    uint8_t qr=*(plte++);
    uint8_t qg=*(plte++);
    uint8_t qb=*(plte++);
    uint8_t qa=0xff;
    if (ix<trnsc) qa=trns[ix];
    int dr=rgba.r-qr; if (dr<0) dr=-dr;
    int dg=rgba.g-qg; if (dg<0) dg=-dg;
    int db=rgba.b-qb; if (db<0) db=-db;
    int da=rgba.a-qa; if (da<0) da=-da;
    int score=dr+dg+db+da; // There are much better algorithms, but this is already too expensive by far.
    if (!score) return ix;
    if (score<bestscore) {
      best=ix;
      bestscore=score;
    }
  }
  return best;
}

/* Reformat image in place.
 */

int png_image_reformat(struct png_image *image,int depth,int colortype) {
  if (!image) return -1;
  int npixelsize=png_calculate_pixel_size(depth,colortype);
  if (npixelsize<1) return -1;
  int nstride=png_minimum_stride(image->w,npixelsize);
  if (nstride<1) return -1;
  if ((depth==image->depth)&&(colortype==image->colortype)&&(nstride==image->stride)) return 0;
  void *nv=calloc(nstride,image->h);
  if (!nv) return -1;
  const uint8_t *srcrow=image->v;
  uint8_t *dstrow=nv;
  int yi=image->h;
  
  // Format didn't change, do rowwise memcpy.
  if ((depth==image->depth)&&(colortype==image->colortype)) {
    for (;yi-->0;srcrow+=image->stride,dstrow+=nstride) {
      memcpy(dstrow,srcrow,nstride);
    }
    
  // Index to index, read plain integers and pad or truncate. (don't even look at the color table).
  } else if ((colortype==3)&&(image->colortype==3)) {
    png_ird_fn rd=png_ird_get(image->pixelsize);
    png_iwr_fn wr=png_iwr_get(npixelsize);
    for (;yi-->0;srcrow+=image->stride,dstrow+=nstride) {
      int x=0; for (;x<image->w;x++) {
        wr(dstrow,x,rd(srcrow,x));
      }
    }
    
  } else {
    const void *plte=0,*trns=0;
    int pltec=0,trnsc=0;
    if ((colortype==3)||(image->colortype==3)) {
      pltec=png_image_get_chunk(&plte,image,"PLTE",0)/3;
      if (pltec>0) {
        trnsc=png_image_get_chunk(&trns,image,"tRNS",0);
      }
    }
    
    // From index, read from PLTE.
    if (pltec&&(image->colortype==3)) {
      png_ird_fn rd=png_ird_get(image->pixelsize);
      png_rgbawr_fn wr=png_rgbawr_get(depth,colortype);
      for (;yi-->0;srcrow+=image->stride,dstrow+=nstride) {
        int x=0; for (;x<image->w;x++) {
          wr(dstrow,x,png_plte_lookup(plte,pltec,trns,trnsc,rd(srcrow,x)));
        }
      }
      
    // To index, do expensive reverse lookups in PLTE. Caller must assign PLTE before converting.
    } else if (pltec&&(colortype==3)) {
      png_rgbard_fn rd=png_rgbard_get(image->depth,image->colortype);
      png_iwr_fn wr=png_iwr_get(npixelsize);
      for (;yi-->0;srcrow+=image->stride,dstrow+=nstride) {
        int x=0; for (;x<image->w;x++) {
          wr(dstrow,x,png_plte_reverse(plte,pltec,trns,trnsc,rd(srcrow,x)));
        }
      }
      
    // Normal anything-to-anything conversion. Index is treated as Gray.
    } else {
      png_rgbard_fn rd=png_rgbard_get(image->depth,image->colortype);
      png_rgbawr_fn wr=png_rgbawr_get(depth,colortype);
      for (;yi-->0;srcrow+=image->stride,dstrow+=nstride) {
        int x=0; for (;x<image->w;x++) {
          wr(dstrow,x,rd(srcrow,x));
        }
      }
    }
  }
  free(image->v);
  image->v=nv;
  image->depth=depth;
  image->colortype=colortype;
  image->pixelsize=npixelsize;
  image->stride=nstride;
  return 0;
}
