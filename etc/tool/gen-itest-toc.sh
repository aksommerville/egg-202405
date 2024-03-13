#!/bin/bash

if [ "$#" -lt 1 ] ; then
  echo "Usage: $0 OUTPUT [INPUT...]"
  exit 1
fi

DSTPATH="$1"
shift 1

TMPPATH=mid/tests/test/int/tmptoc
rm -f $TMPPATH
touch $TMPPATH

while [ "$#" -gt 0 ] ; do
  SRCPATH="$1"
  shift 1
  SRCBASE="$(basename $SRCPATH)"
  nl -ba -s' ' "$SRCPATH" | sed -En 's/^ *([0-9]+) *(XXX_)?ITEST\(([a-zA-Z0-9_]+)(,[^\)]*)?\).*$/'"$SRCBASE"' \1 _\2 \3 \4/p' >>$TMPPATH
done

rm -f "$DSTPATH"
cat - >"$DSTPATH" <<EOF
/* itest_toc.h
 * Generated at $(date).
 * Careful where you include this -- each include site gets its own copy of the TOC.
 */
 
#ifndef ITEST_TOC_H
#define ITEST_TOC_H

EOF

cat $TMPPATH | while read F L I N T ; do
  echo "int $N();" >>"$DSTPATH"
done

cat - >>"$DSTPATH" <<EOF

static const struct itest {
  int (*fn)();
  const char *name;
  const char *tags;
  const char *file;
  int line;
  int if_unspecified;
} itestv[]={
EOF

cat $TMPPATH | while read F L I N T ; do
  if [ "$I" = "_" ] ; then
    IFUN=1
  else
    IFUN=0
  fi
  echo "  {$N,\"$N\",\"$T\",\"$F\",$L,$IFUN}," >>"$DSTPATH"
done

cat - >>"$DSTPATH" <<EOF
};

#endif
EOF
