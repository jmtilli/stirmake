#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

$VALGRIND ../../smka -bc -s || die "clean failed"
$VALGRIND ../../smka -s || die "build failed"

if [ \! -e bin ]; then
  die "bin nonexistent"
fi
if [ \! -e obj ]; then
  die "obj nonexistent"
fi
$VALGRIND ../../smka -bc -s || die "clean failed"

echo OK
