#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include "rlead.h"
#include "opt/serial/serial.h"

/* Image object.
 */
 
void rlead_image_cleanup(struct rlead_image *image) {
  if (!image) return;
  if (image->v) free(image->v);
}

void rlead_image_del(struct rlead_image *image) {
  if (!image) return;
  rlead_image_cleanup(image);
  free(image);
}

/* Encoder context.
 */
 
struct rlead_encoder {
  struct sr_encoder *dst; // WEAK
  uint8_t *filtered;
  int w,h,stride,pxc;
  uint8_t dstbuf,dstmask;
  int using_filter;
};

static void rlead_encoder_cleanup(struct rlead_encoder *encoder) {
  if (encoder->filtered) free(encoder->filtered);
}

/* Emit one value bitwise.
 */
 
static int rlead_emit_bits(struct rlead_encoder *encoder,int v,int startmask) {
  for (;startmask;startmask>>=1) {
    if (v&startmask) encoder->dstbuf|=encoder->dstmask;
    if (encoder->dstmask==1) {
      if (sr_encode_u8(encoder->dst,encoder->dstbuf)<0) return -1;
      encoder->dstbuf=0;
      encoder->dstmask=0x80;
    } else {
      encoder->dstmask>>=1;
    }
  }
  return 0;
}

/* Emit one run length.
 */
 
static int rlead_emit_run(struct rlead_encoder *encoder,int runlen) {
   
  if (runlen<1) {
    fprintf(stderr,"%s: Invalid run length %d\n",__func__,runlen);
    return -1;
  }
  runlen--;
   
  int range=2;
  for (;;) {
    int limit=range-1;
    if (runlen<limit) { // end of sequence, after we emit this. sic '<' not '<='
      if (rlead_emit_bits(encoder,runlen,range>>1)<0) return -1;
      break;
    }
    if (rlead_emit_bits(encoder,limit,range>>1)<0) return -1;
    runlen-=limit;
    if (range>=0x40000000) return -1;
    range<<=1;
  }
  
  return 0;
}

/* Encode to rlead, having acquired the filtered image.
 */
 
static int rlead_encode_inner(struct rlead_encoder *encoder) {

  uint8_t flags=0;
  if (encoder->filtered[0]&0x80) flags|=0x01; // initial color
  if (encoder->using_filter) flags|=0x02;

  // 7-byte header: 0xbb,0xad,u16 w,u16 h,u8 flags(1=color,2=filter)
  if (sr_encode_raw(encoder->dst,"\xbb\xad",2)<0) return -1;
  if (sr_encode_intbe(encoder->dst,encoder->w,2)<0) return -1;
  if (sr_encode_intbe(encoder->dst,encoder->h,2)<0) return -1;
  if (sr_encode_u8(encoder->dst,flags)<0) return -1;
  
  // Read pixels.
  encoder->dstbuf=0;
  encoder->dstmask=0x80;
  int runcolor=(encoder->filtered[0]&0x80)?1:0;
  int runlen=0;
  const uint8_t *srcrow=encoder->filtered;
  int yi=encoder->h;
  for (;yi-->0;srcrow+=encoder->stride) {
    const uint8_t *srcp=srcrow;
    uint8_t srcmask=0x80;
    int xi=encoder->w;
    for (;xi-->0;) {
      int pixel=((*srcp)&srcmask)?1:0;
      if (srcmask==1) { srcmask=0x80; srcp++; }
      else srcmask>>=1;
      if (pixel!=runcolor) {
        if (rlead_emit_run(encoder,runlen)<0) return -1;
        runcolor=pixel;
        runlen=1;
      } else {
        runlen++;
      }
    }
  }
  if (runlen) {
    if (rlead_emit_run(encoder,runlen)<0) return -1;
  }
  if (encoder->dstmask!=0x80) {
    if (sr_encode_u8(encoder->dst,encoder->dstbuf)<0) return -1;
  }
  
  return 0;
}

/* Count the 0 and 255 bytes in a buffer.
 * ie a quick-ish way of estimating how run-length encodable this will be.
 */
 
static int rlead_count_flushes(const uint8_t *v,int c) {
  int flushc=0;
  for (;c-->0;v++) if ((*v==0x00)||(*v==0xff)) flushc++;
  return flushc;
}

/* Copy and filter image into encoder.
 */
 
static int rlead_encoder_acquire_image(struct rlead_encoder *encoder,const uint8_t *src,int w,int h,int stride) {
  if ((w<1)||(w>0x7fff)) return -1;
  if ((h<1)||(h>0x7fff)) return -1;
  if (stride<(w+7)>>3) return -1;
  encoder->stride=stride;
  encoder->pxc=encoder->stride*h;
  if (!(encoder->filtered=malloc(encoder->pxc))) return -1;
  memcpy(encoder->filtered,src,encoder->pxc);
  encoder->w=w;
  encoder->h=h;
  
  int flushc_before=rlead_count_flushes(encoder->filtered,encoder->pxc);
  uint8_t *dstp=encoder->filtered+encoder->pxc;
  uint8_t *srcp=dstp-encoder->stride;
  int i=encoder->stride*(h-1);
  while (i-->0) {
    dstp--;
    srcp--;
    (*dstp)^=(*srcp);
  }
  int flushc_after=rlead_count_flushes(encoder->filtered,encoder->pxc);
  
  if (flushc_before>flushc_after) {
    srcp=encoder->filtered;
    dstp=srcp+encoder->stride;
    i=encoder->stride*(h-1);
    while (i-->0) {
      (*dstp)^=(*srcp);
      dstp++;
      srcp++;
    }
  } else {
    encoder->using_filter=1;
  }
  
  return 0;
}

/* Encode.
 */
 
int rlead_encode(struct sr_encoder *dst,const struct rlead_image *image) {
  if (!dst||!image) return -1;
  struct rlead_encoder encoder={.dst=dst};
  if (
    (rlead_encoder_acquire_image(&encoder,image->v,image->w,image->h,image->stride)<0)||
    (rlead_encode_inner(&encoder)<0)
  ) {
    rlead_encoder_cleanup(&encoder);
    return -1;
  }
  rlead_encoder_cleanup(&encoder);
  return 0;
}

/* Decode into image.
 * Caller strips the 7-byte header.
 */
 
static int rlead_decode_inner(struct rlead_image *image,const uint8_t *SRC,int srcc,uint8_t flags) {
  int srcp=0;
  uint8_t srcmask=0x80;
  int dstx=0,dsty=0; // TODO use an iterator on (dst). Keeping it simple for now, at some compute cost.
  uint8_t *dst=image->v;
  int color=flags&1;
  
  while (dsty<image->h) {
  
    /* Read the next run length.
     */
    int runlen=0;
    int wordsize=1;
    while (1) {
      int word=0;
      int wordmask=1<<(wordsize-1);
      while (wordmask) {
        if (srcp>=srcc) return -1;
        int srcbit=SRC[srcp]&srcmask;
        if (srcmask==1) { srcp++; srcmask=0x80; }
        else srcmask>>=1;
        if (srcbit) word|=wordmask;
        wordmask>>=1;
      }
      runlen+=word;
      if (word!=(1<<wordsize)-1) break; // unsaturated, end of sequence
      if (wordsize>=30) return -1;
      wordsize++;
    }
    runlen++;
    
    /* When the color is zero, we don't have to write anything.
     * Just advance the write head.
     */
    if (!color) {
      dstx+=runlen;
      if (dstx>=image->w) {
        dsty+=dstx/image->w;
        dstx%=image->w;
      }
      
    } else {
      // Emit individual bits. TODO This is wildly inefficient. Use an iterator on dst.
      while (runlen) {
        dst[dsty*image->stride+(dstx>>3)]|=0x80>>(dstx&7);
        runlen--;
        if (++dstx>=image->w) {
          dstx=0;
          dsty++;
          if (dsty>=image->h) break; // this is an error but whatever, just stop reading
        }
      }
    }
    color^=1;
  }
  
  /* With the image now complete, undo the XOR row filter.
   */
  if (flags&2) {
    uint8_t *rp=dst;
    uint8_t *wp=dst+image->stride;
    int c=image->stride*(image->h-1);
    for (;c-->0;rp++,wp++) (*wp)^=(*rp);
  }
  
  return 0;
}

/* Decode.
 */
 
struct rlead_image *rlead_decode(const void *src,int srcc) {
  if (!src||(srcc<7)) return 0;
  if (memcmp(src,"\xbb\xad",2)) return 0;
  const uint8_t *SRC=src;
  int w=(SRC[2]<<8)|SRC[3];
  int h=(SRC[4]<<8)|SRC[5];
  int flags=SRC[6];
  if ((w<1)||(w>0x7fff)) return 0;
  if ((h<1)||(h>0x7fff)) return 0;
  
  int stride=(w+7)>>3;
  struct rlead_image *image=calloc(1,sizeof(struct rlead_image));
  if (!image) return 0;
  if (!(image->v=calloc(stride,h))) {
    rlead_image_del(image);
    return 0;
  }
  image->w=w;
  image->h=h;
  image->stride=stride;
  
  if (rlead_decode_inner(image,SRC+7,srcc-7,flags)<0) {
    rlead_image_del(image);
    return 0;
  }
  return image;
}
