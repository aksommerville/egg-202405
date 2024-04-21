#include "eggrom_internal.h"

struct eggrom eggrom={0};

/* -c input
 */
 
static int eggrom_c_input_path(const char *path,char ftype,uint8_t tid);
 
static int eggrom_c_input_file(const char *path,uint8_t tid) {
  struct romw_res *res=romw_res_add(eggrom.romw);
  if (!res) return -1;
  if ((res->serialc=file_read(&res->serial,path))<0) {
    res->serialc=0;
    fprintf(stderr,"%s: Failed to read file.\n",path);
    return -2;
  }
  if (romw_res_set_path(res,path,-1)<0) return -1;
  res->tid=tid;
  int err=eggrom_infer_ids(res);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error inferring resource IDs.\n",path);
    return -2;
  }
  return 0;
}

static int eggrom_c_input_dir_cb(const char *path,const char *base,char ftype,void *userdata) {
  uint8_t tid=*(uint8_t*)userdata;
  int err=eggrom_tid_eval(base,-1,1);
  if (err>0) tid=err;
  return eggrom_c_input_path(path,ftype,tid);
}

static int eggrom_c_input_dir(const char *path,uint8_t tid) {
  return dir_read(path,eggrom_c_input_dir_cb,&tid);
}
 
static int eggrom_c_input_path(const char *path,char ftype,uint8_t tid) {
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='f') return eggrom_c_input_file(path,tid);
  if (ftype=='d') return eggrom_c_input_dir(path,tid);
  return 0;
}
 
static int eggrom_c_input(const char *path) {
  if (!eggrom.romw) {
    if (!(eggrom.romw=romw_new())) return -1;
  }
  int err=eggrom_c_input_path(path,0,0);
  if (err<0) return err;
  return 0;
}

/* -c output
 */
 
static int eggrom_c_finish() {
  int err;
  if (!eggrom.dstpath) {
    fprintf(stderr,"%s: Please specify output path as '-oPATH'\n",eggrom.exename);
    return -2;
  }
  if (!eggrom.romw) { // No inputs. That's legal, actually, maybe they want just an empty archive.
    if (!(eggrom.romw=romw_new())) return -1;
  }
  
  /* Resource processing.
   */
  if ((err=eggrom_js_combine())<0) return err;
  if ((err=eggrom_expand_resources())<0) return err;
  if ((err=eggrom_assign_missing_ids())<0) return err;
  if ((err=eggrom_link_resources())<0) return err;
  
  /* Sort, validate, encode, all generic from here out.
   */
  romw_sort(eggrom.romw);
  if (romw_validate_preencode(eggrom.romw)<0) {
    return -2; // romw_validate_preencode logs all errors
  }
  struct sr_encoder serial={0};
  if (romw_encode(&serial,eggrom.romw)<0) {
    fprintf(stderr,"%s: Unspecified error encoding ROM file.\n",eggrom.dstpath);
    sr_encoder_cleanup(&serial);
    return -2;
  }
  err=file_write(eggrom.dstpath,serial.v,serial.c);
  sr_encoder_cleanup(&serial);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",eggrom.dstpath,serial.c);
    return -2;
  }
  return 0;
}

/* -x input
 */
 
static int eggrom_x_1(uint8_t tid,uint16_t qual,uint16_t rid,const void *v,int c,void *userdata) {
  char dstpath[1024];
  int dstpathc=snprintf(dstpath,sizeof(dstpath),"%s",eggrom.dstpath);
  if ((dstpathc<1)||(dstpathc>=sizeof(dstpath))) return -1;
  dstpath[dstpathc++]='/';

  int err=eggrom_tid_repr(dstpath+dstpathc,sizeof(dstpath)-dstpathc,tid);
  if ((err<1)||(dstpathc>=sizeof(dstpath)-err)) return -1;
  dstpathc+=err;
  dstpath[dstpathc++]='/';
  
  if (qual) {
    err=eggrom_qual_repr(dstpath+dstpathc,sizeof(dstpath)-dstpathc,tid,qual);
    if ((err<1)||(dstpathc>=sizeof(dstpath)-err)) return -1;
    dstpathc+=err;
    dstpath[dstpathc++]='/';
  }
  
  err=sr_decuint_repr(dstpath+dstpathc,sizeof(dstpath)-dstpathc,rid,0);
  if ((err<1)||(dstpathc>=sizeof(dstpath)-err)) return -1;
  dstpathc+=err;
  dstpath[dstpathc]=0;
  
  if (dir_mkdirp_parent(dstpath)<0) {
    fprintf(stderr,"%s: Failed to create parent directories.\n",dstpath);
    return -2;
  }
  if (file_write(dstpath,v,c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",dstpath,c);
    return -2;
  }
  return 0;
}
 
static int eggrom_x_input(const char *path) {
  if (!eggrom.dstpath) {
    fprintf(stderr,"%s: Please provide '-oOUTPUT' before input path ('%s').\n",eggrom.exename,path);
    return -2;
  }
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) {
    fprintf(stderr,"%s: Error reading file.\n",path);
    return -2;
  }
  int err=romr_for_each(serial,serialc,eggrom_x_1,0);
  free(serial);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error extracting ROM.\n",eggrom.exename);
    return -2;
  }
  return 0;
}

/* -x output
 */
 
static int eggrom_x_finish() {
  // Nothing to do. Output was produced at input reception.
  return 0;
}

/* -t input
 */
 
static int eggrom_t_1(uint8_t tid,uint16_t qual,uint16_t rid,const void *v,int c,void *userdata) {
  if (!c) return 0;
  switch (eggrom.format) {
  
    case 0: {
        char tidstr[16];
        int tidstrc=eggrom_tid_repr(tidstr,sizeof(tidstr),tid);
        if ((tidstrc<1)||(tidstrc>sizeof(tidstr))) {
          tidstrc=sr_decuint_repr(tidstr,sizeof(tidstr),tid,0);
        }
        char qualstr[16];
        int qualstrc=eggrom_qual_repr(qualstr,sizeof(qualstr),tid,qual);
        if ((qualstrc<1)||(qualstrc>sizeof(qualstr))) {
          qualstrc=sr_decuint_repr(qualstr,sizeof(qualstr),qual,0);
        }
        fprintf(stdout,"  %.*s:%.*s:%d: %d bytes\n",tidstrc,tidstr,qualstrc,qualstr,rid,c);
      } break;
      
    case 'n': {
        fprintf(stdout,"  %d:%d:%d: %d bytes\n",tid,qual,rid,c);
      } break;
      
    case 'm': {
        fprintf(stdout,"%d %d %d %d\n",tid,qual,rid,c);
      } break;
      
    case 'j': {
        struct sr_encoder encoder={0};
        sr_encode_json_object_start(&encoder,0,0);
        sr_encode_json_int(&encoder,"tid",3,tid);
        sr_encode_json_int(&encoder,"qual",4,qual);
        sr_encode_json_int(&encoder,"rid",3,rid);
        sr_encode_json_int(&encoder,"len",3,c);
        if (sr_encode_json_end(&encoder,0)<0) {
          sr_encoder_cleanup(&encoder);
          return -1;
        }
        fprintf(stdout,"%.*s,\n",encoder.c,(char*)encoder.v);
        sr_encoder_cleanup(&encoder);
      } break;
      
    default: return -1;
  }
  return 0;
}
 
static int eggrom_t_input(const char *path) {
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",path);
    return -2;
  }
  switch (eggrom.format) {
    case 0:
    case 'n': {
        fprintf(stdout,"%s: Decoding %d-byte file...\n",path,serialc);
      } break;
    case 'j': {
        fprintf(stdout,"[\n");
      } break;
    case 'm': break; // no header
  }
  if (romr_for_each(serial,serialc,eggrom_t_1,0)<0) {
    fprintf(stderr,"%s: ERROR!\n",path);
  }
  switch (eggrom.format) {
    case 'j': {
        // We've emitted an illegal trailing comma. Could add a dummy expression here to make it legal... Does anyone care?
        fprintf(stdout,"]\n");
      } break;
  }
  free(serial);
  return 0;
}

/* -t output
 */
 
static int eggrom_t_finish() {
  // Nothing to do. We dumped inputs as they arrived.
  return 0;
}

/* Receive input path from command line.
 */
 
static int eggrom_process_input(const char *path) {
  switch (eggrom.command) {
    case 0: {
        fprintf(stderr,"%s: Please specify command (-c, -x, -t) before input ('%s')\n",eggrom.exename,path);
        return -2;
      } break;
    case 'c': return eggrom_c_input(path);
    case 'x': return eggrom_x_input(path);
    case 't': return eggrom_t_input(path);
    default: return -1;
  }
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  int err;
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) eggrom.exename=argv[0];
  else eggrom.exename="eggrom";
  int argi=1; for (;argi<argc;argi++) {
    const char *arg=argv[argi];
    if (!arg||!arg[0]) continue;
    
    // No dashes: Input path. Command must be specified first.
    if (arg[0]!='-') {
      if ((err=eggrom_process_input(arg))<0) {
        if (err!=-2) fprintf(stderr,"%s: Unspecified error processing input.\n",arg);
        return 1;
      }
      continue;
    }
    
    // Single dash alone, and more than one dash, not currently defined.
    if (!arg[1]||(arg[1]=='-')) {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",eggrom.exename,arg);
      return 1;
    }
    
    // Anything else is "-kVVV".
    switch (arg[1]) {
      case 'c':
      case 'x':
      case 't': {
          if (eggrom.command&&(eggrom.command!=arg[1])) {
            fprintf(stderr,"%s: Multiple commands ('%c' and '%c').\n",eggrom.exename,eggrom.command,arg[1]);
            return 1;
          }
          eggrom.command=arg[1];
        } break;
      case 'o': {
          if (eggrom.dstpath) {
            fprintf(stderr,"%s: Multiple output paths\n",eggrom.exename);
            return 1;
          }
          eggrom.dstpath=arg+2;
        } break;
      case 'f': { // Format for -t
               if (!strcmp(arg+2,"default")) eggrom.format=0;
          else if (!strcmp(arg+2,"machine")) eggrom.format='m';
          else if (!strcmp(arg+2,"json")) eggrom.format='j';
          else if (!strcmp(arg+2,"numeric")) eggrom.format='n';
          else {
            fprintf(stderr,"%s: Unknown table format '%s' (-fdefault,-fmachine,-fjson,-fnumeric)\n",eggrom.exename,arg);
            return 1;
          }
        } break;
      default: {
          fprintf(stderr,"%s: Unexpected argument '%s'\n",eggrom.exename,arg);
          return 1;
        }
    }
  }
  
  switch (eggrom.command) {
    case 0: {
        fprintf(stderr,"%s: Please specify a command: -c -x -t\n",eggrom.exename);
        return 1;
      }
    case 'c': err=eggrom_c_finish(); break;
    case 'x': err=eggrom_x_finish(); break;
    case 't': err=eggrom_t_finish(); break;
    default: {
        fprintf(stderr,"%s: Unimplemented command '%c'\n",eggrom.exename,eggrom.command);
        return 1;
      }
  }
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error.\n",eggrom.exename);
    return 1;
  }
  
  return 0;
}
