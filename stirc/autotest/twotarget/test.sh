#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

$VALGRIND ../../smka -s -bc > /dev/null 2>&1 || die "clean failed"
$VALGRIND ../../smka -s > /dev/null 2>&1 || die "first invocation failed"

if [ \! -e 'tgt1' -o \! -e 'tgt2' ]; then
  die "Nonexistent"
fi
$VALGRIND ../../smka -s -q tgt1 > /dev/null 2>&1 || die "tgt1 check failed"
$VALGRIND ../../smka -s -q tgt2 > /dev/null 2>&1 || die "tgt2 check failed"
sleep 0.05
touch Stirfile
../../smka -s -q tgt1 > /dev/null 2>&1 && die "tgt1 check2 failed"
../../smka -s -q tgt2 > /dev/null 2>&1 && die "tgt2 check2 failed"

$VALGRIND ../../smka -s 'tgt4' > /dev/null 2>&1 || die "tgt4 failed"
$VALGRIND ../../smka -s -bc > /dev/null 2>&1 || die "clean failed"

echo OK
