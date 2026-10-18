#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

$VALGRIND ../../smka -s -bc > /dev/null 2>&1 || die "clean failed"
../../stirmake -s -f Stirfile.noignore > /dev/null 2>&1 && die "no-ignore-fail"
$VALGRIND ../../stirmake -s -f Stirfile.noautophony > /dev/null 2>&1 || die "no-autophony-fail"

touch x.h y.h

$VALGRIND ../../smka -s > /dev/null 2>&1 || die "first invocation failed"
if [ \! -e x.d -o \! -e x.o ]; then
  die "Nonexistent"
fi
$VALGRIND ../../smka -s > /dev/null 2>&1 || die "second invocation failed"
if [ \! -e x.d -o \! -e x.o ]; then
  die "Nonexistent"
fi
$VALGRIND ../../stirmake -s -f Stirfile.noignore > /dev/null 2>&1 || die "no-ignore-fail 2"

$VALGRIND ../../smka -s -q x.d > /dev/null 2>&1 || die "up-to-date test fail 1"
$VALGRIND ../../smka -s -q y.d > /dev/null 2>&1 || die "up-to-date test fail 2"

touch y.h

$VALGRIND ../../smka -s -q x.d > /dev/null 2>&1 || die "up-to-date test y fail 1"
$VALGRIND ../../smka -s -q y.d > /dev/null 2>&1 || die "up-to-date test y fail 2"

touch x.h

../../smka -s -q x.d > /dev/null 2>&1 && die "up-to-date test x fail 1"
$VALGRIND ../../smka -s -q y.d > /dev/null 2>&1 || die "up-to-date test x fail 2"

$VALGRIND ../../stirmake -s -f Stirfile.noautophony > /dev/null 2>&1 || die "no-autophony-fail 2"
rm -f x.h y.h
../../stirmake -s -f Stirfile.noautophony > /dev/null 2>&1 && die "no-autophony-fail 3"

$VALGRIND ../../smka -s -bc > /dev/null 2>&1 || die "clean failed"

rm -f x.h y.h

echo OK
