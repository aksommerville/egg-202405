/* test.h
 * All unit and integration tests should include this.
 */
 
#ifndef TEST_H
#define TEST_H

// This header only needs stdio.h, but I figure we always want stdlib and string too...
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Table of contents declarations.
 *************************************************************************/

#define UTEST(fnname,...) if (test_filter(#fnname,#__VA_ARGS__,1)) { \
  if (fnname()<0) { \
    fprintf(stderr,"TEST FAIL %s [%s %s]\n",#fnname,__FILE__,#__VA_ARGS__); \
  } else { \
    fprintf(stderr,"TEST PASS %s [%s %s]\n",#fnname,__FILE__,#__VA_ARGS__); \
  } \
} else { \
  fprintf(stderr,"TEST SKIP %s [%s %s]\n",#fnname,__FILE__,#__VA_ARGS__); \
}
#define XXX_UTEST(fnname,...) if (test_filter(#fnname,#__VA_ARGS__,0)) { \
  if (fnname()<0) { \
    fprintf(stderr,"TEST FAIL %s [%s %s]\n",#fnname,__FILE__,#__VA_ARGS__); \
  } else { \
    fprintf(stderr,"TEST PASS %s [%s %s]\n",#fnname,__FILE__,#__VA_ARGS__); \
  } \
} else { \
  fprintf(stderr,"TEST SKIP %s [%s %s]\n",#fnname,__FILE__,#__VA_ARGS__); \
}

#define ITEST(fnname,...) int fnname()
#define XXX_ITEST(fnname,...) int fnname()

int test_filter(const char *fnname,const char *tags,int if_unspecified);

/* Assertions.
 **************************************************************************/
 
#define TEST_FAIL_RESULT -1

#define TEST_FAIL_MORE(k,fmt,...) { \
  fprintf(stderr,"TEST DETAIL | %20s: "fmt"\n",k,##__VA_ARGS__); \
}
#define TEST_FAIL_BEGIN(fmt,...) { \
  fprintf(stderr,"TEST DETAIL +---------------------------------------------------\n"); \
  if (fmt[0]) TEST_FAIL_MORE("Message",fmt,##__VA_ARGS__) \
  TEST_FAIL_MORE("Location","%s:%d",__FILE__,__LINE__) \
}
#define TEST_FAIL_END { \
  fprintf(stderr,"TEST DETAIL +---------------------------------------------------\n"); \
  return TEST_FAIL_RESULT; \
}

#define FAIL(...) { \
  TEST_FAIL_BEGIN(""__VA_ARGS__) \
  TEST_FAIL_END \
}

#define ASSERT(value,...) { \
  if (!(value)) { \
    TEST_FAIL_BEGIN(""__VA_ARGS__) \
    TEST_FAIL_MORE("As written","%s",#value) \
    TEST_FAIL_MORE("Expected","true") \
    TEST_FAIL_END \
  } \
}

#define ASSERT_NOT(value,...) { \
  if (value) { \
    TEST_FAIL_BEGIN(""__VA_ARGS__) \
    TEST_FAIL_MORE("As written","%s",#value) \
    TEST_FAIL_MORE("Expected","false") \
    TEST_FAIL_END \
  } \
}

#define ASSERT_INTS_OP(a,op,b,...) { \
  int _a=(a),_b=(b); \
  if (!(_a op _b)) { \
    TEST_FAIL_BEGIN(""__VA_ARGS__) \
    TEST_FAIL_MORE("As written","%s %s %s",#a,#op,#b) \
    TEST_FAIL_MORE("Values","%d %s %d",_a,#op,_b) \
    TEST_FAIL_END \
  } \
}
#define ASSERT_INTS(a,b,...) ASSERT_INTS_OP(a,==,b,##__VA_ARGS__)

#define ASSERT_CALL(call,...) { \
  int _result=(call); \
  if (_result<0) { \
    TEST_FAIL_BEGIN(""__VA_ARGS__) \
    TEST_FAIL_MORE("As written","%s",#call) \
    TEST_FAIL_MORE("Expected","Successful call") \
    TEST_FAIL_MORE("Value","%d",_result) \
    TEST_FAIL_END \
  } \
}
#define ASSERT_FAILURE(call,...) { \
  int _result=(call); \
  if (_result>=0) { \
    TEST_FAIL_BEGIN(""__VA_ARGS__) \
    TEST_FAIL_MORE("As written","%s",#call) \
    TEST_FAIL_MORE("Expected","Failed call") \
    TEST_FAIL_MORE("Value","%d",_result) \
    TEST_FAIL_END \
  } \
}

#define ASSERT_FLOATS(a,b,e,...) { \
  double _a=(a),_b=(b),_e=(e); \
  double _d=_a-_b; \
  if (_d<0.0) _d=-_d; \
  if (_d>_e) { \
    TEST_FAIL_BEGIN(""__VA_ARGS__) \
    TEST_FAIL_MORE("As written","%s == %s within %s",#a,#b,#e) \
    TEST_FAIL_MORE("Values","%f == %f within %f",_a,_b,_e) \
    TEST_FAIL_MORE("Difference","%f",_d) \
    TEST_FAIL_END \
  } \
}

#define ASSERT_STRINGS(a,ac,b,bc,...) { \
  const char *_a=(a),*_b=(b); \
  int _ac=(ac),_bc=(bc); \
  if (!_a) _ac=0; else if (_ac<0) { _ac=0; while (_a[_ac]) _ac++; } \
  if (!_b) _bc=0; else if (_bc<0) { _bc=0; while (_b[_bc]) _bc++; } \
  if ((_ac!=_bc)||memcmp(_a,_b,_ac)) { \
    TEST_FAIL_BEGIN(""__VA_ARGS__) \
    TEST_FAIL_MORE("(A) As written","%s : %s",#a,#ac) \
    TEST_FAIL_MORE("(B) As written","%s : %s",#b,#bc) \
    TEST_FAIL_MORE("(A) Value","%.*s",_ac,_a) \
    TEST_FAIL_MORE("(B) Value","%.*s",_bc,_b) \
    TEST_FAIL_END \
  } \
}

#endif
