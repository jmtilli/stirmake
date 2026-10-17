#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

../../smka -s -bc > /dev/null 2>&1 || die "clean failed"

touch x.h y.h

../../smka -s > /dev/null 2>&1 || die "first invocation failed"
if [ \! -e x.d -o \! -e x.o ]; then
  die "Nonexistent"
fi
../../smka -s > /dev/null 2>&1 || die "second invocation failed"
if [ \! -e x.d -o \! -e x.o ]; then
  die "Nonexistent"
fi

../../smka -s -q x.d > /dev/null 2>&1 || die "up-to-date test fail 1"
../../smka -s -q y.d > /dev/null 2>&1 || die "up-to-date test fail 2"

touch y.h

../../smka -s -q x.d > /dev/null 2>&1 || die "up-to-date test y fail 1"
../../smka -s -q y.d > /dev/null 2>&1 || die "up-to-date test y fail 2"

touch x.h

../../smka -s -q x.d > /dev/null 2>&1 && die "up-to-date test x fail 1"
../../smka -s -q y.d > /dev/null 2>&1 || die "up-to-date test x fail 2"

../../smka -s -bc > /dev/null 2>&1 || die "clean failed"

rm -f x.h y.h

echo OK
