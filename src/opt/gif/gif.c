#include "gif.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* Cleanup.
 */

void gif_image_cleanup(struct gif_image *image) {
  if (!image) return;
  if (image->v) free(image->v);
}

void gif_image_del(struct gif_image *image) {
  if (!image) return;
  gif_image_cleanup(image);
  free(image);
}
