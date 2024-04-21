#include "egg_native_internal.h"
#include "opt/rawimg/rawimg.h"

#define NATIVE 1
#define BUNDLED 2
#define EXTERNAL 3

#if EGG_ROM_SOURCE==NATIVE
  // When NATIVE, there is a ROM file baked in at build time, but it has no wasm resources.
  // Instead, you must also link against native implementations of the Egg Client API.
  extern const unsigned char egg_bundled_rom[];
  extern const int egg_bundled_rom_length;
  int egg_native_uses_rom_file() { return 0; }
  
#elif EGG_ROM_SOURCE==BUNDLED
  // BUNDLED, the entire ROM file is baked in at build time, and otherwise we're just a runner.
  extern const unsigned char egg_bundled_rom[];
  extern const int egg_bundled_rom_length;
  int egg_native_uses_rom_file() { return 0; }
  
#elif EGG_ROM_SOURCE==EXTERNAL
  // EXTERNAL is the typical case: Load a ROM file at runtime.
  int egg_native_uses_rom_file() { return 1; }
  
#else
  #error "Please define EGG_ROM_SOURCE to one of: NATIVE BUNDLED EXTERNAL"
#endif

/* Cleanup.
 */
 
void egg_native_rom_cleanup() {
  romr_cleanup(&egg.romr);
  if (egg.romserial) free(egg.romserial);
  if (egg.romtitle) free(egg.romtitle);
  if (egg.romicon) free(egg.romicon);
}

/* With egg.romr populated, read metadata:1 and record whatever we need.
 */
 
static int egg_native_rom_load_iconImage(const char *v,int vc) {
  int rid;
  if ((sr_int_eval(&rid,v,vc)<2)||(rid<1)||(rid>0xffff)) return 0;
  const void *serial=0;
  int serialc=romr_get(&serial,&egg.romr,EGG_TID_image,rid);
  if (serialc<1) return 0;
  struct rawimg *rawimg=rawimg_decode(serial,serialc);
  if (!rawimg) return 0;
  if (rawimg_force_rgba(rawimg)>=0) {
    if (rawimg->stride==rawimg->w<<2) {
      if (egg.romicon) free(egg.romicon);
      egg.romicon=rawimg->v;
      rawimg->v=0;
      egg.romiconw=rawimg->w;
      egg.romiconh=rawimg->h;
    }
  }
  rawimg_del(rawimg);
  return 0;
}
 
static int egg_native_rom_metadata_1(const char *k,int kc,const char *v,int vc) {
  // See etc/doc/metadata-resource.md for keys. I believe I've called out everything here that ought to be handled at runtime.
  
  if ((kc==5)&&!memcmp(k,"title",5)) {
    char *nv=malloc(vc+1);
    if (!nv) return -1;
    memcpy(nv,v,vc);
    nv[vc]=0;
    if (egg.romtitle) free(egg.romtitle);
    egg.romtitle=nv;
    return 0;
  }
  
  if ((kc==8)&&!memcmp(k,"titleStr",8)) {
    // Decimal rid of a string resource containing the translatable title. Better for displaying to user.
    //TODO?
    return 0;
  }
  
  if ((kc==9)&&!memcmp(k,"iconImage",9)) {
    return egg_native_rom_load_iconImage(v,vc);
  }
  
  if ((kc==11)&&!memcmp(k,"framebuffer",11)) {
    int vp=0,w=0,h=0;
    while ((vp<vc)&&(v[vp]>='0')&&(v[vp]<='9')) {
      int digit=v[vp++]-'0';
      w*=10;
      w+=digit;
    }
    if ((w<1)||(vp>=vc)||(v[vp++]!='x')) return 0;
    while ((vp<vc)&&(v[vp]>='0')&&(v[vp]<='9')) {
      int digit=v[vp++]-'0';
      h*=10;
      h+=digit;
    }
    if ((h<1)||(vp<vc)) return 0;
    egg.romfbw=w;
    egg.romfbh=h;
    return 0;
  }
  
  if ((kc==8)&&!memcmp(k,"required",8)) {
    //TODO Examine tokens and... fail? At least issue a warning if the game asks for something we can't supply.
    return 0;
  }
  
  if ((kc==8)&&!memcmp(k,"optional",8)) {
    //TODO Not quite sure how we're going to use this. Maybe it's a signal to turn on off-by-default features?
    return 0;
  }
  
  return 0;
}
 
static int egg_native_rom_load_metadata() {
  const uint8_t *src=0;
  int srcc=romr_get(&src,&egg.romr,EGG_TID_metadata,1);
  if (srcc<=0) {
    fprintf(stderr,"%s: No metadata.\n",egg.rompath);
  } else {
    int srcp=0,stopp=srcc-2;
    while (srcp<=stopp) {
      int kc=src[srcp++];
      int vc=src[srcp++];
      const char *k=(char*)src+srcp;
      if (srcp>srcc-kc) break;
      srcp+=kc;
      const char *v=(char*)src+srcp;
      if (srcp>srcc-vc) break;
      srcp+=vc;
      int err=egg_native_rom_metadata_1(k,kc,v,vc);
      if (err<0) return err;
    }
  }
  return 0;
}

/* Load ROM from file.
 */
#if EGG_ROM_SOURCE==EXTERNAL

static int egg_native_rom_load_file() {
  if (!egg.rompath||!egg.rompath[0]) {
    fprintf(stderr,"%s: ROM file required.\n",egg.exename);
    return -2;
  }
  if ((egg.romserialc=file_read(&egg.romserial,egg.rompath))<0) {
    egg.romserialc=0;
    fprintf(stderr,"%s: Failed to read file.\n",egg.rompath);
    return -2;
  }
  if (romr_decode_borrow(&egg.romr,egg.romserial,egg.romserialc)<0) {
    fprintf(stderr,"%s: Invalid ROM file.\n",egg.rompath);
    return -2;
  }
  fprintf(stderr,"%s: Decoded ROM file '%s'.\n",egg.exename,egg.rompath);
  return 0;
}

#else

static int egg_native_rom_load_embedded() {
  if (romr_decode_borrow(&egg.romr,egg_bundled_rom,egg_bundled_rom_length)<0) {
    fprintf(stderr,"%s: Invalid bundled ROM.\n",egg.exename);
    return -2;
  }
  return 0;
}

#endif

/* Init.
 * Configuration is complete, but runtimes and drivers have not been initialized yet.
 */
 
int egg_native_rom_init() {
  int err;
  
  #if (EGG_ROM_SOURCE==NATIVE)||(EGG_ROM_SOURCE==BUNDLED)
    if ((err=egg_native_rom_load_embedded())<0) return err;
  #else
    if ((err=egg_native_rom_load_file())<0) return err;
  #endif
  
  if ((err=egg_native_rom_load_metadata())<0) return err;
  
  return 0;
}

/* Initialize client hooks.
 */
 
static void egg_native_init_function(struct egg_function_location *loc,int fnid,const char *name) {
  loc->modid=1;
  loc->fnid=fnid;
  if (wamr_link_function(egg.wamr,loc->modid,loc->fnid,name)>=0) {
    loc->tid=EGG_TID_wasm;
    return;
  }
  if (qjs_link_function(egg.qjs,loc->modid,loc->fnid,name)>=0) {
    loc->tid=EGG_TID_js;
    return;
  }
  loc->tid=0;
}
 
int egg_native_init_client_code() {
  #if EGG_ROM_SOURCE==NATIVE
    // Nothing to initialize.
  #else
    const char *refname=egg.rompath?egg.rompath:egg.exename;
    void *serial;
    int serialc;
    if ((serialc=romr_get(&serial,&egg.romr,EGG_TID_wasm,1))>0) {
      if (wamr_add_module(egg.wamr,1,serial,serialc,refname)<0) {
        fprintf(stderr,"%s: Error loading wasm:1\n",refname);
        return -2;
      }
    }
    if ((serialc=romr_get(&serial,&egg.romr,EGG_TID_js,1))>0) {
      if (qjs_add_module(egg.qjs,1,serial,serialc,refname)<0) {
        fprintf(stderr,"%s: Error loading js:1\n",refname);
        return -2;
      }
    }
    egg_native_init_function(&egg.loc_client_init,1,"egg_client_init");
    egg_native_init_function(&egg.loc_client_quit,2,"egg_client_quit");
    egg_native_init_function(&egg.loc_client_update,3,"egg_client_update");
    egg_native_init_function(&egg.loc_client_render,4,"egg_client_render");
    if (!egg.loc_client_update.tid&&!egg.loc_client_render.tid) {
      fprintf(stderr,"%s: ROM file does not implement egg_client_update or egg_client_render.\n",refname);
      return -2;
    }
    //TODO Should we load all of the wasm and js resources, or just rid 1?
  #endif
  return 0;
}

/* Calls to client.
 */
 
#if EGG_ROM_SOURCE==NATIVE

int egg_native_call_client_init() { return egg_client_init(); }
void egg_native_call_client_quit() { egg_client_quit(); }
int egg_native_call_client_update(double elapsed) { egg_client_update(elapsed); return 0; }
int egg_native_call_client_render() { egg_client_render(); return 0; }
int egg_native_vm_update() { return 0; }

#else
 
int egg_native_call_client_init() {
  int err=0;
  uint32_t argv[1]={0};
  switch (egg.loc_client_init.tid) {
    case EGG_TID_wasm: err=wamr_call(egg.wamr,egg.loc_client_init.modid,egg.loc_client_init.fnid,argv,1); break;
    case EGG_TID_js: err=qjs_call(egg.qjs,egg.loc_client_init.modid,egg.loc_client_init.fnid,argv,1); break;
  }
  if ((err<0)||(argv[0]&0x80000000)) {
    fprintf(stderr,"%s: Error in game at egg_client_init\n",egg.exename);
    return -2;
  }
  return 0;
}

void egg_native_call_client_quit() {
  uint32_t argv[1]={0};
  switch (egg.loc_client_quit.tid) {
    case EGG_TID_wasm: wamr_call(egg.wamr,egg.loc_client_quit.modid,egg.loc_client_quit.fnid,argv,1); break;
    case EGG_TID_js: qjs_call(egg.qjs,egg.loc_client_quit.modid,egg.loc_client_quit.fnid,argv,1); break;
  }
}

int egg_native_call_client_update(double elapsed) {
  int err=0;
  uint32_t argv[2]={0};
  memcpy(argv,&elapsed,8);
  switch (egg.loc_client_update.tid) {
    case EGG_TID_wasm: err=wamr_call(egg.wamr,egg.loc_client_update.modid,egg.loc_client_update.fnid,argv,2); break;
    case EGG_TID_js: err=qjs_callf(egg.qjs,egg.loc_client_update.modid,egg.loc_client_update.fnid,"f",elapsed); break;
  }
  return err;
}

int egg_native_call_client_render() {
  int err=0;
  uint32_t argv[1]={0};
  switch (egg.loc_client_render.tid) {
    case EGG_TID_wasm: err=wamr_call(egg.wamr,egg.loc_client_render.modid,egg.loc_client_render.fnid,argv,1); break;
    case EGG_TID_js: err=qjs_call(egg.qjs,egg.loc_client_render.modid,egg.loc_client_render.fnid,argv,1); break;
  }
  return err;
}

int egg_native_vm_update() {
  return qjs_update(egg.qjs);
}

#endif
