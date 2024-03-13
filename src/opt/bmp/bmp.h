/* bmp.h
 * Required: serial(encode only)
 *
 * Minimal encoder and decoder for Microsoft's ridiculous BMP format.
 * We accept with or without BITMAPFILEHEADER: Images embedded in ICO files don't include it.
 */
 
#ifndef BMP_H
#define BMP_H

struct sr_encoder;

struct bmp_image {
  void *v;
  int w,h;
  int stride;
  int pixelsize; // bits
  void *ctab; // 32-bit rgba
  int ctabc; // in 4-byte entries
  int compression;
  int rmask,gmask,bmask,amask;
};

void bmp_image_cleanup(struct bmp_image *image);
void bmp_image_del(struct bmp_image *image);

int bmp_encode(struct sr_encoder *dst,const struct bmp_image *image,int skip_file_header);

struct bmp_image *bmp_decode(const void *src,int srcc);

/* BMP supports a loose set of pixel formats.
 * To constrain that a bit, call this to rewrite the pixels in place such that they are PNG-legal.
 */
int bmp_force_png_format(struct bmp_image *image);

#endif
