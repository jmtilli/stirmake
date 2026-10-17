#!/bin/sh

FAILCNT=0

fail()
{
  echo "Test failed"
  FAILCNT="`expr "$FAILCNT" + 1`"
}

runtest()
{
  echo "Executing test: $1"
  (cd "$1"; ./test.sh) || fail
}

runtest "clean"
runtest "touchmode"
runtest "headerrm"
runtest "autotarget"

echo "Failure count: $FAILCNT"
if [ "$FAILTCNT" = 0 ]; then
  exit 0
fi
exit 1
