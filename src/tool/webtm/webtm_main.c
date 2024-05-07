#include "webtm_internal.h"

struct webtm webtm={0};

/* Acquire configuration.
 */
 
static int webtm_configure(int argc,char **argv) {
  webtm.exename=((argc>=1)&&argv&&argv[0]&&argv[0][0])?argv[0]:"webtm";
  int i=1; for (;i<argc;i++) {
    const char *arg=argv[i];
    
    if (!strcmp(arg,"--help")) {
      fprintf(stderr,
        "Usage: %s -oOUTPUT_HTML --js=src/web/egg.js --html=src/web/tm.html\n"
        "   Or: %s -oOUTPUT_HTML --rom=.../myrom.egg --html=out/web/egg-headless.html\n"
      ,webtm.exename,webtm.exename);
      return -2;
    }
    
    if (!memcmp(arg,"-o",2)) {
      if (webtm.dstpath) {
        fprintf(stderr,"%s: Multiple output paths.\n",webtm.exename);
        return -2;
      }
      webtm.dstpath=arg+2;
      continue;
    }
    
    if (!memcmp(arg,"--js=",5)) {
      if (webtm.jspath) {
        fprintf(stderr,"%s: Multiple Javascript bootstraps.\n",webtm.exename);
        return -2;
      }
      webtm.jspath=arg+5;
      continue;
    }
    
    if (!memcmp(arg,"--html=",7)) {
      if (webtm.htmlpath) {
        fprintf(stderr,"%s: Multiple HTML templates.\n",webtm.exename);
        return -2;
      }
      webtm.htmlpath=arg+7;
      continue;
    }
    
    if (!memcmp(arg,"--rom=",6)) {
      if (webtm.rompath) {
        fprintf(stderr,"%s: Multiple ROM inputs.\n",webtm.exename);
        return -2;
      }
      webtm.rompath=arg+6;
      continue;
    }
    
    fprintf(stderr,"%s: Unexpected argument '%s'.\n",webtm.exename,arg);
    return -2;
  }
  if (!webtm.dstpath) {
    fprintf(stderr,"%s: Please specify output path '-oPATH'\n",webtm.exename);
    return -2;
  }
  if (!webtm.htmlpath) {
    fprintf(stderr,"%s: Please specify HTML template '--html=PATH'\n",webtm.exename);
    return -2;
  }
  if (webtm.jspath&&webtm.rompath) {
    fprintf(stderr,"%s: '--rom=' and '--js=' are mutually exclusive. '--js=' to generate the template, or '--rom=' to apply it.\n",webtm.exename);
    return -2;
  }
  if (!webtm.jspath&&!webtm.rompath) {
    fprintf(stderr,"%s: Please specify Javascript bootstrap '--js=PATH' or ROM file '--rom=PATH'\n",webtm.exename);
    return -2;
  }
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  int err;
  if ((err=webtm_configure(argc,argv))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error reading configuration.\n",webtm.exename);
    return 1;
  }
  if ((webtm.htmlsrcc=file_read(&webtm.htmlsrc,webtm.htmlpath))<0) {
    fprintf(stderr,"%s: Failed to read file.\n",webtm.htmlpath);
    return 1;
  }
  if (webtm.jspath) {
    if ((err=webtm_js_compile(&webtm.jssrc,webtm.jspath))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error processing Javascript.\n",webtm.jspath);
      return 1;
    }
    if ((err=webtm_compose_template(&webtm.dst,webtm.htmlsrc,webtm.htmlsrcc,webtm.jssrc.v,webtm.jssrc.c))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error composing HTML.\n",webtm.exename);
      return 1;
    }
  } else {
    if ((webtm.romc=file_read(&webtm.rom,webtm.rompath))<0) {
      fprintf(stderr,"%s: Failed to read file.\n",webtm.rompath);
      return 1;
    }
    if ((err=webtm_compose_bundle(&webtm.dst,webtm.htmlsrc,webtm.htmlsrcc,webtm.rom,webtm.romc))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error composing HTML.\n",webtm.exename);
      return 1;
    }
  }
  if (file_write(webtm.dstpath,webtm.dst.v,webtm.dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",webtm.dstpath,webtm.dst.c);
    return 1;
  }
  return 0;
}
