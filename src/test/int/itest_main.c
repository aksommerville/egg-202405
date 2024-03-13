#include "test/test.h"
#include "test/int/itest_toc.h"

/* Main.
 */
 
int main(int argc,char **argv) {
  const struct itest *itest=itestv;
  int i=sizeof(itestv)/sizeof(itestv[0]);
  for (;i-->0;itest++) {
    if (test_filter(itest->name,itest->tags,itest->if_unspecified)) {
      if (itest->fn()<0) {
        fprintf(stderr,"TEST FAIL %s [%s:%d%s]\n",itest->name,itest->file,itest->line,itest->tags);
      } else {
        fprintf(stderr,"TEST PASS %s [%s:%d%s]\n",itest->name,itest->file,itest->line,itest->tags);
      }
    } else {
      fprintf(stderr,"TEST SKIP %s [%s:%d%s]\n",itest->name,itest->file,itest->line,itest->tags);
    }
  }
  return 0;
}
