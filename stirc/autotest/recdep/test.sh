#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

../../smka -bc -s || die "clean failed"
../../smka -s || die "build failed"
../../smka -q -s recdep/lib.a || die "check1 failed"
sleep 0.05
touch recdep/src.c
../../smka -q -s recdep/lib.a && die "check2 failed"
../../smka -s || die "build failed"
../../smka -q -s recdep/lib.a || die "check3 failed"
../../smka -bc -s || die "clean failed"

../../stirmake -f Stirfile.detouch -bc -s || die "clean failed"
../../stirmake -f Stirfile.detouch -s || die "build failed"
../../stirmake -f Stirfile.detouch -q -s recdep/lib.a || die "check4 failed"
sleep 0.05
touch recdep/src.c
../../stirmake -f Stirfile.detouch -q -s recdep/lib.a && die "check5 failed"
../../stirmake -f Stirfile.detouch -s || die "build failed"
../../stirmake -f Stirfile.detouch -q -s recdep/lib.a || die "check6 failed"
../../stirmake -f Stirfile.detouch -bc -s || die "clean failed"

echo OK
