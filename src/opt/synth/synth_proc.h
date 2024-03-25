/* synth_proc.h
 * Attachment point for custom programs.
 * Anything with a global LFO or post-mix effects needs to use proc instead of voice.
 */
 
#ifndef SYNTH_PROC_H
#define SYNTH_PROC_H

struct synth_proc {
  uint8_t chid;
  uint8_t origin;
  int64_t birthday;
  void *userdata;
  void (*del)(struct synth_proc *proc);
  void (*update)(float *v,int c,struct synth *synth,struct synth_proc *proc);
  void (*release)(struct synth *synth,struct synth_proc *proc);
};

static inline void synth_proc_cleanup(struct synth_proc *proc) {
  if (proc->del) {
    proc->del(proc);
    proc->del=0;
  }
}

void synth_proc_init(struct synth *synth,struct synth_proc *proc);

static inline void synth_proc_release(struct synth *synth,struct synth_proc *proc) {
  if (proc->release) proc->release(synth,proc);
  else proc->update=0; // force defunct
}

static inline void synth_proc_update(float *v,int c,struct synth *synth,struct synth_proc *proc) {
  if (proc->update) proc->update(v,c,synth,proc);
}

static inline int synth_proc_is_channel(const struct synth_proc *proc,uint8_t chid) {
  return (proc->chid==chid);
}

static inline int synth_proc_is_song(const struct synth_proc *proc) {
  return (proc->origin==SYNTH_ORIGIN_SONG);
}

static inline int synth_proc_is_defunct(const struct synth_proc *proc) {
  return !proc->update;
}

static inline int synth_proc_compare(
  const struct synth_proc *a,
  const struct synth_proc *b
) {
  return a->birthday-b->birthday;
}

#endif
