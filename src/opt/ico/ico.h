/* ico.h
 * Required: serial
 * Optional: png
 * We do not use our sister "bmp" format; bmp embedded in ico is quirky enough to warrant its own implementation.
 * Wish I'd god damn known that before writing the bmp unit :(
 *
 * Minimal interface to Microsoft Icon files.
 * Used by "favicon.ico", and I like them as general-purpose multi-size icons.
 */
 
#ifndef ICO_H
#define ICO_H

struct sr_encoder;

struct ico_file {
  struct ico_image {
    void *v;
    int w,h; // Present before decode. Max 256.
    int stride; // Absent until decode.
    int ctc,planec,pixelsize; // Before decode.
    int format;
    void *serial;
    int serialc;
  } *imagev;
  int imagec,imagea;
};

void ico_image_cleanup(struct ico_image *image);
void ico_file_cleanup(struct ico_file *file);
void ico_file_del(struct ico_file *file);

int ico_encode(struct sr_encoder *dst,struct ico_file *file);

struct ico_file *ico_decode(const void *src,int srcc);

/* Return a WEAK reference to an image in (file), exact match for (w,h), or if (obo) the closest match.
 * If the image was encoded, we decode it in place before returning.
 */
struct ico_image *ico_file_get_image(struct ico_file *file,int w,int h,int obo);

/* Decode image in place.
 * Usually you should let ico_file_get_image() do this for you.
 * Noop if already decoded.
 */
int ico_image_decode(struct ico_image *image);

/* Call if you've modified the image data and intend to re-encode.
 * Without this, we might have the original encoded data still present and would lose your changes.
 * Redundant calls are cheap and harmless.
 */
void ico_image_dirty(struct ico_image *image);

/* Plain "add_image" allocates an RGBA pixel buffer.
 * "uninitialized" leaves the whole thing zeroed.
 */
struct ico_image *ico_file_add_image(struct ico_file *file,int w,int h);
struct ico_image *ico_file_add_uninitialized_image(struct ico_file *file);

#endif
