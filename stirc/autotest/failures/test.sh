#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

for a in Stirfile*; do
  ../../stirmake -f "$a" > /dev/null 2>&1 && die "Execution of $a succeeded unexpectedly"
done

echo OK
