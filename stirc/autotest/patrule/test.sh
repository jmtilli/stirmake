#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

for a in Stirfile*; do
  $VALGRIND ../../stirmake -f "$a" -s || die "$a execution fail"
  $VALGRIND ../../stirmake -f "$a" -bc -s || die "$a clean fail"
done

echo "OK"
