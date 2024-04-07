#include "sfg_internal.h"

/* PCM dump.
 */
 
void sfg_pcm_del(struct sfg_pcm *pcm) {
  if (!pcm) return;
  if (pcm->refc-->1) return;
  free(pcm);
}

int sfg_pcm_ref(struct sfg_pcm *pcm) {
  if (!pcm) return -1;
  if (pcm->refc<1) return -1;
  if (pcm->refc==INT_MAX) return -1;
  pcm->refc++;
  return 0;
}

struct sfg_pcm *sfg_pcm_new(int c) {
  if (c<1) return 0;
  if (c>(INT_MAX-sizeof(struct sfg_pcm))/sizeof(float)) return 0;
  struct sfg_pcm *pcm=calloc(1,sizeof(struct sfg_pcm)+c*sizeof(float));
  if (!pcm) return 0;
  pcm->refc=1;
  pcm->c=c;
  return pcm;
}

/* Printer object.
 */
 
static void sfg_env_cleanup(struct sfg_env *env) {
  if (env->pointv) free(env->pointv);
}
 
static void sfg_op_cleanup(struct sfg_op *op) {
  sfg_env_cleanup(&op->env);
  if (op->buf) free(op->buf);
}
 
void sfg_voice_cleanup(struct sfg_voice *voice) {
  if (voice->opv) {
    while (voice->opc-->0) sfg_op_cleanup(voice->opv+voice->opc);
    free(voice->opv);
  }
  sfg_env_cleanup(&voice->rate);
  sfg_env_cleanup(&voice->range);
}

void sfg_printer_del(struct sfg_printer *printer) {
  if (!printer) return;
  sfg_pcm_del(printer->pcm);
  if (printer->voicev) {
    while (printer->voicec-->0) sfg_voice_cleanup(printer->voicev+printer->voicec);
    free(printer->voicev);
  }
  free(printer);
}

struct sfg_printer *sfg_printer_new(int rate,const void *bin,int binc) {
  if ((rate<200)||(rate>200000)) return 0;
  struct sfg_printer *printer=calloc(1,sizeof(struct sfg_printer));
  if (!printer) return 0;
  printer->rate=rate;
  if ((sfg_printer_decode(printer,bin,binc)<0)||!printer->pcm) {
    sfg_printer_del(printer);
    return 0;
  }
  return printer;
}

struct sfg_pcm *sfg_printer_get_pcm(const struct sfg_printer *printer) {
  if (!printer) return 0;
  return printer->pcm;
}
