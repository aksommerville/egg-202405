/* qjs.h
 * Adapter to QuickJS.
 * API is deliberately similar to 'wamr.h'.
 */
 
#ifndef QJS_H
#define QJS_H

#include <stdint.h>

struct qjs;

void qjs_del(struct qjs *qjs);

struct qjs *qjs_new();

/* (entryv) is array of JSCFunctionListEntry from quickjs.h.
 */
int qjs_set_exports(struct qjs *qjs,const char *modname,const void *entryv,int entryc);

int qjs_add_module(struct qjs *qjs,int modid,const void *v,int c,const char *refname);
int qjs_link_function(struct qjs *qjs,int modid,int fnid,const char *name);

/* Return value goes in (argv[0]).
 * This is designed to operate just like wamr_call(). Not sure that's helpful, though.
 */
int qjs_call(struct qjs *qjs,int modid,int fnid,uint32_t *argv,int argc);

/* Return value from the function, if it returns an integer >=0.
 * (fmt) describes variadic arguments:
 *   'i' int
 *   'f' double
 *   's' char*
 *   '*s' int,char*
 */
int qjs_callf(struct qjs *qjs,int modid,int fnid,const char *fmt,...);

#endif
