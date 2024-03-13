#include "gif.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

/* Encoder context.
 */
 
struct gif_encoder {
  struct sr_encoder *dst; // WEAK
  const struct gif_image *image; // WEAK
  uint8_t ctab[768]; // Formatted for encoding. (R,G,B,...)
  int ctabc; // Count in entries, 2..256
  int colorkey; // Index of transparent color, or <0 for none.
  
  int pixlenp; // Index in (dst) of the current block's length byte.
  int mincodesize;
  int clear_code;
  int eoi_code;
  int next_code;
  int code_limit;
  int codesize;
  uint8_t dstbuf;
  uint8_t dstmask;
  struct gif_dict_entry {
    int a,b;
  } dict[4096];
  
  uint8_t intake[256];
  int intakec;
  int intake_code;
};

static void gif_encoder_cleanup(struct gif_encoder *encoder) {
}

/* Search color table.
 */
 
static int gif_ctab_search(const uint8_t *ctab,int ctabc,uint8_t r,uint8_t g,uint8_t b) {
  int p=0;
  for (;p<ctabc;p++,ctab+=3) {
    if (ctab[0]!=r) continue;
    if (ctab[1]!=g) continue;
    if (ctab[2]!=b) continue;
    return p;
  }
  return -1;
}
 
static int gif_ctab_search_nearest(const uint8_t *ctab,int ctabc,int colorkey,uint8_t r,uint8_t g,uint8_t b) {
  int p=0,bestp=0,bestscore=999;
  for (;p<ctabc;p++,ctab+=3) {
    if (p==colorkey) continue;
    int dr=ctab[0]-r; if (dr<0) dr=-dr;
    int dg=ctab[1]-g; if (dg<0) dg=-dg;
    int db=ctab[2]-b; if (db<0) db=-db;
    int score=dr+dg+db;
    if (!score) return p;
    if (score<bestscore) {
      bestscore=score;
      bestp=p;
    }
  }
  return bestp;
}

/* Populate (ctab,ctabc,use_transparent) based on the incoming pixels.
 * We take the first 256 colors we see, no fuzzing.
 */
 
static int gif_encoder_analyze_colors(struct gif_encoder *encoder) {
  encoder->ctabc=0;
  encoder->colorkey=-1;
  const uint8_t *row=encoder->image->v;
  int yi=encoder->image->h;
  for (;yi-->0;row+=encoder->image->stride) {
    const uint8_t *p=row;
    int xi=encoder->image->w;
    for (;xi-->0;p+=4) {
      if (!p[3]) { // transparent
        if (encoder->colorkey<0) {
          if (encoder->ctabc>=256) { // oops, drop the last one
            encoder->colorkey=255;
          } else {
            encoder->colorkey=encoder->ctabc++;
          }
        }
      } else if (encoder->ctabc<256) {
        if (gif_ctab_search(encoder->ctab,encoder->ctabc,p[0],p[1],p[2])<0) {
          memcpy(encoder->ctab+encoder->ctabc*3,p,3);
          encoder->ctabc++;
        }
      }
    }
  }
  return 0;
}

/* Logical Screen Descriptor.
 * We do not emit a Global Color Table, even if it is possible.
 */
 
static int gif_encode_lsd(struct gif_encoder *ctx) {
  if (sr_encode_intle(ctx->dst,ctx->image->w,2)<0) return -1;
  if (sr_encode_intle(ctx->dst,ctx->image->h,2)<0) return -1;
  if (sr_encode_u8(ctx->dst,0)<0) return -1; // flags. all zero since there's no GCT
  if (sr_encode_u8(ctx->dst,0)<0) return -1; // bgcolor, not relevant
  if (sr_encode_u8(ctx->dst,0)<0) return -1; // aspect ratio. zero means "don't care"
  return 0;
}

/* Graphic Control Extension.
 * This is only required if the image has transparent pixels.
 * It's also highly relevant for animation, but we're not doing that.
 */
 
static int gif_encode_gce_if_applicable(struct gif_encoder *ctx) {
  if (ctx->colorkey<0) return 0; // Don't need it.
  if (sr_encode_raw(ctx->dst,"\x21\xf9\x04",3)<0) return -1; // Introducer and length.
  if (sr_encode_u8(ctx->dst,0x05)<0) return -1; // Flags: Disposal=Keep and Enable Transparent.
  if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1; // Delay time.
  if (sr_encode_u8(ctx->dst,ctx->colorkey)<0) return -1; // Transparent Color Index, this is the important part.
  if (sr_encode_u8(ctx->dst,0)<0) return -1; // End of chunk.
  return 1;
}

/* Push bits onto the output.
 */
 
static int gif_encode_lzw_word(struct gif_encoder *ctx,int v) {
  int i=ctx->codesize; while (i-->0) {
    if (v&1) ctx->dstbuf|=ctx->dstmask;
    if (ctx->dstmask==0x80) {
      if (sr_encode_u8(ctx->dst,ctx->dstbuf)<0) return -1;
      ctx->dstbuf=0;
      ctx->dstmask=1;
      int blocklen=ctx->dst->c-ctx->pixlenp-1;
      if (blocklen>=0xff) {
        ((uint8_t*)ctx->dst->v)[ctx->pixlenp]=blocklen;
        ctx->pixlenp=ctx->dst->c;
        if (sr_encode_u8(ctx->dst,0)<0) return -1;
      }
    } else {
      ctx->dstmask<<=1;
    }
    v>>=1;
  }
  return 0;
}

/* Push one more pixel onto the running LZW stream.
 */
 
static int gif_encode_lzw_pixel(struct gif_encoder *ctx,uint8_t v) {
  
  /* When intake is empty, append (v) to it and (v) is also the intake_code.
   */
  if (!ctx->intakec) {
    ctx->intake[0]=v;
    ctx->intakec=1;
    ctx->intake_code=v;
    return 0;
  }
    
  /* Look for a dictionary entry of (intake_code,v).
   * If it exists, append (v) to intake.
   * We can only do this when there's room in (intake). If it's full, we must emit.
   */
  if (ctx->intakec<sizeof(ctx->intake)) {
    int p=ctx->eoi_code+1;
    const struct gif_dict_entry *entry=ctx->dict+p;
    for (;p<ctx->next_code;p++,entry++) {
      if (entry->a!=ctx->intake_code) continue;
      if (entry->b!=v) continue;
      ctx->intake[ctx->intakec++]=v;
      ctx->intake_code=p;
      return 0;
    }
  }
    
  /* Emit what's cached for intake, drop that, add to dictionary, and start a fresh intake.
   */
  if (gif_encode_lzw_word(ctx,ctx->intake_code)<0) return -1;
  if (ctx->next_code<0xfff) {
    ctx->dict[ctx->next_code].a=ctx->intake_code;
    ctx->dict[ctx->next_code].b=v;
    ctx->next_code++;
    if (ctx->next_code>=ctx->code_limit+1) {
      ctx->code_limit<<=1;
      ctx->codesize++;
    }
  }
  ctx->intakec=1;
  ctx->intake[0]=v;
  ctx->intake_code=v;
  
  return 0;
}

/* Begin encoding LZW-compressed pixels.
 */
 
static int gif_encoder_begin_lzw(struct gif_encoder *ctx,int mincodesize) {
  if (sr_encode_u8(ctx->dst,mincodesize)<0) return -1;
  ctx->mincodesize=mincodesize;
  ctx->pixlenp=ctx->dst->c;
  if (sr_encode_u8(ctx->dst,0)<0) return -1; // Fill in block length here.
  ctx->clear_code=1<<mincodesize;
  ctx->eoi_code=ctx->clear_code+1;
  ctx->next_code=ctx->eoi_code+1;
  ctx->code_limit=1<<(mincodesize+1);
  ctx->codesize=mincodesize+1;
  ctx->dstbuf=0;
  ctx->dstmask=0x01;
  ctx->intakec=0;
  if (gif_encode_lzw_word(ctx,ctx->clear_code)<0) return -1;
  return 0;
}

/* End batch of pixels.
 */
 
static int gif_encoder_end_lzw(struct gif_encoder *ctx) {
  if (ctx->intakec) {
    if (gif_encode_lzw_word(ctx,ctx->intake_code)<0) return -1;
  }
  if (gif_encode_lzw_word(ctx,ctx->eoi_code)<0) return -1;
  if (ctx->dstmask!=1) {
    if (sr_encode_u8(ctx->dst,ctx->dstbuf)<0) return -1;
  }
  int blocklen=ctx->dst->c-ctx->pixlenp-1;
  ((uint8_t*)ctx->dst->v)[ctx->pixlenp]=blocklen;
  if (sr_encode_u8(ctx->dst,0)<0) return -1; // Terminate chain.
  return 0;
}

/* Image Descriptor, Local Color Table, and data.
 */
 
static int gif_encode_image(struct gif_encoder *ctx) {

  int mincodesize;
  uint8_t flags=0x80; // LCT present.
       if (ctx->ctabc>=0x80) { flags|=0x07; mincodesize=8; }
  else if (ctx->ctabc>=0x40) { flags|=0x06; mincodesize=7; }
  else if (ctx->ctabc>=0x20) { flags|=0x05; mincodesize=6; }
  else if (ctx->ctabc>=0x10) { flags|=0x04; mincodesize=5; }
  else if (ctx->ctabc>=0x08) { flags|=0x03; mincodesize=4; }
  else                       { flags|=0x02; mincodesize=3; }
  
  if (sr_encode_raw(ctx->dst,"\x2c\0\0\0\0",5)<0) return -1; // Image Separator, and (x,y)=(0,0)
  if (sr_encode_intle(ctx->dst,ctx->image->w,2)<0) return -1;
  if (sr_encode_intle(ctx->dst,ctx->image->h,2)<0) return -1;
  if (sr_encode_u8(ctx->dst,flags)<0) return -1;
  
  // NB It's not (ctx->ctabc) for the length; we have to round up to a power of two.
  if (sr_encode_raw(ctx->dst,ctx->ctab,(1<<mincodesize)*3)<0) return -1;
  
  if (gif_encoder_begin_lzw(ctx,mincodesize)<0) return -1;
  
  const uint8_t *srcrow=ctx->image->v;
  int yi=ctx->image->h;
  for (;yi-->0;srcrow+=ctx->image->stride) {
    const uint8_t *srcp=srcrow;
    int xi=ctx->image->w;
    for (;xi-->0;srcp+=4) {
      int p;
      if (srcp[3]) {
        p=gif_ctab_search_nearest(ctx->ctab,ctx->ctabc,ctx->colorkey,srcp[0],srcp[1],srcp[2]);
      } else {
        p=ctx->colorkey;
      }
      if (gif_encode_lzw_pixel(ctx,p)<0) return -1;
    }
  }
  if (gif_encoder_end_lzw(ctx)<0) return -1;
  
  return 0;
}

/* Encode in context.
 */
 
static int gif_encode_inner(struct gif_encoder *ctx) {
  if (gif_encoder_analyze_colors(ctx)<0) return -1;
  if (sr_encode_raw(ctx->dst,"GIF89a",6)<0) return -1;
  if (gif_encode_lsd(ctx)<0) return -1;
  if (gif_encode_gce_if_applicable(ctx)<0) return -1;
  if (gif_encode_image(ctx)<0) return -1;
  if (sr_encode_u8(ctx->dst,0x3b)<0) return -1;
  return 0;
}

/* Encode.
 */

int gif_encode(struct sr_encoder *dst,const struct gif_image *image) {
  if (!dst||!image) return -1;
  if ((image->w<1)||(image->w>0x7fff)) return -1;
  if ((image->h<1)||(image->h>0x7fff)) return -1;
  struct gif_encoder ctx={.dst=dst,.image=image};
  int err=gif_encode_inner(&ctx);
  gif_encoder_cleanup(&ctx);
  return err;
}
