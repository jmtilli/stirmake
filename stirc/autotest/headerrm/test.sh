#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

$VALGRIND ../../smka -s -bc || die "clean failed"
$VALGRIND ../../smka -s || die "first invocation failed"
if [ \! -e x.d -o \! -e x.o ]; then
  die "Nonexistent"
fi
$VALGRIND ../../smka -s || die "second invocation failed"
if [ \! -e x.d -o \! -e x.o ]; then
  die "Nonexistent"
fi
../../stirmake -f Stirfail > /dev/null 2>&1 && die "this is supposed to fail"
$VALGRIND ../../smka -s -bc || die "clean failed"
$VALGRIND ../../stirmake -f Stirfail > /dev/null 2>&1 || die "this is supposed to work now"
$VALGRIND ../../smka -s -bc || die "clean failed"
echo OK
