/* strfmt.h
 * Poor man's sprintf, for freestanding environments or odd situations
 * where you don't actually have a va_list of inputs.
 */
 
#ifndef STRFMT_H
#define STRFMT_H

#include <stdint.h>

/* Initialize (fmt) and zero the rest.
 * No cleanup necessary.
 */
struct strfmt {

  const char *fmt;

  /* When this is not zero, you must call strfmt_provide_* accordingly:
   */
  char awaiting;
  
  // Private.
  int state,align,len,prec;
  int manprec;
  union {
    int i;
    int64_t l;
    double f;
    const void *p;
  } v;
};

int strfmt_is_finished(const struct strfmt *strfmt);

/* Produce some output and return its length.
 * May return zero if we're stalled due to (strfmt->awaiting).
 * Never returns >dsta. We will break it up as needed.
 */
int strfmt_more(char *dst,int dsta,struct strfmt *strfmt);

/* When (strfmt->awaiting) is set to 'i','l','f',p', you should pull from varags
 * and then call the corresponding function here.
 */
void strfmt_provide_i(struct strfmt *strfmt,int v);
void strfmt_provide_l(struct strfmt *strfmt,int64_t v);
void strfmt_provide_f(struct strfmt *strfmt,double v);
void strfmt_provide_p(struct strfmt *strfmt,const void *p);

#endif
