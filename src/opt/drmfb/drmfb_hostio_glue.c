#include "drmfb_internal.h"
#include "opt/hostio/hostio_video.h"
#include <libdrm/drm_fourcc.h>
#include <limits.h>

/* Instance definition.
 */
 
struct hostio_video_drmfb {
  struct hostio_video hdr;
  struct drmfb *drmfb;
};

#define DRIVER ((struct hostio_video_drmfb*)driver)

/* Delete.
 */
 
static void _drmfb_del(struct hostio_video *driver) {
  drmfb_del(DRIVER->drmfb);
}

/* Init.
 */
 
struct drmfb_mode_context {
  struct hostio_video *driver;
  const struct hostio_video_setup *setup;
  struct drmfb_mode selection;
};

// (-1,0,1,2) = (a,tie,b,b-perfect); which of these modes do we prefer?
static int drmfb_compare_modes(
  const struct drmfb_mode *a,
  const struct drmfb_mode *b,
  const struct hostio_video_setup *setup
) {
  
  /* If one mode has invalid dimensions, drop it cold.
   * This will always happen the first time, when (a) is not initialized yet.
   */
  if ((a->w<1)||(a->h<1)) return 1;
  if ((b->w<1)||(b->h<1)) return -1;
  
  /* A perfect match is when the mode is already selected, and its dimensions match the request exactly.
   * Perfect is only applicable to (b); every mode will arrive here first as (b).
   */
  if (b->already) {
    if ((setup->w>0)&&(setup->h>0)) {
      if ((setup->w==b->w)&&(setup->h==b->h)) return 2;
    } else if ((setup->fbw>0)&&(setup->fbh>0)) {
      if ((setup->fbw==b->w)&&(setup->fbh==b->h)) return 2;
    }
  }
  
  /* When they didn't request a screen size, take the one already running.
   * This is just my opinion maybe? It's annoying when games change the monitor resolution.
   * A caller providing explicit screen size is asking to change resolution, so allow it then.
   * Most cases, this is the only block that matters.
   */
  if ((setup->w<=0)||(setup->h<=0)) {
    if (a->already) return -1;
    if (b->already) return 1;
  }
  
  /* When a screen size was requested, take exact matches of it.
   */
  if ((setup->w>0)&&(setup->h>0)) {
    if ((a->w!=b->w)||(a->h!=b->h)) {
      if ((a->w==setup->w)&&(a->h==setup->h)) return -1;
      if ((b->w==setup->w)&&(b->h==setup->h)) return 1;
    }
  }
  
  /* If rates don't match, take the one nearer 60 Hz.
   */
  if (a->rate!=b->rate) {
    int da=a->rate-60; if (da<0) da=-da;
    int db=b->rate-60; if (db<0) db=-db;
    if (da<db) return -1;
    if (da>db) return 1;
  }
  
  /* Compare to requested framebuffer size.
   * We only want modes larger or equal to the framebuffer.
   * A match of aspect ratio is better than a match of absolute size.
   */
  if ((setup->fbw>0)&&(setup->fbh>0)&&((a->w!=b->w)||(a->h!=b->h))) {
    int aok=((a->w>=setup->fbw)&&(a->h>=setup->fbh));
    int bok=((b->w>=setup->fbw)&&(b->h>=setup->fbh));
    if (aok&&!bok) return -1;
    if (!aok&&bok) return 1;
    if (aok&&bok) {
      // The ideal aspect comparator tells us what proportion of the screen is covered by a letterbox or pillarbox.
      // Since it's software framebuffers everywhere, assume that scaling will be by integer multiples.
      int axscale=a->w/setup->fbw;
      int ayscale=a->h/setup->fbh;
      int ascale=(axscale<ayscale)?axscale:ayscale;
      int bxscale=b->w/setup->fbw;
      int byscale=b->h/setup->fbh;
      int bscale=(bxscale<byscale)?bxscale:byscale;
      int adstw=setup->fbw*ascale;
      int adsth=setup->fbh*ascale;
      int bdstw=setup->fbw*bscale;
      int bdsth=setup->fbh*bscale;
      double afill=(double)(adstw*adsth)/(double)(a->w*a->h);
      double bfill=(double)(bdstw*bdsth)/(double)(b->w*b->h);
      // Where aspect is concerned, it's not exact. Let's say anything within 10% is fine.
      double relaspect=afill/bfill;
      if (relaspect>1.111) return -1;
      if (relaspect<0.900) return 1;
      // Less scaling is better.
      if (ascale<bscale) return -1;
      if (ascale>bscale) return 1;
    }
  }
  
  /* And finally, no decision.
   */
  return 0;
}
 
static int _drmfb_cb_mode(const struct drmfb_mode *mode,void *userdata) {
  struct drmfb_mode_context *ctx=userdata;
  struct hostio_video *driver=ctx->driver;
  /**
  fprintf(stderr,
    "%s %d[%d:%d] (%d,%d) %dhz %s\n",
    __func__,mode->kid,mode->connector_id,mode->modep,
    mode->w,mode->h,mode->rate,
    mode->already?"DEFAULT":""
  );
  /**/
  int choice=drmfb_compare_modes(&ctx->selection,mode,ctx->setup);
  if (choice<=0) return 0; // Keep what we have. (ties break toward the first seen).
  ctx->selection=*mode;
  if (choice>=2) return 1; // This selection is perfect, stop looking.
  return 0; // Keep this new one but keep looking.
}
 
static int _drmfb_init(struct hostio_video *driver,const struct hostio_video_setup *setup) {
  if (!(DRIVER->drmfb=drmfb_new())) return -1;
  struct drmfb_mode_context ctx={.driver=driver,.setup=setup};
  if (drmfb_for_each_mode(DRIVER->drmfb,_drmfb_cb_mode,&ctx)<0) return -1;
  if (drmfb_set_mode(DRIVER->drmfb,&ctx.selection)<0) return -1;
  const struct drmfb_mode *mode=drmfb_get_mode(DRIVER->drmfb);
  if (!mode) return -1;
  driver->w=mode->w;
  driver->h=mode->h;
  driver->fullscreen=1;
  return 0;
}

/* Framebuffer description.
 */
 
static void drmfb_2101010_finish(struct hostio_video_fb_description *desc,int alpha) {
  // For 32-bit RGB formats with 10-bit chromas, our big table only sets (rmask), and we figure out the rest here.
  // (chorder) will remain unset; that's only for bytewise channels.
  desc->pixelsize=32;
  if (desc->rmask&0xffff0000) {
    desc->gmask=desc->rmask>>10;
    desc->bmask=desc->gmask>>10;
  } else {
    desc->gmask=desc->rmask<<10;
    desc->bmask=desc->gmask<<10;
  }
  if (alpha) desc->amask=~(desc->rmask|desc->gmask|desc->bmask);
}
 
static int _drmfb_fb_describe(struct hostio_video_fb_description *desc,struct hostio_video *driver) {
  const struct drmfb_mode *mode=drmfb_get_mode(DRIVER->drmfb);
  if (!mode) return -1;
  desc->w=mode->w;
  desc->h=mode->h;
  if ((desc->stride=DRIVER->drmfb->fbv[0].stridewords<<2)<0) return -1;
  desc->pixelsize=0;
  desc->rmask=0;
  desc->gmask=0;
  desc->bmask=0;
  desc->amask=0;
  memset(desc->chorder,0,4);
  
  uint32_t pixel_format=drmfb_get_pixel_format(DRIVER->drmfb);
  switch (pixel_format) {
    case DRM_FORMAT_C8: desc->pixelsize=8; desc->chorder[0]='i'; break;
    // There's no Gray but there is Red (and no Green or Blue). I'm assuming "Red" means "Gray".
    case DRM_FORMAT_R8: desc->pixelsize=8; desc->chorder[0]='y'; desc->rmask=desc->gmask=desc->bmask=0xff; break;
    case DRM_FORMAT_R16: desc->pixelsize=16; desc->rmask=desc->gmask=desc->bmask=0xffff; break;
    // RG88, GR88, RG32, GR32: Unclear from the header, which channel in which position. And why does "Red+Green" even exist?
    // RGB332, BGR233: I'm reading the channel order big-endian. Might have got it backward. I haven't seen this format in the wild.
    case DRM_FORMAT_RGB332: desc->pixelsize=8; desc->rmask=0xe0; desc->gmask=0x1c; desc->bmask=0x03; break;
    case DRM_FORMAT_BGR233: desc->pixelsize=8; desc->rmask=0x07; desc->gmask=0x38; desc->bmask=0xc0; break;
    // The more realistic, but no less annoying, 16-bit RGB formats. Again, assuming drm_fourcc.h lists channels big-endianly.
    case DRM_FORMAT_XRGB4444: desc->pixelsize=16; desc->rmask=0x0f00; desc->gmask=0x00f0; desc->bmask=0x000f; break;
    case DRM_FORMAT_XBGR4444: desc->pixelsize=16; desc->rmask=0x0004; desc->gmask=0x00f0; desc->bmask=0x0f00; break;
    case DRM_FORMAT_RGBX4444: desc->pixelsize=16; desc->rmask=0xf000; desc->gmask=0x0f00; desc->bmask=0x00f0; break;
    case DRM_FORMAT_BGRX4444: desc->pixelsize=16; desc->rmask=0x00f0; desc->gmask=0x0f00; desc->bmask=0xf000; break;
    case DRM_FORMAT_ARGB4444: desc->pixelsize=16; desc->rmask=0x0f00; desc->gmask=0x00f0; desc->bmask=0x000f; desc->amask=0xf000; break;
    case DRM_FORMAT_ABGR4444: desc->pixelsize=16; desc->rmask=0x000f; desc->gmask=0x00f0; desc->bmask=0x0f00; desc->amask=0xf000; break;
    case DRM_FORMAT_RGBA4444: desc->pixelsize=16; desc->rmask=0xf000; desc->gmask=0x0f00; desc->bmask=0x00f0; desc->amask=0x000f; break;
    case DRM_FORMAT_BGRA4444: desc->pixelsize=16; desc->rmask=0x00f0; desc->gmask=0x0f00; desc->bmask=0xf000; desc->amask=0x000f; break;
    case DRM_FORMAT_XRGB1555: desc->pixelsize=16; desc->rmask=0x7c00; desc->gmask=0x03e0; desc->bmask=0x001f; break;
    case DRM_FORMAT_XBGR1555: desc->pixelsize=16; desc->rmask=0x001f; desc->gmask=0x03e0; desc->bmask=0xec00; break;
    case DRM_FORMAT_RGBX5551: desc->pixelsize=16; desc->rmask=0xf800; desc->gmask=0x0ec0; desc->bmask=0x003e; break;
    case DRM_FORMAT_BGRX5551: desc->pixelsize=16; desc->rmask=0x003e; desc->gmask=0x0ec0; desc->bmask=0xf800; break;
    case DRM_FORMAT_ARGB1555: desc->pixelsize=16; desc->rmask=0x7c00; desc->gmask=0x03e0; desc->bmask=0x001f; desc->amask=0x8000; break;
    case DRM_FORMAT_ABGR1555: desc->pixelsize=16; desc->rmask=0x001f; desc->gmask=0x03e0; desc->bmask=0xec00; desc->amask=0x8000; break;
    case DRM_FORMAT_RGBA5551: desc->pixelsize=16; desc->rmask=0xf800; desc->gmask=0x07c0; desc->bmask=0x003e; desc->amask=0x0001; break;
    case DRM_FORMAT_BGRA5551: desc->pixelsize=16; desc->rmask=0x003e; desc->gmask=0x07c0; desc->bmask=0xf800; desc->amask=0x0001; break;
    case DRM_FORMAT_RGB565:   desc->pixelsize=16; desc->rmask=0xf800; desc->gmask=0x07e0; desc->bmask=0x001f; break;
    case DRM_FORMAT_BGR565:   desc->pixelsize=16; desc->rmask=0x001f; desc->gmask=0x07e0; desc->bmask=0xf800; break;
    // 24-bit we only make "chorder" available, not "mask" (since you can't read them as natural words).
    case DRM_FORMAT_RGB888: desc->pixelsize=24; memcpy(desc->chorder,"rgb",3); break;
    case DRM_FORMAT_BGR888: desc->pixelsize=24; memcpy(desc->chorder,"bgr",3); break;
    #define xmask pixelsize /* so "x" channels get harmlessly ignored */
    #define BYTEWISERGBA(drmtag,c0,c1,c2,c3) case DRM_FORMAT_##drmtag: { \
      desc->chorder[0]=(#c0)[0]; \
      desc->chorder[1]=(#c1)[0]; \
      desc->chorder[2]=(#c2)[0]; \
      desc->chorder[3]=(#c3)[0]; \
      desc->c0##mask=0x000000ff; \
      desc->c1##mask=0x0000ff00; \
      desc->c2##mask=0x00ff0000; \
      desc->c3##mask=0xff000000; \
      desc->pixelsize=32; \
    } break;
    BYTEWISERGBA(XRGB8888,b,g,r,x)
    BYTEWISERGBA(XBGR8888,r,g,b,x)
    BYTEWISERGBA(RGBX8888,x,b,g,r)
    BYTEWISERGBA(BGRX8888,x,r,g,b)
    BYTEWISERGBA(ARGB8888,b,g,r,a)
    BYTEWISERGBA(ABGR8888,r,g,b,a)
    BYTEWISERGBA(RGBA8888,a,b,g,r)
    BYTEWISERGBA(BGRA8888,a,r,g,b)
    #undef BYTEWISERGBA
    #undef xmask
    // 32-bit RGBA divided in 10s and 2s:
    case DRM_FORMAT_XRGB2101010: desc->rmask=0x3ff00000; drmfb_2101010_finish(desc,0); break;
    case DRM_FORMAT_XBGR2101010: desc->rmask=0x000003ff; drmfb_2101010_finish(desc,0); break;
    case DRM_FORMAT_RGBX1010102: desc->rmask=0xffc00000; drmfb_2101010_finish(desc,0); break;
    case DRM_FORMAT_BGRX1010102: desc->rmask=0x00000ffc; drmfb_2101010_finish(desc,0); break;
    case DRM_FORMAT_ARGB2101010: desc->rmask=0x3ff00000; drmfb_2101010_finish(desc,1); break;
    case DRM_FORMAT_ABGR2101010: desc->rmask=0x000003ff; drmfb_2101010_finish(desc,1); break;
    case DRM_FORMAT_RGBA1010102: desc->rmask=0xffc00000; drmfb_2101010_finish(desc,1); break;
    case DRM_FORMAT_BGRA1010102: desc->rmask=0x00000ffc; drmfb_2101010_finish(desc,1); break;
    // drm_fourcc.h declares many, many more. Floating-point, 64-bit, YUV, vendor-specific, lots of stuff I don't expect to support.
    default: return -1;
  }
  // Bytewise RGBA was written with masks assuming a little-endian host.
  // Detect big-endian and swap.
  if ((desc->pixelsize==32)&&desc->chorder[0]) {
    uint32_t bodetect=0x04030201;
    if (*(uint8_t*)&bodetect==0x04) {
      desc->rmask=((uint32_t)desc->rmask>>24)|((desc->rmask>>8)&0xff00)|((desc->rmask<<8)&0xff0000)|(desc->rmask<<24);
      desc->gmask=((uint32_t)desc->gmask>>24)|((desc->gmask>>8)&0xff00)|((desc->gmask<<8)&0xff0000)|(desc->gmask<<24);
      desc->bmask=((uint32_t)desc->bmask>>24)|((desc->bmask>>8)&0xff00)|((desc->bmask<<8)&0xff0000)|(desc->bmask<<24);
      desc->amask=((uint32_t)desc->amask>>24)|((desc->amask>>8)&0xff00)|((desc->amask<<8)&0xff0000)|(desc->amask<<24);
    }
  }
  //TODO What does a big-endian host mean to the 16-bit formats?
  return 0;
}

/* Render fences.
 */
 
static void *_drmfb_fb_begin(struct hostio_video *driver) {
  return drmfb_begin(DRIVER->drmfb);
}

static int _drmfb_fb_end(struct hostio_video *driver) {
  return drmfb_end(DRIVER->drmfb);
}

/* Type definition.
 */
 
const struct hostio_video_type hostio_video_type_drmfb={
  .name="drmfb",
  .desc="Direct framebuffer with Linux DRM.",
  .objlen=sizeof(struct hostio_video_drmfb),
  .appointment_only=0,
  .provides_input=0,
  .del=_drmfb_del,
  .init=_drmfb_init,
  .fb_describe=_drmfb_fb_describe,
  .fb_begin=_drmfb_fb_begin,
  .fb_end=_drmfb_fb_end,
};
