#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

../../smka -s -bc > /dev/null 2>&1 || die "clean failed"
../../smka -s > /dev/null 2>&1 || die "first invocation failed"

if [ \! -e 'tgt1' -o \! -e 'tgt2' ]; then
  die "Nonexistent"
fi
../../smka -s -q tgt1 > /dev/null 2>&1 || die "tgt1 check failed"
../../smka -s -q tgt2 > /dev/null 2>&1 || die "tgt2 check failed"
touch Stirfile
../../smka -s -q tgt1 > /dev/null 2>&1 && die "tgt1 check2 failed"
../../smka -s -q tgt2 > /dev/null 2>&1 && die "tgt2 check2 failed"

../../smka -s 'tgt4' > /dev/null 2>&1 || die "tgt4 failed"
../../smka -s -bc > /dev/null 2>&1 || die "clean failed"

echo OK
