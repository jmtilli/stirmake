#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

export LC_ALL=C

../../smka -h 2>&1|grep -q '[^[:print:][:space:]]' && die "Err1"
../../smka -v 2>&1|grep -q '[^[:print:][:space:]]' && die "Err2"
../../smka -G 2>&1|grep -q '[^[:print:][:space:]]' && die "Err3"

echo OK
