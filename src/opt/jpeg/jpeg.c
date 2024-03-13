#include "jpeg.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <jpeglib.h>

/* Cleanup.
 */

void jpeg_image_cleanup(struct jpeg_image *image) {
  if (image->v) free(image->v);
}

void jpeg_image_del(struct jpeg_image *image) {
  if (!image) return;
  if (image->v) free(image->v);
  free(image);
}

/* Extra bit for longjmp'ing out of errors that libjpeg would "helpfully" kill the program over.
 */

struct jpeg_errmgr_with_jump {
  struct jpeg_error_mgr errmgr;
  jmp_buf error_jump_buf;
};

static void jpeg_error_exit(j_common_ptr libjpeg) {
  struct jpeg_errmgr_with_jump *myerrmgr=(void*)libjpeg->err;
  longjmp(myerrmgr->error_jump_buf,1);
}

/* Encoder context.
 */
 
struct jpeg_encoder {
  struct jpeg_compress_struct libjpeg;
  struct jpeg_errmgr_with_jump err;
  const struct jpeg_image *image;
  struct sr_encoder *dst;
  unsigned char *ljdst;
  unsigned long ljdstc;
  JSAMPLE *rowbuf;
};

static void jpeg_encoder_cleanup(struct jpeg_encoder *ctx) {
  jpeg_destroy_compress(&ctx->libjpeg);
  if (ctx->ljdst) free(ctx->ljdst);
  if (ctx->rowbuf) free(ctx->rowbuf);
}

/* Encode in context.
 */
 
static int jpeg_encode_inner(struct jpeg_encoder *ctx) {
  ctx->libjpeg.err=jpeg_std_error(&ctx->err.errmgr);
  if (setjmp(ctx->err.error_jump_buf)) return -1;
  ctx->err.errmgr.error_exit=jpeg_error_exit;
  jpeg_create_compress(&ctx->libjpeg);
  jpeg_mem_dest(&ctx->libjpeg,&ctx->ljdst,&ctx->ljdstc);
  
  ctx->libjpeg.image_width=ctx->image->w;
  ctx->libjpeg.image_height=ctx->image->h;
  ctx->libjpeg.input_components=ctx->image->chanc;
  switch (ctx->image->chanc) {
    case 1: ctx->libjpeg.in_color_space=JCS_GRAYSCALE; break;
    case 3: ctx->libjpeg.in_color_space=JCS_RGB; break;
    default: return -1;
  }
  
  jpeg_set_defaults(&ctx->libjpeg);
  jpeg_start_compress(&ctx->libjpeg,1);
  #if MAXJSAMPLE==255
    uint8_t *srcrow=ctx->image->v;
    int yi=ctx->image->h;
    for (;yi-->0;srcrow+=ctx->image->stride) {
      if (jpeg_write_scanlines(&ctx->libjpeg,&srcrow,1)!=1) return -1;
    }
  #else
    if (!(ctx->rowbuf=malloc(sizeof(JSAMPLE)*ctx->image->w*ctx->image->chanc))) return -1;
    const uint8_t *srcrow=ctx->image->v;
    int yi=ctx->image->h;
    for (;yi-->0;srcrow+=ctx->image->stride) {
      const uint8_t *srcp=srcrow;
      JSAMPLE *dstp=ctx->rowbuf;
      int xi=ctx->image->w*ctx->image->chanc;
      for (;xi-->0;dstp++,srcp++) {
        *dstp=((*srcp)*MAXJSAMPLE)/255;
      }
      if (jpeg_write_scanlines(&ctx->libjpeg,&ctx->rowbuf,1)!=1) return -1;
    }
  #endif
  jpeg_finish_compress(&ctx->libjpeg);
  
  // If we had an empty encoder to begin with, very likely, swap out for libjpeg's buffer.
  // Otherwise copy it.
  if (ctx->dst->c) {
    if (sr_encode_raw(ctx->dst,ctx->ljdst,ctx->ljdstc)<0) return -1;
  } else {
    if (ctx->dst->v) free(ctx->dst->v);
    ctx->dst->v=ctx->ljdst;
    ctx->dst->c=ctx->ljdstc;
    ctx->dst->a=ctx->ljdstc;
    ctx->ljdst=0;
  }
  return 0;
}

/* Encode.
 */

int jpeg_encode(struct sr_encoder *dst,const struct jpeg_image *image) {
  struct jpeg_encoder ctx={
    .dst=dst,
    .image=image,
  };
  int err=jpeg_encode_inner(&ctx);
  jpeg_encoder_cleanup(&ctx);
  return err;
}

/* Decoder context.
 */
 
struct jpeg_decoder {
  struct jpeg_decompress_struct libjpeg;
  struct jpeg_errmgr_with_jump err;
  struct jpeg_image *image; // yoink after success
  JSAMPLE *rowbuf;
};

static void jpeg_decoder_cleanup(struct jpeg_decoder *ctx) {
  jpeg_destroy_decompress(&ctx->libjpeg);
  jpeg_image_del(ctx->image);
  if (ctx->rowbuf) free(ctx->rowbuf);
}

/* Decode in context.
 */
 
static int jpeg_decode_inner(struct jpeg_decoder *ctx,const void *src,int srcc) {
  ctx->libjpeg.err=jpeg_std_error(&ctx->err.errmgr);
  if (setjmp(ctx->err.error_jump_buf)) return -1;
  ctx->err.errmgr.error_exit=jpeg_error_exit;
  jpeg_create_decompress(&ctx->libjpeg);
  jpeg_mem_src(&ctx->libjpeg,src,srcc);
  if (jpeg_read_header(&ctx->libjpeg,1)!=JPEG_HEADER_OK) return -1;
  if (!jpeg_start_decompress(&ctx->libjpeg)) return -1;
  if ((ctx->libjpeg.image_width<1)||(ctx->libjpeg.image_width>0x7fff)) return -1;
  if ((ctx->libjpeg.image_height<1)||(ctx->libjpeg.image_height>0x7fff)) return -1;
  if ((ctx->libjpeg.output_components<1)||(ctx->libjpeg.output_components>4)) return -1;
  if (!(ctx->image=calloc(1,sizeof(struct jpeg_image)))) return -1;
  ctx->image->w=ctx->libjpeg.image_width;
  ctx->image->h=ctx->libjpeg.image_height;
  ctx->image->chanc=ctx->libjpeg.output_components;
  ctx->image->stride=ctx->libjpeg.image_width*ctx->image->chanc;
  if (!(ctx->image->v=malloc(ctx->image->stride*ctx->image->h))) return -1;
  if (!(ctx->rowbuf=malloc(sizeof(JSAMPLE)*ctx->image->chanc*ctx->image->w))) return -1;
  
  int y=0;
  uint8_t *dstrow=ctx->image->v;
  for (;y<ctx->image->h;y++,dstrow+=ctx->image->stride) {
    int err=jpeg_read_scanlines(&ctx->libjpeg,&ctx->rowbuf,1);
    if (err!=1) return -1;
    #if MAXJSAMPLE==255
      memcpy(dstrow,ctx->rowbuf,ctx->image->stride);
    #else
      uint8_t *dstp=dstrow;
      JSAMPLE *srcp=ctx->rowbuf;
      int xi=ctx->image->w;
      for (;xi-->0;dstp++,srcp++) {
        *dstp=((*srcp)*0xff)/MAXJSAMPLE;
      }
    #endif
  }
  
  if (!jpeg_finish_decompress(&ctx->libjpeg)) return -1;
  return 0;
}

/* Decode.
 */

struct jpeg_image *jpeg_decode(const void *src,int srcc) {
  struct jpeg_decoder ctx={0};
  int err=jpeg_decode_inner(&ctx,src,srcc);
  struct jpeg_image *result=0;
  if (err>=0) {
    result=ctx.image;
    ctx.image=0;
  }
  jpeg_decoder_cleanup(&ctx);
  return result;
}
