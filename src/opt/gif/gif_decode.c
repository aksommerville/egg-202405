#include "gif.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#if USE_mswin
  #define LITTLE_ENDIAN 1234
  #define BIG_ENDIAN 4321
  #define BYTE_ORDER LITTLE_ENDIAN
#elif USE_macos
  #include <machine/endian.h>
#else
  #include <endian.h>
#endif

/* Decoder context.
 */
 
struct gif_decoder {
  struct gif_image *image;
  uint32_t *gct; // 256 entries or null (regardless of source size). RGBA, ready to drop in the output image.
  uint32_t *lct;
  struct gif_dict_entry {
    int a,b;
  } dict[4096];
  
  // Output iterator.
  const uint32_t *ctab;
  int ctabsize;
  int colorkey;
  int dststridewords;
  uint32_t *dstrow,*dstp;
  int dstrowc,dstcolc;
  int dstcolc0;
};

static void gif_decoder_cleanup(struct gif_decoder *ctx) {
  gif_image_del(ctx->image);
  if (ctx->gct) free(ctx->gct);
  if (ctx->lct) free(ctx->lct);
}

/* Fill a rectangle in the output image, with an RGBA pixel.
 */
 
static void gif_fill(struct gif_image *image,int x,int y,int w,int h,uint32_t pixel) {
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>image->w-w) w=image->w-x;
  if (y>image->h-h) h=image->h-y;
  if ((w<1)||(h<1)) return;
  uint8_t *row=(uint8_t*)image->v+image->stride*y+(x<<2);
  int yi=h; for (;yi-->0;row+=image->stride) {
    uint32_t *p=(uint32_t*)row;
    int xi=w; for (;xi-->0;p++) *p=pixel;
  }
}

/* Decode a color table.
 * (ctc) in entries; input length is (ctc*3) bytes.
 * Always produces a full 256-color table, and caller must free it.
 * Output values are ready to drop into our RGBA output.
 */
 
static uint32_t *gif_ctab_decode(const uint8_t *src,int ctc) {
  if (ctc>256) ctc=256;
  uint32_t *dst=calloc(4,256);
  if (!dst) return 0;
  uint32_t *dstp=dst;
  for (;ctc-->0;src+=3,dstp++) {
    #if BYTE_ORDER==BIG_ENDIAN
      *dstp=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|0xff;
    #else
      *dstp=src[0]|(src[1]<<8)|(src[2]<<16)|0xff000000;
    #endif
  }
  return dst;
}

/* Measure a chain of data-sub-blocks through its terminating zero.
 */
 
static int gif_measure_subblocks(const uint8_t *src,int srcc) {
  int srcp=0;
  for (;;) {
    if (srcp>=srcc) return -1;
    uint8_t len=src[srcp++];
    if (!len) return srcp;
    srcp+=len;
  }
}

/* Graphic Control Extension.
 */
 
struct gif_gce {
  uint8_t flags; // 1c=Disposal, 02=Input, 01=Colorkey
  uint16_t delay; // s/100
  int colorkey;
};

static int gif_gce_decode(
  struct gif_gce *gce,
  struct gif_decoder *ctx,
  const uint8_t *src,int srcc
) {
  // We're looking at a data-sub-blocks chain, which must be at least* 4 bytes long, and all in the first segment.
  // * Per spec, *exactly* 4 bytes long, but we won't be picky about that.
  if (srcc<5) return -1;
  if (src[0]<4) return -1;
  gce->flags=src[1];
  gce->delay=src[2]|(src[3]<<8);
  gce->colorkey=src[4];
  return 0;
}

static int gif_gce_get_disposal(const struct gif_gce *gce) {
  return ((gce->flags>>2)&3);
}

#define GIF_DISPOSAL_NONE 0
#define GIF_DISPOSAL_KEEP 1
#define GIF_DISPOSAL_BG   2
#define GIF_DISPOSAL_PREV 3
// 4..7 reserved. NONE and KEEP are equivalent.

/* Prepare context for outputting pixels.
 */
 
static int gif_decoder_prep_output(
  struct gif_decoder *ctx,
  int x,int y,int w,int h,
  int mincodesize,
  const struct gif_gce *gce
) {
  if ((x<0)||(y<0)||(w<1)||(h<1)||(x>ctx->image->w-w)||(y>ctx->image->h-h)) return -1;
  if (ctx->lct) ctx->ctab=ctx->lct;
  else if (ctx->gct) ctx->ctab=ctx->gct;
  else return -1;
  ctx->ctabsize=1<<mincodesize;
  ctx->colorkey=gce->colorkey;
  ctx->dststridewords=ctx->image->stride>>2;
  ctx->dstrow=(uint32_t*)ctx->image->v+y*ctx->dststridewords+x;
  ctx->dstp=ctx->dstrow;
  ctx->dstrowc=h;
  ctx->dstcolc=ctx->dstcolc0=w;
  return 0;
}

/* Resolve one LZW codeword and emit it, per previous preparation.
 * Caller ensures the word is defined.
 */
 
static int gif_decoder_emit_code(struct gif_decoder *ctx,int code) {
  if ((code<0)||(code>0xfff)) return -1;
  if (!ctx->dstp) return 0; // output complete
  if (code<ctx->ctabsize) {
    if (code!=ctx->colorkey) {
      *(ctx->dstp)=ctx->ctab[code];
    }
    if (ctx->dstcolc>1) {
      ctx->dstcolc--;
      ctx->dstp++;
    } else if (ctx->dstrowc>1) {
      ctx->dstrowc--;
      ctx->dstrow+=ctx->dststridewords;
      ctx->dstp=ctx->dstrow;
      ctx->dstcolc=ctx->dstcolc0;
    } else {
      ctx->dstp=0;
    }
  } else {
    gif_decoder_emit_code(ctx,ctx->dict[code].a);
    gif_decoder_emit_code(ctx,ctx->dict[code].b);
  }
  return 0;
}

/* Look up the first unit from a code sequence.
 */
 
static int gif_decoder_first_unit(const struct gif_decoder *ctx,int code) {
  int panic=0x1000;
  while (panic-->0) {
    if (code<ctx->ctabsize) return code;
    code=ctx->dict[code].a;
  }
  return 0;
}

/* Decode Image Descriptor, Local Color Table, and Table Based Image Data.
 * Apply to decoder's image.
 * Return length consumed.
 * Input must begin with the 0x2c Image Separator.
 */
 
static int gif_decode_subimage(
  struct gif_decoder *ctx,
  const uint8_t *src,int srcc,
  const struct gif_gce *gce
) {
  if (srcc<10) return -1;
  if (src[0]!=0x2c) return -1;
  int x=src[1]|(src[2]<<8);
  int y=src[3]|(src[4]<<8);
  int w=src[5]|(src[6]<<8);
  int h=src[7]|(src[8]<<8);
  int flags=src[9];
  int srcp=10;
  
  if (ctx->lct) {
    free(ctx->lct);
    ctx->lct=0;
  }
  if (flags&0x80) {
    int lctc=1<<((flags&7)+1);
    int lctbytec=lctc*3;
    if (srcp>srcc-lctbytec) return -1;
    if (!(ctx->lct=gif_ctab_decode(src+srcp,lctc))) return -1;
    srcp+=lctbytec;
  }
  
  if (srcp>=srcc) return -1;
  int mincodesize=src[srcp++];
  if ((mincodesize<2)||(mincodesize>11)) return -1;
  if (gif_decoder_prep_output(ctx,x,y,w,h,mincodesize,gce)<0) return -1;
  int prevcode=-1;
  int code=0; // incoming code word
  int codemask=1; // next bit to set in (code).
  int codelimit=1<<(mincodesize+1); // code is ready when mask reaches this.
  int clear=1<<mincodesize;
  int eoi=clear+1;
  int next_assignable=eoi+1;
  uint8_t srcmask=1; // next bit to read from input.
  
  for (;;) {
    if (srcp>=srcc) return -1;
    uint8_t chunklen=src[srcp++];
    if (!chunklen) break;
    while (chunklen) {
      if (src[srcp]&srcmask) code|=codemask;
      if (srcmask==0x80) { srcmask=1; srcp++; chunklen--; }
      else srcmask<<=1;
      codemask<<=1;
      if (codemask>=codelimit) {
        if (code==clear) {
          codelimit=1<<(mincodesize+1);
          next_assignable=eoi+1;
        } else if (code==eoi) {
          ctx->dstp=0; // prevent further output
        } else {
        
          if (code<clear) { // Emit one pixel.
            gif_decoder_emit_code(ctx,code);
            
          } else if (code>next_assignable) {
            return -1;
            
          } else if (code==next_assignable) {
            gif_decoder_emit_code(ctx,prevcode);
            gif_decoder_emit_code(ctx,gif_decoder_first_unit(ctx,prevcode));
            
          } else { // Emit sequence from dictionary.
            gif_decoder_emit_code(ctx,code);
          }
          
          if (prevcode>=0) {
            if (next_assignable<0xfff) {
              if (code==next_assignable) {
                ctx->dict[next_assignable]=(struct gif_dict_entry){prevcode,gif_decoder_first_unit(ctx,prevcode)};
              } else {
                ctx->dict[next_assignable]=(struct gif_dict_entry){prevcode,gif_decoder_first_unit(ctx,code)};
              }
              next_assignable++;
            }
            if (next_assignable>=codelimit) {
              codelimit<<=1;
            }
          }
          prevcode=code;
        }
        code=0;
        codemask=1;
      }
    }
  }
  
  return srcp;
}

/* Decode a Plain Text block and apply to image.
 * No need to return consumed length; our caller will measure it generically after.
 */
 
static int gif_text_decode(
  struct gif_decoder *ctx,
  const uint8_t *src,int srcc,
  const struct gif_gce *gce
) {
  fprintf(stderr,"TODO %s, limit %d bytes\n",__func__,srcc);
  // These are kind of silly. Do they ever get used in the field?
  // I'm not going to implement. Can revisit later if we feel like it.
  // The main thing: How do we acquire glyph data? To look at the spec, it seems we need to accomodate any glyph size at all.
  return 0;
}

/* With header decoded and output image initialized, consume more data off the stream.
 * Skips comments and such, and returns after each render block.
 */
 
static int gif_decode_more(struct gif_decoder *ctx,const uint8_t *src,int srcc) {
  struct gif_gce gce={.colorkey=-1};
  int srcp=0,err;
  while (srcp<srcc) {
    if (src[srcp]==0x3b) break; // Stop at EOF and don't consume it.
    
    // Image Separator: Decode and return.
    if (src[srcp]==0x2c) {
      if ((err=gif_decode_subimage(ctx,src+srcp,srcc-srcp,&gce))<1) return -1;
      return srcp+err;
    }
    
    // Anything else we can consume must be (0x21,label,data-sub-blocks).
    if (src[srcp++]!=0x21) return -1;
    if (srcp>=srcc) return -1;
    uint8_t label=src[srcp++];
    int done=0;
    switch (label) {
      case 0x01: { // Plain Text
          if (gif_text_decode(ctx,src+srcp,srcc-srcp,&gce)<0) return -1;
          done=1; // This is a Rendering Block, so we must return to flush the GCE.
        } break;
      case 0xf9: { // Graphic Control Extension
          if (gif_gce_decode(&gce,ctx,src+srcp,srcc-srcp)<0) return -1;
        } break;
      case 0xfe: // Comment, skip
      case 0xff: // Application Extension, skip
      default: break; // Skip unknown too.
    }
    if ((err=gif_measure_subblocks(src+srcp,srcc-srcp))<1) return -1;
    srcp+=err;
    if (done) return srcp;
  }
  return srcp;
}

/* Decode Logical Screen Descriptor and Global Color Table if present.
 * Return <0 or length consumed.
 * (src) should begin immediately after the 6-byte signature.
 * We create the image object.
 */
 
static int gif_decode_header(struct gif_decoder *ctx,const uint8_t *src,int srcc) {
  int srcp=0;
  if (srcp>srcc-7) return -1;
  int w=src[0]|(src[1]<<8);
  int h=src[2]|(src[3]<<8);
  int flags=src[4];
  int bgcolor=src[5];
  //int aspect=src[6]; // don't care
  srcp+=7;
  if ((w<1)||(w>0x7fff)) return -1;
  if ((h<1)||(h>0x7fff)) return -1;
  
  if (ctx->image) return -1;
  if (!(ctx->image=calloc(1,sizeof(struct gif_image)))) return -1;
  ctx->image->w=w;
  ctx->image->h=h;
  ctx->image->stride=w<<2;
  if (ctx->image->stride>INT_MAX/h) return -1;
  if (!(ctx->image->v=calloc(ctx->image->stride,h))) return -1;
  
  if (flags&0x80) { // GCT present
    int gctc=1<<((flags&7)+1);
    int gctcbytec=gctc*3;
    if (srcp>srcc-gctcbytec) return -1;
    if (!(ctx->gct=gif_ctab_decode(src+srcp,gctc))) return -1;
    srcp+=gctcbytec;
  }
  
  return srcp;
}

/* Decode in context.
 */
 
static int gif_decode_inner(struct gif_decoder *ctx,const uint8_t *src,int srcc) {
  
  if (srcc<6) return -1;
  if (memcmp(src,"GIF87a",6)&&memcmp(src,"GIF89a",6)) return -1;
  int srcp=6;
  
  int err=gif_decode_header(ctx,src+srcp,srcc-srcp);
  if (err<0) return -1;
  srcp+=err;
  
  while (srcp<srcc) {
    if (src[srcp]==0x3b) break; // EOF
    if ((err=gif_decode_more(ctx,src+srcp,srcc-srcp))<1) return -1;
    srcp+=err;
  }
  
  // We should assert here that at least one render block was present, the spec does say it's mandatory.
  // But we can operate sensibly without; you get a blank image based on the header. Good enough.
  
  return 0;
}

/* Decode, public entry point.
 */

struct gif_image *gif_decode(const void *src,int srcc) {
  struct gif_decoder decoder={0};
  if (gif_decode_inner(&decoder,src,srcc)<0) {
    gif_decoder_cleanup(&decoder);
    return 0;
  }
  struct gif_image *image=decoder.image;
  decoder.image=0;
  gif_decoder_cleanup(&decoder);
  return image;
}
