#!/bin/bash

#-------------------------------------------------------------------

echo "Edit $0 to change test verbosity."

log_pass() { true ;   echo -e "\x1b[42m  \x1b[0m PASS $1" ; }
log_fail() { true ;   echo -e "\x1b[41m  \x1b[0m FAIL $1" ; }
#log_skip() { true ;   echo -e "\x1b[47m  \x1b[0m SKIP $1" ; }
log_skip() { true; }
log_detail() { true ; echo -e "\x1b[91m$1\x1b[0m" ; }
log_raw() { true ;    echo "$1" ; }

#-------------------------------------------------------------------

FAILC=0
PASSC=0
SKIPC=0

runtest() {
  while IFS= read INTAKE ; do
    read INTRODUCER KEYWORD ARGS <<<"$INTAKE"
    if [ "$INTRODUCER" != "TEST" ] ; then
      log_raw "$INTAKE"
    else
      case "$KEYWORD" in
        PASS) PASSC=$((PASSC+1)) ; log_pass "$ARGS" ;;
        FAIL) FAILC=$((FAILC+1)) ; log_fail "$ARGS" ;;
        SKIP) SKIPC=$((SKIPC+1)) ; log_skip "$ARGS" ;;
        DETAIL) log_detail "$ARGS" ;;
        *) log_raw "$INTAKE" ;;
      esac
    fi
  done < <( $1 2>&1 || echo "TEST FAIL $1" )
}

for EXE in $* ; do
  runtest "$EXE"
done

if [ "$FAILC" -gt 0 ] ; then
  echo -e "\x1b[41m    \x1b[0m $FAILC fail, $PASSC pass, $SKIPC skip"
elif [ "$PASSC" -gt 0 ] ; then
  echo -e "\x1b[42m    \x1b[0m $FAILC fail, $PASSC pass, $SKIPC skip"
else
  echo -e "\x1b[47m    \x1b[0m $FAILC fail, $PASSC pass, $SKIPC skip"
fi
