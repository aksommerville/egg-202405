/* rawimg.h
 * Required: serial
 * Optional: bmp gif ico png qoi rlead jpeg
 *
 * Generic in-memory image format, and plain uncompressed serial format.
 *
 * Serial format:
 *   0000   4 Signature: "\x00raw"
 *   0004   2 Width 1..32767
 *   0006   2 Height 1..32767
 *   0008   4 Red mask -- For pixelsize 8 or 16, it may be in the low or high part, you must check both.
 *   000c   4 Green mask
 *   0010   4 Blue mask
 *   0014   4 Alpha mask
 *   0018   4 Channel order
 *   001c   1 Bit order
 *   001d   1 Pixel size, bits (factor or multiple of 8)
 *   001e   2 Color table size in entries.
 *   0020 ... Pixels. (ceil((Width*Pixelsize)/8)*Height)
 *   .... ... Color table, RGBA. (ctsize*4)
 */
 
#ifndef RAWIMG_H
#define RAWIMG_H

#include <stdint.h>

struct sr_encoder;

#define RAWIMG_XFORM_XREV 1
#define RAWIMG_XFORM_YREV 2
#define RAWIMG_XFORM_SWAP 4

struct rawimg {
  void *v;
  int ownv;
  int w,h;
  int stride;
  int pixelsize;
  
  /* For 8, 16, and 32 bit pixels, optionally populate these mask to show
   * how the pixel is laid out when read as a native word.
   */
  uint32_t rmask,gmask,bmask,amask;
  
  /* Any image with channels laid out bytewise may populate this with the channel names in storage order.
   * Uppercase if the channel is big-endian, lower if little-endian, and either is OK if 8-bit.
   * eg 48-bit PNG should set "RGB\0" here.
   * Many images can use both *mask and chorder, some can only use one, and some neither.
   */
  char chorder[4];
  
  /* Pixels smaller than 8 bits must indicate the packing order:
   *   '<' = little-endian = 0x01 first. eg GIF.
   *   '>' = big-endian = 0x80 first. eg PNG. (and, oddly, BMP).
   */
  char bitorder;
  
  /* Color table is entirely optional.
   * If present, we free it at cleanup.
   * Entries must be [R,G,B,A].
   * (ctabc) is the count of entries, not bytes.
   * Don't assume that it's long enough for (pixelsize).
   * We do not use this during iteration or conversion. But of course you can, in the convert callback.
   */
  uint8_t *ctab;
  int ctabc;
  
  /* Name of encoding that decoded images came from.
   * Replace before encode if you want to produce a specific format.
   * Should be a compile-time constant.
   * Null or empty, we default to "rawimg", our own format.
   */
  const char *encfmt;
};

/* Image object.
 *******************************************************************/

void rawimg_cleanup(struct rawimg *rawimg);
void rawimg_del(struct rawimg *rawimg);

struct rawimg *rawimg_new_alloc(int w,int h,int pixelsize);
struct rawimg *rawimg_new_handoff(void *v,int w,int h,int stride,int pixelsize);
struct rawimg *rawimg_new_borrow(void *v,int w,int h,int stride,int pixelsize);
struct rawimg *rawimg_new_copy(const struct rawimg *src); // Minimizes stride, otherwise identical to (src).

/* Reformat, crop, apply axiswise transform.
 * Can crop but can't extend -- bounds must be valid for (src).
 * Always produces a new image with minimum stride on success, even if identical to src.
 */
struct rawimg *rawimg_new_convert(
  const struct rawimg *src,
  int pixelsize,
  int x,int y,int w,int h,
  uint8_t xform,
  int (*cvt)(int srcpx,void *userdata),
  void *userdata
);

/* Rewrite image in place in the most appropriate canonical format:
 *  - rgba8, R stored first.
 *  - y8.
 *  - y1be.
 * We also supply "rgb", 24 bits, R first, but do not generically canonicalize to it.
 */
int rawimg_canonicalize(struct rawimg *rawimg);
int rawimg_force_rgba(struct rawimg *rawimg);
int rawimg_force_rgb(struct rawimg *rawimg);
int rawimg_force_y8(struct rawimg *rawimg);
int rawimg_force_y1(struct rawimg *rawimg);

/* Test for canonical formats. (0,1,2)=( No, Ambiguous, Yes ).
 * "Ambiguous" means the size is correct but format isn't fully specified.
 */
int rawimg_is_rgba(const struct rawimg *rawimg);
int rawimg_is_rgb(const struct rawimg *rawimg);
int rawimg_is_y8(const struct rawimg *rawimg);
int rawimg_is_y1(const struct rawimg *rawimg);

int rawimg_set_ctab(struct rawimg *rawimg,const void *rgba,int colorc);
int rawimg_require_ctab(struct rawimg *rawimg,int colorc);

/* Encoding.
 *****************************************************************/

/* We check other available image-format units, and can produce or consume any of them.
 * (rawimg->encfmt) can be null or empty string for "rawimg" format.
 * Otherwise: "png" "jpeg" "gif" "bmp" "ico" "qoi" "rlead" "rawimg"
 * GIF gets flattened on decode, and ICO we select the largest image.
 *
 * Decoding pretty much always works; rawimg is a superset of most encodable formats,
 * and we fudge a few format-specific peculiarities (eg LRBT BMPs).
 *
 * Encoding not so much: The rawimg must be in a pixel format amenable to the requested encoding.
 * We won't do that heavy conversion here.
 */
int rawimg_encode(struct sr_encoder *encoder,const struct rawimg *rawimg);
struct rawimg *rawimg_decode(const void *src,int srcc);

/* Happens automatically as needed. But maybe you have other uses for this.
 * Format units do not need to be enabled for format detection to work.
 */
const char *rawimg_detect_format(const void *src,int srcc);

/* Only those formats actually enabled at compile time will report.
 * (p) from zero.
 */
const char *rawimg_supported_format_by_index(int p);

/* Iteration.
 ******************************************************************/

struct rawimg_iterator_1 {
  void *p;
  int d;
  int pbit;
  int dpbit;
  int c; // in pixels
};

struct rawimg_iterator {
  struct rawimg_iterator_1 minor;
  struct rawimg_iterator_1 major;
  int minorc;
  int pixelsize;
  void *pixels;
  int sizebit; // +(1,2,4) if big-endian, -(1,2,4) if little-endian
  int (*read)(const struct rawimg_iterator *iter);
  void (*write)(struct rawimg_iterator *iter,int pixel);
};

/* (x,y,w,h) must be within (image) bounds.
 * (xform) tells us which corner to start at and which direction to travel.
 * RAWIMG_XFORM_SWAP makes us travel vertically first then horizontally -- it does NOT change (w,h).
 * On success, (iter) is pointing at the first pixel and ready to use.
 *
 * Note that bytewise pixels can be iterated much more efficiently than this.
 * We only supply this generic iterator that works for all sizes.
 */
int rawimg_iterate(
  struct rawimg_iterator *iter,
  const struct rawimg *image,
  int x,int y,int w,int h,
  uint8_t xform
);

/* Advance to the next pixel.
 * Returns zero if complete, or nonzero if we're pointing to the next pixel.
 */
static inline int rawimg_iterator_next(struct rawimg_iterator *iter) {
  if (iter->minor.c) {
    iter->minor.c--;
    if (iter->minor.dpbit) {
      iter->minor.pbit+=iter->minor.dpbit;
    } else {
      iter->minor.p=(char*)iter->minor.p+iter->minor.d;
    }
    return 1;
  }
  if (iter->major.c) {
    iter->major.c--;
    if (iter->major.dpbit) {
      iter->major.pbit+=iter->major.dpbit;
      iter->minor.pbit=iter->major.pbit;
    } else {
      iter->major.p=(char*)iter->major.p+iter->major.d;
      iter->minor.p=iter->major.p;
    }
    iter->minor.c=iter->minorc;
    return 1;
  }
  return 0;
}

static inline int rawimg_iterator_read(struct rawimg_iterator *iter) {
  return iter->read(iter);
}

static inline void rawimg_iterator_write(struct rawimg_iterator *iter,int pixel) {
  iter->write(iter,pixel);
}

/* Odds, ends.
 ***************************************************************/

/* Validates both parameters and returns the smallest legal stride for such an image.
 * This is always the stride of encoded images.
 * Live images may exceed it.
 */
int rawimg_minimum_stride(int w,int pixelsize);

#endif
