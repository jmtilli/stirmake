#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

$VALGRIND ../../smka -bc -s || die "clean failed"
$VALGRIND ../../smka -s || die "build failed"
$VALGRIND ../../smka -q -s recdep/lib.a || die "check1 failed"
sleep 0.05
touch recdep/src.c
../../smka -q -s recdep/lib.a && die "check2 failed"
$VALGRIND ../../smka -s || die "build failed"
$VALGRIND ../../smka -q -s recdep/lib.a || die "check3 failed"
$VALGRIND ../../smka -bc -s || die "clean failed"

$VALGRIND ../../stirmake -f Stirfile.detouch -bc -s || die "clean failed"
$VALGRIND ../../stirmake -f Stirfile.detouch -s || die "build failed"
$VALGRIND ../../stirmake -f Stirfile.detouch -q -s recdep/lib.a || die "check4 failed"
sleep 0.05
touch recdep/src.c
../../stirmake -f Stirfile.detouch -q -s recdep/lib.a && die "check5 failed"
$VALGRIND ../../stirmake -f Stirfile.detouch -s || die "build failed"
$VALGRIND ../../stirmake -f Stirfile.detouch -q -s recdep/lib.a || die "check6 failed"
$VALGRIND ../../stirmake -f Stirfile.detouch -bc -s || die "clean failed"

echo OK
