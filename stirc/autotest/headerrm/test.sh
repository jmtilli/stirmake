#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

../../smka -s -bc || die "clean failed"
../../smka -s || die "first invocation failed"
if [ \! -e x.d -o \! -e x.o ]; then
  die "Nonexistent"
fi
../../smka -s || die "second invocation failed"
if [ \! -e x.d -o \! -e x.o ]; then
  die "Nonexistent"
fi
../../stirmake -f Stirfail > /dev/null 2>&1 && die "this is supposed to fail"
../../smka -s -bc || die "clean failed"
../../stirmake -f Stirfail > /dev/null 2>&1 || die "this is supposed to work now"
../../smka -s -bc || die "clean failed"
echo OK
