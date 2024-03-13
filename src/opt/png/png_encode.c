#include "png.h"
#include "opt/serial/serial.h"
#include <zlib.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

/* Encoder context.
 */
 
struct png_encoder {
  struct sr_encoder *dst; // WEAK
  const struct png_image *image; // WEAK
  uint8_t *rowbuf;
  int rowbufc; // including filter byte, ie 1+stride
  int xstride; // bytes column to column for filter purposes (min 1)
  int stride; // May be less than input.
  z_stream *z; // Initialized if not null.
};

static void png_encoder_cleanup(struct png_encoder *ctx) {
  if (ctx->rowbuf) free(ctx->rowbuf);
  if (ctx->z) {
    deflateEnd(ctx->z);
    free(ctx->z);
  }
}

/* Add one chunk to encoder.
 */
 
static int png_encode_chunk(struct png_encoder *ctx,const char chunktype[4],const void *v,int c) {
  if (sr_encode_intbe(ctx->dst,c,4)<0) return -1;
  if (sr_encode_raw(ctx->dst,chunktype,4)<0) return -1;
  if (sr_encode_raw(ctx->dst,v,c)<0) return -1;
  uint32_t crc=crc32(crc32(0,0,0),(Bytef*)chunktype,4);
  if (c) crc=crc32(crc,v,c); // evidently crc32() doesn't like empty input
  if (sr_encode_intbe(ctx->dst,crc,4)<0) return -1;
  return 0;
}

/* Encode IHDR.
 */
 
static int png_encode_IHDR(struct png_encoder *ctx) {
  uint8_t tmp[13]={
    ctx->image->w>>24,
    ctx->image->w>>16,
    ctx->image->w>>8,
    ctx->image->w,
    ctx->image->h>>24,
    ctx->image->h>>16,
    ctx->image->h>>8,
    ctx->image->h,
    ctx->image->depth,
    ctx->image->colortype,
    0,0,0, // Compression, Filter, Interlace
  };
  return png_encode_chunk(ctx,"IHDR",tmp,sizeof(tmp));
}

/* Encode loose chunks.
 */
 
static int png_encode_chunks(struct png_encoder *ctx) {
  const struct png_chunk *chunk=ctx->image->chunkv;
  int i=ctx->image->chunkc;
  for (;i-->0;chunk++) {
    if (png_encode_chunk(ctx,chunk->chunktype,chunk->v,chunk->c)<0) return -1;
  }
  return 0;
}

/* Filters.
 * NONE and SUB accept null (prv), the others require it.
 * (they do declare a prv parameter, just for consistency).
 * Each returns the count of zeroes after filtering.
 */
 
static inline int png_count_zeroes(const uint8_t *v,int c) {
  int zeroc=0;
  for (;c-->0;v++) if (!*v) zeroc++;
  return zeroc;
}
 
static int png_filter_row_NONE(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  memcpy(dst,src,c);
  return png_count_zeroes(dst,c);
}
 
static int png_filter_row_SUB(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  const uint8_t *dst0=dst;
  int c0=c;
  memcpy(dst,src,xstride);
  c-=xstride;
  dst+=xstride;
  const uint8_t *tail=src;
  src+=xstride;
  for (;c-->0;dst++,src++,tail++) {
    *dst=(*src)-(*tail);
  }
  return png_count_zeroes(dst0,c0);
}

static int png_filter_row_UP(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  const uint8_t *dst0=dst;
  int c0=c;
  for (;c-->0;dst++,src++,prv++) {
    *dst=(*src)-(*prv);
  }
  return png_count_zeroes(dst0,c0);
}

static int png_filter_row_AVG(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  const uint8_t *dst0=dst;
  int c0=c;
  const uint8_t *tail=src;
  int i=0; for (;i<xstride;i++,dst++,src++,prv++) {
    *dst=(*src)-((*prv)>>1);
  }
  c-=xstride;
  for (;c-->0;dst++,src++,prv++,tail++) {
    *dst=(*src)-(((*tail)+(*prv))>>1);
  }
  return png_count_zeroes(dst0,c0);
}

static inline uint8_t png_paeth(uint8_t a,uint8_t b,uint8_t c) {
  int p=a+b-c;
  int pa=p-a; if (pa<0) pa=-pa;
  int pb=p-b; if (pb<0) pb=-pb;
  int pc=p-c; if (pc<0) pc=-pc;
  if ((pa<=pb)&&(pa<=pc)) return a;
  if (pb<=pc) return b;
  return c;
}

static int png_filter_row_PAETH(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  const uint8_t *dst0=dst;
  int c0=c;
  const uint8_t *srctail=src,*prvtail=prv;
  int i=0; for (;i<xstride;i++,dst++,src++,prv++) {
    *dst=(*src)-(*prv);
  }
  c-=xstride;
  for (;c-->0;dst++,src++,prv++,srctail++,prvtail++) {
    *dst=(*src)-png_paeth(*srctail,*prv,*prvtail);
  }
  return png_count_zeroes(dst0,c0);
}

/* Apply filter to one row.
 * Return the chosen filter byte and overwrite (dst) completely.
 */
 
static uint8_t png_filter_row(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  if (!prv) {
    // All filters are legal for the first row, but only NONE and SUB make sense.
    // (PAETH turns into SUB, UP turns into NONE, and AVG I'm not sure).
    int nonescore=png_filter_row_NONE(dst,src,0,c,xstride);
    int subscore=png_filter_row_SUB(dst,src,0,c,xstride);
    if (subscore>=nonescore) { // SUB wins ties, since it's the last one and already present in buffer.
      return 1;
    }
    memcpy(dst,src,c);
    return 0;
  }
  // Normal case: Try all five filters, and keep whichever produces the most zeroes.
  int nonescore=png_filter_row_NONE(dst,src,prv,c,xstride);
  int subscore=png_filter_row_SUB(dst,src,prv,c,xstride);
  int upscore=png_filter_row_UP(dst,src,prv,c,xstride);
  int avgscore=png_filter_row_AVG(dst,src,prv,c,xstride);
  int paethscore=png_filter_row_PAETH(dst,src,prv,c,xstride);
  // Ties break in order of convenience: PAETH, NONE, SUB, UP, AVG
  if ((paethscore>=avgscore)&&(paethscore>=upscore)&&(paethscore>=subscore)&&(paethscore>=nonescore)) {
    return 4;
  }
  if ((nonescore>=avgscore)&&(nonescore>=upscore)&&(nonescore>=subscore)) {
    memcpy(dst,src,c);
    return 0;
  }
  if ((subscore>=avgscore)&&(subscore>=upscore)) {
    png_filter_row_SUB(dst,src,prv,c,xstride);
    return 1;
  }
  if (upscore>=avgscore) {
    png_filter_row_UP(dst,src,prv,c,xstride);
    return 2;
  }
  png_filter_row_AVG(dst,src,prv,c,xstride);
  return 3;
}

/* Allocate more space in master output if needed, and point zlib to it.
 */
 
static int png_encoder_require_zlib_output(struct png_encoder *ctx) {
  if (sr_encoder_require(ctx->dst,8192)<0) return -1;
  ctx->z->next_out=(Bytef*)ctx->dst->v+ctx->dst->c;
  ctx->z->avail_out=ctx->dst->a-ctx->dst->c;
  return 0;
}

/* Feed (rowbuf) into zlib, and advance output as needed.
 */
 
static int png_encode_row(struct png_encoder *ctx) {
  ctx->z->next_in=(Bytef*)ctx->rowbuf;
  ctx->z->avail_in=ctx->rowbufc;
  while (ctx->z->avail_in) {
    if (png_encoder_require_zlib_output(ctx)<0) return -1;
    int out0=ctx->z->total_out;
    int err=deflate(ctx->z,Z_NO_FLUSH);
    if (err<0) {
      fprintf(stderr,"%s: deflate: %d\n",__func__,err);
      return -1;
    }
    int outadd=ctx->z->total_out-out0;
    ctx->dst->c+=outadd;
  }
  return 0;
}

/* Flush zlib out.
 */
 
static int png_encode_end_of_image(struct png_encoder *ctx) {
  while (1) {
    if (png_encoder_require_zlib_output(ctx)<0) return -1;
    int out0=ctx->z->total_out;
    int err=deflate(ctx->z,Z_FINISH);
    if (err<0) {
      fprintf(stderr,"%s: deflate(Z_FINISH): %d\n",__func__,err);
      return -1;
    }
    int outadd=ctx->z->total_out-out0;
    ctx->dst->c+=outadd;
    if (err=Z_STREAM_END) break;
  }
  return 0;
}

/* Encode IDAT, the main event.
 * We're not going to use png_encode_chunk. We'll build up the chunk incrementally, then calculate CRC on our own.
 */
 
static int png_encode_IDAT(struct png_encoder *ctx) {
  int lenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0\0\0IDAT",8)<0) return -1;
  
  ctx->rowbufc=1+ctx->stride;
  if (!(ctx->rowbuf=malloc(ctx->rowbufc))) return -1;
  ctx->xstride=(ctx->image->pixelsize+7)>>3;
  
  if (!(ctx->z=calloc(1,sizeof(z_stream)))) return -1;
  if (deflateInit(ctx->z,Z_BEST_COMPRESSION)<0) {
    free(ctx->z);
    ctx->z=0;
    return -1;
  }
  
  const uint8_t *pvrow=0;
  const uint8_t *srcrow=ctx->image->v;
  int y=0;
  for (;y<ctx->image->h;y++,srcrow+=ctx->image->stride) {
    ctx->rowbuf[0]=png_filter_row(ctx->rowbuf+1,srcrow,pvrow,ctx->stride,ctx->xstride);
    if (png_encode_row(ctx)<0) return -1;
    pvrow=srcrow;
  }
  if (png_encode_end_of_image(ctx)<0) return -1;
  
  int len=ctx->dst->c-lenp-8;
  int crc=crc32(crc32(0,0,0),((Bytef*)ctx->dst->v)+lenp+4,ctx->dst->c-lenp-4);
  if (sr_encode_intbe(ctx->dst,crc,4)<0) return -1;
  uint8_t *lenv=(uint8_t*)ctx->dst->v+lenp;
  lenv[0]=len>>24;
  lenv[1]=len>>16;
  lenv[2]=len>>8;
  lenv[3]=len;
  return 0;
}

/* Encode in context.
 */
 
static int png_encode_inner(struct png_encoder *ctx) {

  if ((ctx->stride=png_minimum_stride(ctx->image->w,ctx->image->pixelsize))<1) return -1;
  if (ctx->stride>ctx->image->stride) return -1;

  if (sr_encode_raw(ctx->dst,"\x89PNG\r\n\x1a\n",8)<0) return -1;
  if (png_encode_IHDR(ctx)<0) return -1;
  if (png_encode_chunks(ctx)<0) return -1;
  if (png_encode_IDAT(ctx)<0) return -1;
  if (png_encode_chunk(ctx,"IEND",0,0)<0) return -1;
  return 0;
}

/* Encode.
 */

int png_encode(struct sr_encoder *dst,const struct png_image *image) {
  if (!dst||!image) return -1;
  struct png_encoder ctx={
    .dst=dst,
    .image=image,
  };
  int err=png_encode_inner(&ctx);
  png_encoder_cleanup(&ctx);
  return err;
}
