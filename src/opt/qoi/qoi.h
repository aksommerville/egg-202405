/* qoi.h
 * Minimal encoder and decoder for Quite OK Image format.
 */
 
#ifndef QOI_H
#define QOI_H

struct sr_encoder;

struct qoi_image {
  void *v; // bytewise RGBA, always minimum stride (w*4).
  int w,h;
};

void qoi_image_cleanup(struct qoi_image *image);
void qoi_image_del(struct qoi_image *image);

int qoi_encode(struct sr_encoder *dst,const struct qoi_image *image);

struct qoi_image *qoi_decode(const void *src,int srcc);

#endif
