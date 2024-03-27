#include "pcmprint_internal.h"

/* PCM container object.
 */

void pcmprint_pcm_del(struct pcmprint_pcm *pcm) {
  if (!pcm) return;
  if (pcm->refc-->1) return;
  free(pcm);
}

int pcmprint_pcm_ref(struct pcmprint_pcm *pcm) {
  if (!pcm) return -1;
  if (pcm->refc<1) return -1;
  if (pcm->refc==INT_MAX) return -1;
  pcm->refc++;
  return 0;
}

struct pcmprint_pcm *pcmprint_pcm_new(int c) {
  if (c<1) return 0;
  if (c>(INT_MAX-sizeof(struct pcmprint_pcm))/sizeof(float)) return 0;
  struct pcmprint_pcm *pcm=calloc(1,sizeof(struct pcmprint_pcm)+sizeof(float)*c);
  if (!pcm) return 0;
  pcm->refc=1;
  pcm->c=c;
  return pcm;
}

/* Delete printer.
 */
 
static void pcmprint_cmd_cleanup(struct pcmprint_cmd *cmd) {
  pcmprint_env_cleanup(&cmd->env);
  if (cmd->buf) free(cmd->buf);
}
 
static void pcmprint_voice_cleanup(struct pcmprint_voice *voice) {
  if (voice->wave&&voice->ownwave) free(voice->wave);
  pcmprint_env_cleanup(&voice->rate);
  pcmprint_env_cleanup(&voice->fmrangeenv);
  if (voice->cmdv) {
    while (voice->cmdc-->0) pcmprint_cmd_cleanup(voice->cmdv+voice->cmdc);
    free(voice->cmdv);
  }
}

void pcmprint_del(struct pcmprint *pcmprint) {
  if (!pcmprint) return;
  pcmprint_pcm_del(pcmprint->pcm);
  if (pcmprint->voicev) {
    while (pcmprint->voicec-->0) pcmprint_voice_cleanup(pcmprint->voicev+pcmprint->voicec);
    free(pcmprint->voicev);
  }
  if (pcmprint->sine) free(pcmprint->sine);
  free(pcmprint);
}

/* New printer.
 */

struct pcmprint *pcmprint_new(int rate,const void *src,int srcc) {
  if ((rate<200)||(rate>200000)) return 0;
  if (!src) return 0;
  struct pcmprint *pcmprint=calloc(1,sizeof(struct pcmprint));
  if (!pcmprint) return 0;
  pcmprint->rate=rate;
  if (pcmprint_decode(pcmprint,src,srcc)<0) {
    pcmprint_del(pcmprint);
    return 0;
  }
  return pcmprint;
}

/* Trivial accessors.
 */

struct pcmprint_pcm *pcmprint_get_pcm(const struct pcmprint *pcmprint) {
  if (!pcmprint) return 0;
  return pcmprint->pcm;
}

/* Add voice.
 */
 
struct pcmprint_voice *pcmprint_voice_new(struct pcmprint *pcmprint) {
  if (pcmprint->voicec>=pcmprint->voicea) {
    int na=pcmprint->voicea+8;
    if (na>INT_MAX/sizeof(struct pcmprint_voice)) return 0;
    void *nv=realloc(pcmprint->voicev,sizeof(struct pcmprint_voice)*na);
    if (!nv) return 0;
    pcmprint->voicev=nv;
    pcmprint->voicea=na;
  }
  struct pcmprint_voice *voice=pcmprint->voicev+pcmprint->voicec++;
  memset(voice,0,sizeof(struct pcmprint_voice));
  return voice;
}

/* Generate sine wave if needed.
 */
 
int pcmprint_require_sine(struct pcmprint *pcmprint) {
  if (pcmprint->sine) return 0;
  if (!(pcmprint->sine=malloc(sizeof(float)*PCMPRINT_WAVE_SIZE_SAMPLES))) return -1;
  float *v=pcmprint->sine;
  int i=PCMPRINT_WAVE_SIZE_SAMPLES;
  float p=0.0f,dp=(M_PI*2.0f)/(float)PCMPRINT_WAVE_SIZE_SAMPLES;
  for (;i-->0;v++,p+=dp) *v=sinf(p);
  return 0;
}

/* Add command to voice.
 */
 
struct pcmprint_cmd *pcmprint_voice_add_cmd(struct pcmprint_voice *voice) {
  if (voice->cmdc>=voice->cmda) {
    int na=voice->cmda+8;
    if (na>INT_MAX/sizeof(struct pcmprint_cmd)) return 0;
    void *nv=realloc(voice->cmdv,sizeof(struct pcmprint_cmd)*na);
    if (!nv) return 0;
    voice->cmdv=nv;
    voice->cmda=na;
  }
  struct pcmprint_cmd *cmd=voice->cmdv+voice->cmdc++;
  memset(cmd,0,sizeof(struct pcmprint_cmd));
  return cmd;
}
