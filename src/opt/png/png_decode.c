#include "png.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <zlib.h>

/* Decoder context.
 */
 
struct png_decoder {
  struct png_image *image;
  z_stream *z; // Initialized if not null
  int rowbufc; // 1+stride
  uint8_t *rowbuf;
  int xstride; // bytes column to column for filter purposes, min 1
  int y; // Next output row.
  uint8_t *dstrow,*prvrow; // points into image->v
};

static void png_decoder_cleanup(struct png_decoder *ctx) {
  png_image_del(ctx->image);
  if (ctx->z) {
    inflateEnd(ctx->z);
    free(ctx->z);
  }
  if (ctx->rowbuf) free(ctx->rowbuf);
}

/* Undo filter, into the real output.
 * (dst,src) are always required and (prv) is always optional.
 */
 
static void png_unfilter_NONE(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  memcpy(dst,src,c);
}
 
static void png_unfilter_SUB(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  memcpy(dst,src,xstride);
  const uint8_t *tail=dst;
  dst+=xstride;
  src+=xstride;
  c-=xstride;
  for (;c-->0;tail++,dst++,src++) {
    (*dst)=(*src)+(*tail);
  }
}
 
static void png_unfilter_UP(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  if (prv) {
    for (;c-->0;dst++,src++,prv++) {
      (*dst)=(*src)+(*prv);
    }
  } else {
    memcpy(dst,src,c);
  }
}
 
static void png_unfilter_AVG(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  if (prv) {
    const uint8_t *tail=dst;
    int i=0;
    for (;i<xstride;i++,src++,dst++,prv++) {
      (*dst)=(*src)+((*prv)>>1);
    }
    for (;i<c;i++,src++,dst++,prv++,tail++) {
      (*dst)=(*src)+(((*prv)+(*tail))>>1);
    }
  } else {
    memcpy(dst,src,xstride);
    int i=xstride; for (;i<c;i++) {
      dst[i]=src[i]+(dst[i-xstride]>>1);
    }
  }
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
 
static void png_unfilter_PAETH(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  if (prv) {
    const uint8_t *tail=dst;
    const uint8_t *prvtail=prv;
    int i=0;
    for (;i<xstride;i++,src++,dst++,prv++) {
      (*dst)=(*src)+(*prv);
    }
    for (;i<c;i++,src++,dst++,prv++,tail++,prvtail++) {
      (*dst)=(*src)+png_paeth(*tail,*prv,*prvtail);
    }
  } else {
    const uint8_t *tail=dst;
    int i=0;
    for (;i<xstride;i++,dst++,src++) {
      (*dst)=(*src);
    }
    for (;i<c;i++,dst++,src++,tail++) {
      (*dst)=(*src)+(*tail);
    }
  }
}

/* If zlib has finished a row, unfilter it and add to the image.
 */
 
static int png_decode_row_if_ready(struct png_decoder *ctx) {
  if (ctx->z->avail_out) return 0; // Not ready.
  ctx->z->next_out=(Bytef*)ctx->rowbuf;
  ctx->z->avail_out=ctx->rowbufc;
  if (ctx->y>=ctx->image->h) return 0; // Discard extra trailing data.
  switch (ctx->rowbuf[0]) {
    case 0: png_unfilter_NONE(ctx->dstrow,ctx->rowbuf+1,ctx->prvrow,ctx->image->stride,ctx->xstride); break;
    case 1: png_unfilter_SUB(ctx->dstrow,ctx->rowbuf+1,ctx->prvrow,ctx->image->stride,ctx->xstride); break;
    case 2: png_unfilter_UP(ctx->dstrow,ctx->rowbuf+1,ctx->prvrow,ctx->image->stride,ctx->xstride); break;
    case 3: png_unfilter_AVG(ctx->dstrow,ctx->rowbuf+1,ctx->prvrow,ctx->image->stride,ctx->xstride); break;
    case 4: png_unfilter_PAETH(ctx->dstrow,ctx->rowbuf+1,ctx->prvrow,ctx->image->stride,ctx->xstride); break;
    default: {
        fprintf(stderr,"%s: Unexpected filter byte 0x%02x at row %d/%d\n",__func__,ctx->rowbuf[0],ctx->y,ctx->image->h);
        return -1;
      }
  }
  ctx->y++;
  ctx->prvrow=ctx->dstrow;
  ctx->dstrow+=ctx->image->stride;
  return 0;
}

/* IDAT.
 */
 
static int png_decode_IDAT(struct png_decoder *ctx,const uint8_t *src,int srcc) {
  if (!ctx->image) return -1; // IDAT before IHDR
  ctx->z->next_in=(Bytef*)src;
  ctx->z->avail_in=srcc;
  while ((ctx->z->avail_in)&&(ctx->y<ctx->image->h)) {
    int err=inflate(ctx->z,Z_NO_FLUSH);
    if (err<0) {
      fprintf(stderr,"%s:inflate:%d\n",__func__,err);
      return -1;
    }
    if (png_decode_row_if_ready(ctx)<0) return -1;
  }
  return 0;
}

/* Flush output.
 */
 
static int png_decoder_flush(struct png_decoder *ctx) {
  while (ctx->y<ctx->image->h) {
    int err=inflate(ctx->z,Z_FINISH);
    if (err<0) {
      fprintf(stderr,"%s:inflate(Z_FINISH):%d\n",__func__,err);
      return -1;
    }
    if (png_decode_row_if_ready(ctx)<0) return -1;
    if (err==Z_STREAM_END) break;
  }
  return 0;
}

/* IHDR.
 * We initialize the image and zlib context here.
 */
 
static int png_decode_IHDR(struct png_decoder *ctx,const uint8_t *src,int srcc) {
  if (ctx->image) return -1; // Multiple IHDR

  if (srcc<13) return -1;
  int w=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3];
  int h=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  int depth=src[8];
  int colortype=src[9];
  int compression=src[10];
  int filter=src[11];
  int interlace=src[12];
  if (compression||filter) return -1; // Only zero defined by spec.
  if (interlace) return -1; // Spec requires 1=adam7, but we're not doing that.
  
  // Let our image ctor validate (w,h,depth,colortype).
  if (!(ctx->image=png_image_new(w,h,depth,colortype))) return -1;
  
  ctx->xstride=(ctx->image->pixelsize+7)>>3;
  ctx->rowbufc=1+ctx->image->stride;
  if (!(ctx->rowbuf=malloc(ctx->rowbufc))) return -1;
  
  if (!(ctx->z=calloc(1,sizeof(z_stream)))) return -1;
  if (inflateInit(ctx->z)<0) return -1;
  ctx->z->next_out=(Bytef*)ctx->rowbuf;
  ctx->z->avail_out=ctx->rowbufc;
  ctx->y=0;
  ctx->dstrow=ctx->image->v;
  ctx->prvrow=0;
  
  return 0;
}

/* Receive unknown chunk.
 */
 
static int png_decode_other(struct png_decoder *ctx,const char *type,const void *v,int c) {
  // Could check the ancillary bit here and fail if critical, but I'm not worried about that.
  // No image yet, means we haven't seen IHDR. That's an error per spec. But I'll play it looser and just ignore such chunks.
  if (!ctx->image) return 0;
  if (!png_image_add_chunk(ctx->image,type,v,c)) return -1;
  return 0;
}

/* Decode in context.
 */
 
static int png_decode_inner(struct png_decoder *ctx,const uint8_t *src,int srcc) {
  if ((srcc<8)||!src||memcmp(src,"\x89PNG\r\n\x1a\n",8)) return -1;
  int srcp=8;
  while (srcp<srcc) {
    if (srcp>srcc-8) return -1;
    int chunklen=(src[srcp]<<24)|(src[srcp+1]<<16)|(src[srcp+2]<<8)|src[srcp+3];
    const char *chunktypestr=(char*)src+srcp+4;
    srcp+=8;
    if ((chunklen<0)||(srcp>srcc-chunklen)) return -1;
    const void *chunk=src+srcp;
    srcp+=chunklen;
    if (srcp>srcc-4) return -1;
    srcp+=4; // skip CRC, don't bother checking.
    
    if (!memcmp(chunktypestr,"IEND",4)) {
      break;
    } else if (!memcmp(chunktypestr,"IHDR",4)) {
      if (png_decode_IHDR(ctx,chunk,chunklen)<0) return -1;
    } else if (!memcmp(chunktypestr,"IDAT",4)) {
      if (png_decode_IDAT(ctx,chunk,chunklen)<0) return -1;
    } else {
      if (png_decode_other(ctx,chunktypestr,chunk,chunklen)<0) return -1;
    }
  }
  if (!ctx->image) return -1; // No IHDR.
  if (png_decoder_flush(ctx)<0) return -1;
  // Could validate here that all the image data was received, but whatever.
  return 0;
}

/* Decode.
 */

struct png_image *png_decode(const void *src,int srcc) {
  struct png_decoder ctx={0};
  int err=png_decode_inner(&ctx,src,srcc);
  if (err>=0) {
    struct png_image *image=ctx.image;
    ctx.image=0;
    png_decoder_cleanup(&ctx);
    return image;
  }
  png_decoder_cleanup(&ctx);
  return 0;
}
