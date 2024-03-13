/* rlead.h
 * Adaptive Run Length Encoding.
 * My own invention, a format for 1-bit images.
 * PNG does a better job compressing. If you don't mind including zlib, prefer PNG.
 * With rlead you should expect 25-50% compression for most images.
 */
 
#ifndef RLEAD_H
#define RLEAD_H

struct sr_encoder;

struct rlead_image {
  void *v;
  int w,h,stride;
};

void rlead_image_cleanup(struct rlead_image *image);
void rlead_image_del(struct rlead_image *image);

int rlead_encode(struct sr_encoder *dst,const struct rlead_image *image);

struct rlead_image *rlead_decode(const void *src,int srcc);

#endif
