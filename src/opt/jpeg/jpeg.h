/* jpeg.h
 * Required: serial
 * Optional:
 * Link: -ljpeg
 * apt install libjpeg-dev
 *
 * Adapter for libjpeg, conforming to our vague image API.
 * To gain a sense of why we didn't implement this independently, have a read of jpeglib.h...
 */
 
#ifndef JPEG_H
#define JPEG_H

struct sr_encoder;

struct jpeg_image {
  void *v;
  int w,h;
  int chanc; // 1 or 3
  int stride;
};

void jpeg_image_cleanup(struct jpeg_image *image);
void jpeg_image_del(struct jpeg_image *image);

int jpeg_encode(struct sr_encoder *dst,const struct jpeg_image *image);

struct jpeg_image *jpeg_decode(const void *src,int srcc);

#endif
