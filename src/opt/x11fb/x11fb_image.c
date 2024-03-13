#include "x11fb_internal.h"

/* New image.
 */
 
XImage *x11fb_new_image(struct x11fb *x11fb,int w,int h) {
  if ((w<1)||(h<1)) return 0;
  void *pixels=calloc(w<<2,h);
  if (!pixels) return 0;
  XImage *image=XCreateImage(
    x11fb->dpy,DefaultVisual(x11fb->dpy,x11fb->screen),
    24,ZPixmap,0,pixels,w,h,32,w<<2
  );
  if (!image) {
    free(pixels);
    return 0;
  }
  return image;
}

/* Scale.
 */
 
void x11fb_scale(XImage *dst,struct x11fb *x11fb,XImage *src) {
  int scale=dst->width/src->width;
  if (scale<1) return;
  
  // When scale is one, we shouldn't have been called.
  // But it's so easy to handle, might as well call it out.
  // Images that land here must always have minimum stride.
  if (scale==1) {
    memcpy(dst->data,src->data,(src->width<<2)*src->height);
    return;
  }
  
  // Opportunity to implement fancy interpolative scalers, if we choose.
  
  // Generic discrete nearest-neighbor scale up.
  // NB "stride" here are in pixels, not bytes.
  int dststride=dst->width;
  int srcstride=src->width;
  int rowcpc=dststride<<2;
  uint32_t *dstrow=(uint32_t*)dst->data;
  const uint32_t *srcrow=(uint32_t*)src->data;
  int yi=src->height;
  for (;yi-->0;srcrow+=srcstride) {
  
    // Copy the first row of (dst) pixel by pixel.
    uint32_t *dstp=dstrow;
    const uint32_t *srcp=srcrow;
    int xi=src->width;
    for (;xi-->0;srcp++) {
      int pi=scale;
      for (;pi-->0;dstp++) *dstp=*srcp;
    }
    
    // Now copy that output row (scale-1) times.
    // If we're going really large, it would make sense to copy multiple rows as they become available.
    // But that's complicated and I don't expect scaling to go beyond like 3 or 4.
    const uint32_t *dstready=dstrow;
    dstrow+=dststride;
    int pi=scale-1;
    for (;pi-->0;dstrow+=dststride) memcpy(dstrow,dstready,rowcpc);
    
  }
}
