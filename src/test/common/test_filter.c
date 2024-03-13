#include "string.h"
#include <stdlib.h>

/* Nonzero if token is present in space-delimited list.
 */
 
static int string_in_list(const char *token,int tokenc,const char *list) {
  if (!list) return 0;
  int listp=0;
  while (list[listp]) {
    if ((unsigned char)list[listp]<=0x20) { listp++; continue; }
    if (list[listp]==',') { listp++; continue; }
    const char *q=list+listp;
    int qc=0;
    while (((unsigned char)list[listp]>0x20)&&(list[listp]!=',')) { listp++; qc++; }
    
    if ((qc==tokenc)&&!memcmp(q,token,tokenc)) return 1;
  }
  return 0;
}

/* Apply filters.
 */
 
int test_filter(const char *fnname,const char *tags,int if_unspecified) {
  int fnnamec=0;
  if (fnname) while (fnname[fnnamec]) fnnamec++;
  const char *filter=getenv("TEST_FILTER");
  if (!filter) return if_unspecified;
  int filterp=0,filters_valid=0;
  while (filter[filterp]) {
    if ((unsigned char)filter[filterp]<=0x20) { filterp++; continue; }
    const char *filter1=filter+filterp;
    int filter1c=0;
    while ((unsigned char)filter[filterp]>0x20) { filter1c++; filterp++; }
    
    if ((filter1c==fnnamec)&&!memcmp(fnname,filter1,fnnamec)) return 1;
    if (string_in_list(filter1,filter1c,tags)) return 1;
    filters_valid=1;
  }
  if (filters_valid) return 0;
  return if_unspecified;
}
