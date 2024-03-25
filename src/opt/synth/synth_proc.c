#include "synth_internal.h"

/* Init.
 */
 
void synth_proc_init(struct synth *synth,struct synth_proc *proc) {
  proc->chid=0xff;
  proc->origin=0;
  proc->birthday=synth->framec;
}
