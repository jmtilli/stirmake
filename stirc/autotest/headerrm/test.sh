#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

../../smka -s -bc || die "clean failed"
../../smka -s || die "first invocation failed"
if [ \! -e x.d -o \! -e x.o ]; then
  die "Nonexistent"
fi
../../smka -s || die "second invocation failed"
if [ \! -e x.d -o \! -e x.o ]; then
  die "Nonexistent"
fi
../../smka -s -bc || die "clean failed"
echo OK
