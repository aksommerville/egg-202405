/* png.h
 * Required: serial (encode only)
 * Link: -lz
 *
 * Simple PNG decoder and encoder.
 * Deviations from spec:
 *  - Interlaced images not supported at all.
 *  - We impose a size limit 32767 per axis.
 *  - We don't acknowledge tRNS for non-indexed formats when converting (per spec, it should be a colorkey).
 *  - Encoding, we put all extra chunks before IDAT. (that might be legal, I'm not sure, maybe some have to go after IDAT?).
 *  - We don't validate CRCs.
 *  - We don't fail on unknown critical chunks.
 *  - Decoding, we accept extra chunks before IHDR. Only it must be before IDAT.
 *  - - Pre-IHDR chunks will be quietly discarded, due to a decoder quirk that would make keeping them inconvenient.
 *  - Decoding, we permit missing or short IDAT. Pixels are zero if not touched.
 *  - Decoding, we do not require an IEND.
 */
 
#ifndef PNG_H
#define PNG_H

struct sr_encoder;

struct png_image {
  void *v;
  int w,h,stride;
  int depth,colortype;
  int pixelsize; // bits, total
  struct png_chunk { // IHDR,IDAT,IEND are never included
    char chunktype[4];
    int c;
    void *v;
  } *chunkv;
  int chunkc,chunka;
};

void png_image_cleanup(struct png_image *image);
void png_image_del(struct png_image *image);

/* New image with newly-allocated and zeroed pixels.
 */
struct png_image *png_image_new(int w,int h,int depth,int colortype);

struct png_chunk *png_image_add_chunk(struct png_image *image,const char chunktype[4],const void *v,int c);
void png_image_remove_chunk(struct png_image *image,int p);
int png_image_get_chunk(void *dstpp,const struct png_image *image,const char chunktype[4],int ix); // (ix) zero normally

/* Rewrite to the given depth and colortype, in place.
 * (image->v) may be freed and re-allocated during the conversion.
 * Stride will always be its minimum after reformat. We'll do only that, if (depth,colortype) already match.
 */
int png_image_reformat(struct png_image *image,int depth,int colortype);

/* Produce a complete PNG file in (dst).
 * Stride does not need to be minimized first.
 * Chunks are encoded just as is, and all before the IDAT chunk.
 */
int png_encode(struct sr_encoder *dst,const struct png_image *image);

/* Decode PNG file in one shot.
 * All chunks except IHDR,IDAT,IEND are preserved blindly.
 */
struct png_image *png_decode(const void *src,int srcc);

int png_calculate_pixel_size(int depth,int colortype);
int png_minimum_stride(int w,int pixelsize);

/* Aside from "reformat" and raw access, we don't provide general image operations.
 * This unit should be used in conjunction with some other generic software-imaging unit.
 */

#endif
