#include "eggrom_internal.h"

#include "opt/pcmprintc/pcmprintc.h"

#define EGGROM_HINT_PCMPRINT 1 /* For precompiled pcmprint (compiled at expansion). */

/* Reencode sound if necessary.
 */
 
int eggrom_sound_compile(struct sr_encoder *dst,const struct romw_res *res) {

  // Pcmprint binary, that we caught at expansion?
  if (res->hint==EGGROM_HINT_PCMPRINT) return 0;
  
  // WAV?
  if ((res->serialc>=12)&&!memcmp(res->serial,"RIFF",4)&&!memcmp((char*)res->serial+8,"WAVE",4)) return 0;
  
  // Anything else must be a single pcmprint text sound.
  struct pcmprintc *ppc=pcmprintc_new(res->serial,res->serialc,res->path,res->lineno);
  if (!ppc) return -1;
  struct pcmprintc_output output;
  int err=pcmprintc_next_final(&output,ppc);
  if (err<0) {
    pcmprintc_del(ppc);
    return err;
  }
  if (sr_encode_raw(dst,output.bin,output.binc)<0) {
    pcmprintc_del(ppc);
    return -1;
  }
  pcmprintc_del(ppc);

  return -1;
}

/* Expand file.
 */
 
int eggrom_sound_expand(const char *src,int srcc,uint16_t qual,const char *path) {

  // A sound file with no ID should be a multi-resource pcmprint text file.
  // Nevertheless, do a cursory signature check to make sure we don't try to parse WAV files as pcmprint.
  // Shouldn't do any harm if we did, just would lead to confusing error messages.
  if ((srcc>=12)&&!memcmp(src,"RIFF",4)&&!memcmp(src+8,"WAVE",4)) return 0;
  
  struct pcmprintc *ppc=pcmprintc_new(src,srcc,path,0);
  if (!ppc) return -1;
  int err=0;
  
  struct pcmprintc_output output;
  while ((err=pcmprintc_next(&output,ppc))>0) {
    struct romw_res *res=romw_res_add(eggrom.romw);
    if (!res) { err=-1; break; }
    res->tid=EGG_TID_sound;
    res->qual=qual;
    res->rid=output.idn;
    romw_res_set_path(res,path,-1);
    romw_res_set_name(res,output.id,output.idc);
    res->hint=EGGROM_HINT_PCMPRINT;
    if ((err=romw_res_set_serial(res,output.bin,output.binc))<0) break;
  }
  
  pcmprintc_del(ppc);
  if ((err<0)&&(err!=-2)) {
    fprintf(stderr,"%s: Unspecified error compiling pcmprint sounds.\n",path);
    return -2;
  }
  return err;
}
