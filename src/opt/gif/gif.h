/* gif.h
 * Required: serial(encode only)
 * Minimal encoder and decoder for GIF images.
 *
 * Unlike our big sister PNG, we do not preserve comments and other extra chunks at decode.
 * I don't intend to support animation.
 * So animated GIFs are flattened during decode, and only the last frame is preserved.
 * Another consequence of that immediate flattening is that we discard color tables: Images will always be 32-bit RGBA.
 *
 * Also, Plain Text blocks. We quietly ignore them.
 *
 * The spec doesn't say one way or the other, but this implementation pronounces it with a soft G.
 */
 
#ifndef GIF_H
#define GIF_H

struct sr_encoder;

struct gif_image {
  void *v; // bytewise RGBA
  int w,h,stride;
};

void gif_image_cleanup(struct gif_image *image);
void gif_image_del(struct gif_image *image);

int gif_encode(struct sr_encoder *dst,const struct gif_image *image);

struct gif_image *gif_decode(const void *src,int srcc);

#endif
