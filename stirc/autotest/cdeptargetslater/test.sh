#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

$VALGRIND ../../smka -s -bc > /dev/null 2>&1 || die "clean failed"
$VALGRIND ../../smka -s > /dev/null 2>&1 || die "first invocation failed"
touch later1 later2 later3 later4 later5 later6
$VALGRIND ../../smka -s > /dev/null 2>&1 || die "second invocation failed"
$VALGRIND ../../smka -s -q all > /dev/null 2>&1 || die "check failed"

$VALGRIND ../../smka -s -bc > /dev/null 2>&1 || die "clean failed"

rm -f later1 later2 later3 later4 later5 later6

echo OK
