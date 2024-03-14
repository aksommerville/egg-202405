#include "egg_native_internal.h"
#include "opt/serial/serial.h"

/* Cleanup.
 */
 
void egg_configure_cleanup() {
  if (egg.video_driver) free(egg.video_driver);
  if (egg.video_device) free(egg.video_device);
  if (egg.audio_driver) free(egg.audio_driver);
  if (egg.audio_device) free(egg.audio_device);
  if (egg.input_driver) free(egg.input_driver);
  if (egg.input_device) free(egg.input_device);
  if (egg.rompath) free(egg.rompath);
  if (egg.storepath) free(egg.storepath);
}

/* Set string field.
 */
 
static int egg_native_configure_set_string(char **dstpp,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (*dstpp) free(*dstpp);
  *dstpp=nv;
  return 0;
}

/* --help
 */
 
static void egg_native_configure_print_help(const char *topic,int topicc) {
  if (egg_native_uses_rom_file()) {
    fprintf(stderr,"\nUsage: %s [OPTIONS] ROMFILE\n\n",egg.exename);
  } else {
    fprintf(stderr,"\nUsage: %s [OPTIONS]\n\n",egg.exename);
  }
  fprintf(stderr,
    "In general, no options should be provided, and we'll pick sensible defaults.\n"
    "OPTIONS:\n"
    "  --help                   Print this message and exit.\n"
    "  --video-driver=LIST      Video drivers in order of preference, see below.\n"
    "  --video-device=NAME      If required by driver.\n"
    "  --fullscreen=BOOLEAN     Start in fullscreen mode.\n"
    "  --video-size=WxH         Initial window size, if supported.\n"
    "  --audio-driver=LIST      Audio drivers in order of preference, see below.\n"
    "  --audio-device=NAME      If required by driver.\n"
    "  --audio-rate=HZ          Suggest audio output rate.\n"
    "  --audio-chanc=1|2        Suggest mono or stereo.\n"
    "  --audio-buffer=BYTES     Suggest audio buffer size.\n"
    "  --input-driver=LIST      Input drivers. Will try to load all. See below.\n"
    "  --input-device=NAME      If required by driver.\n"
    "  --store=PATH             File for persistent data, per-game.\n"
    "\n"
  );
  {
    fprintf(stderr,"Video drivers:\n");
    int p=0; for (;;p++) {
      const struct hostio_video_type *type=hostio_video_type_by_index(p);
      if (!type) break;
      fprintf(stderr,"  %10s : %s\n",type->name,type->desc);
    }
    fprintf(stderr,"\n");
  }
  {
    fprintf(stderr,"Audio drivers:\n");
    int p=0; for (;;p++) {
      const struct hostio_audio_type *type=hostio_audio_type_by_index(p);
      if (!type) break;
      fprintf(stderr,"  %10s : %s\n",type->name,type->desc);
    }
    fprintf(stderr,"\n");
  }
  {
    fprintf(stderr,"Input drivers:\n");
    int p=0; for (;;p++) {
      const struct hostio_input_type *type=hostio_input_type_by_index(p);
      if (!type) break;
      fprintf(stderr,"  %10s : %s\n",type->name,type->desc);
    }
    fprintf(stderr,"\n");
  }
}

/* Receive config field.
 */
 
static int egg_native_configure_kv(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  // When (v) is null, not just empty, value becomes "1" or "0" and we strip "no-" from key.
  if (!v) {
    if ((kc>3)&&!memcmp(k,"no-",3)) {
      k+=3;
      kc-=3;
      v="0";
      vc=1;
    } else {
      v="1";
      vc=1;
    }
  }
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    egg_native_configure_print_help(v,vc);
    egg.terminate=1;
    return 0;
  }
  
  if ((kc==10)&&!memcmp(k,"video-size",10)) {
    int w=0,h=0,vp=0;
    while ((vp<vc)&&(v[vp]>='0')&&(v[vp]<='9')) {
      w*=10;
      w+=v[vp++]-'0';
    }
    if ((vp<vc)&&(v[vp++]=='x')) {
      while ((vp<vc)&&(v[vp]>='0')&&(v[vp]<='9')) {
        h*=10;
        h+=v[vp++]-'0';
      }
    }
    if ((vp<vc)||(w<1)||(h<1)) {
      fprintf(stderr,"%s: Expected 'WxH', found '%.*s'\n",egg.exename,vc,v);
      return -2;
    }
    egg.video_w=w;
    egg.video_h=h;
    return 0;
  }
  
  #define STROPT(arg,fld) if ((kc==sizeof(#arg)-1)&&!memcmp(k,#arg,kc)) { \
    return egg_native_configure_set_string(&egg.fld,v,vc); \
  }
  #define INTOPT(arg,fld) if ((kc==sizeof(#arg)-1)&&!memcmp(k,#arg,kc)) { \
    int n; \
    if (sr_int_eval(&n,v,vc)<2) { \
      fprintf(stderr,"%s: Expected integer for '%.*s', found '%.*s'\n",egg.exename,kc,k,vc,v); \
      return -2; \
    } \
    egg.fld=n; \
    return 0; \
  }
  #define BOOLOPT(arg,fld) if ((kc==sizeof(#arg)-1)&&!memcmp(k,#arg,kc)) { \
    int n; \
    if (sr_bool_eval(&n,v,vc)<0) { \
      fprintf(stderr,"%s: Expected boolean for '%.*s', found '%.*s'\n",egg.exename,kc,k,vc,v); \
      return -2; \
    } \
    egg.fld=n; \
    return 0; \
  }
  
  STROPT("video-driver",video_driver)
  STROPT("video-device",video_device)
  BOOLOPT("fullscreen",fullscreen)
  STROPT("audio-driver",audio_driver)
  STROPT("audio-device",audio_device)
  INTOPT("audio-rate",audio_rate)
  INTOPT("audio-chanc",audio_chanc)
  INTOPT("audio-buffer",audio_buffer)
  STROPT("input-driver",input_driver)
  STROPT("input-device",input_device)
  STROPT("store",storepath)
  
  #undef STROPT
  #undef INTOPT
  #undef BOOLOPT
  
  fprintf(stderr,"%s: Unexpected argument '%.*s' = '%.*s'. Try '--help'.\n",egg.exename,kc,k,vc,v);
  return -2;
}

/* Receive argv.
 */
 
static int egg_native_configure_argv(int argc,char **argv) {
  int argi=1,err;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    // No dash: ROM path, if we're built to accept one.
    if (arg[0]!='-') {
      if (egg_native_uses_rom_file()) {
        if (egg.rompath) {
          fprintf(stderr,"%s: Multiple ROM files.\n",egg.exename);
          return -2;
        }
        if (egg_native_configure_set_string(&egg.rompath,arg,-1)<0) return -1;
        continue;
      } else {
        fprintf(stderr,"%s: Unexpected argument '%s'\n",egg.exename,arg);
        return -2;
      }
    }
    
    // Single dash alone. Reserved.
    if (!arg[1]) {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",egg.exename,arg);
      return -2;
    }
    
    // Short options: "-kVV" or "-k VV"
    if (arg[1]!='-') {
      char k=arg[1];
      const char *v=0;
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if ((err=egg_native_configure_kv(&k,1,v,-1))<0) return err;
      continue;
    }
    
    // Double dash alone. Reserved.
    if (!arg[2]) {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",egg.exename,arg);
      return -2;
    }
    
    // Long options: "--kk=VV" or "--kk VV"
    const char *k=arg+2,*v=0;
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    if (k[kc]=='=') v=k+kc+1;
    else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
    if ((err=egg_native_configure_kv(k,kc,v,-1))<0) return err;
  }
  return 0;
}

/* Determine a default storepath.
 */
 
static int egg_native_configure_default_storepath() {
  const char *refpath;
  if (egg.rompath&&egg_native_uses_rom_file()) {
    refpath=egg.rompath;
  } else {
    refpath=egg.exename;
  }
  int refpathc=0;
  while (refpath[refpathc]) refpathc++;
  int refsepp=path_split(refpath,refpathc);
  int dirlen=refsepp+1;
  if (dirlen<0) dirlen=0;
  const char *refbase=refpath+dirlen;
  int refbasec=refpathc-dirlen;
  int refstemc=0;
  while ((refstemc<refbasec)&&(refbase[refstemc]!='.')) refstemc++;
  const char *sfx=".save";
  int sfxc=5;
  int storepathc=dirlen+refstemc+sfxc;
  if (!(egg.storepath=malloc(storepathc+1))) return -1;
  memcpy(egg.storepath,refpath,dirlen+refstemc);
  memcpy(egg.storepath+dirlen+refstemc,sfx,sfxc);
  egg.storepath[storepathc]=0;
  return 0;
}

/* Done reading configuration. Validate or set defaults.
 */
 
static int egg_native_configure_finish() {
  int err;

  if (!egg.storepath) {
    if ((err=egg_native_configure_default_storepath())<0) return err;
  }
  
  return 0;
}

/* Configure, main entry point.
 */
 
int egg_native_configure(int argc,char **argv) {
  int err;
  egg.exename="egg";
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) egg.exename=argv[0];
  //TODO environment? config files?
  if ((err=egg_native_configure_argv(argc,argv))<0) return err;
  return egg_native_configure_finish();
}
