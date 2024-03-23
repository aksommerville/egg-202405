#include "eggrom_internal.h"
#include "opt/rawimg/rawimg.h"

/* Nonzero if (fmt) is permissible in ROM files.
 * We could support all formats for native runners, but on the web side it's either 
 * write the decoder ourselves or have the browser do it asynchronously.
 * Async is pretty much out of the question.
 * So we're only supporting the formats we can easily write decoders for: rawimg, qoi, rlead
 * I'm also calling ico valid so it can be used for the app icon. These will fail at runtime if you try to load them as is.
 */
 
static int eggrom_image_fmt_valid(const char *fmt) {
  if (!strcmp(fmt,"rawimg")) return 1;
  if (!strcmp(fmt,"qoi")) return 1;
  if (!strcmp(fmt,"rlead")) return 1;
  if (!strcmp(fmt,"ico")) return 1;
  return 0;
}

/* Try encoding to a given format, if it is valid for output.
 * <0 for errors, 0 for decline, >0 if reencoded.
 * *** If we return >0, we have deleted (rawimg), as a convenience ***
 */
 
static int eggrom_image_try_encode(struct sr_encoder *dst,struct rawimg *rawimg,const char *fmt) {
  dst->c=0;
  if (!eggrom_image_fmt_valid(fmt)) return 0;
  rawimg->encfmt=fmt;
  if (rawimg_encode(dst,rawimg)<0) return -1;
  rawimg_del(rawimg);
  return 1;
}

/* Reencode image if necessary.
 */
 
int eggrom_image_compile(struct sr_encoder *dst,const struct romw_res *res) {
  int err;

  // Already in an acceptable output format? Keep it, don't even decode.
  const char *fmt=rawimg_detect_format(res->serial,res->serialc);
  if (eggrom_image_fmt_valid(fmt)) return 0;
  
  // Decode generically.
  struct rawimg *rawimg=rawimg_decode(res->serial,res->serialc);
  if (!rawimg) {
    fprintf(stderr,"%s: Failed to decode image (%d bytes, format '%s')\n",res->path,res->serialc,fmt);
    return -2;
  }
  
  //TODO Detect 1-bit images that were encoded larger. eg it's hard to convince GIMP to save a 1-bit PNG.
  // (well, it's not *hard*, it's just easy to forget...)
  
  // If PNG is allowed, it's always the best choice.
  if (eggrom_image_try_encode(dst,rawimg,"png")>0) return 0;
  
  // For pixel sizes 24 and 32, QOI is usually good.
  if ((rawimg->pixelsize==24)||(rawimg->pixelsize==32)) {
    if (eggrom_image_try_encode(dst,rawimg,"qoi")>0) return 0;
  }
  
  // For pixel size 1, only PNG beats RLEAD, and even then not always.
  // RLEAD is great, it must have been invented by somebody very clever.
  if (rawimg->pixelsize==1) {
    if (eggrom_image_try_encode(dst,rawimg,"rlead")>0) return 0;
  }
  
  // And now we're getting weird. Try all the other known formats, except ICO.
  // Do try qoi again, maybe we have a pixel size other than 24 or 32.
  // Do not try rlead again; it really only works for 1-bit input.
  if (eggrom_image_try_encode(dst,rawimg,"qoi")>0) return 0;
  if (eggrom_image_try_encode(dst,rawimg,"jpeg")>0) return 0;
  if (eggrom_image_try_encode(dst,rawimg,"gif")>0) return 0;
  if (eggrom_image_try_encode(dst,rawimg,"bmp")>0) return 0;
  
  // And if nothing else worked, at least "rawimg" should always be an option.
  if (eggrom_image_try_encode(dst,rawimg,"rawimg")>0) return 0;

  fprintf(stderr,"%s: No suitable output format found.\n",res->path);
  rawimg_del(rawimg);
  return -2;
}
