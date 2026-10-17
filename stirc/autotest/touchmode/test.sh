#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

hookclear()
{
  rm -f bothcleanhookexec cleanhookexec distcleanhookexec
  rm -f subdir/bothcleanhookexec subdir/cleanhookexec subdir/distcleanhookexec
  rm -f subproj/bothcleanhookexec subproj/cleanhookexec subproj/distcleanhookexec
}

noobjnobin()
{
  if [ -e obj -o -e bin ]; then die "$@" AA; fi
  if [ -e subdir/obj -o -e subdir/bin ]; then die "$@" BB; fi
  if [ -e subproj/obj -o -e subproj/bin ]; then die "$@" CC; fi
  ../../stirmake -a -q bin > /dev/null 2>&1 && die "$@" CC3
}
noobj()
{
  if [ -e obj -o \! -e bin ]; then die "$@" AA; fi
  if [ -e subdir/obj -o \! -e subdir/bin ]; then die "$@" BB; fi
  if [ -e subproj/obj -o \! -e subproj/bin ]; then die "$@" CC; fi
  (cd subdir; ../../../stirmake -a -C .. -q bin) > /dev/null 2>&1 || die "$@" CC3
}
nobin()
{
  if [ \! -e obj -o -e bin ]; then die "$@" AA; fi
  if [ \! -e subdir/obj -o -e subdir/bin ]; then die "$@" BB; fi
  if [ \! -e subproj/obj -o -e subproj/bin ]; then die "$@" CC; fi
  (cd subdir; ../../../stirmake -a -q bin) > /dev/null 2>&1 && die "$@" CC3
}
bothexist()
{
  if [ \! -e obj -o \! -e bin ]; then die "$@" AA; fi
  if [ \! -e subdir/obj -o \! -e subdir/bin ]; then die "$@" BB; fi
  if [ \! -e subproj/obj -o \! -e subproj/bin ]; then die "$@" CC; fi
  ../../stirmake -a -q bin > /dev/null 2>&1 || die "$@" CC3
}
bothcleanassert()
{
  if [ \! -e bothcleanhookexec ]; then die "$@" DD; fi
  if [ -e distcleanhookexec -o -e cleanhookexec ]; then die "$@" EE; fi
  if [ \! -e subdir/bothcleanhookexec ]; then die "$@" FF; fi
  if [ -e subdir/distcleanhookexec -o -e subdir/cleanhookexec ]; then die "$@" GG; fi
  if [ \! -e subproj/bothcleanhookexec ]; then die "$@" HH; fi
  if [ -e subproj/distcleanhookexec -o -e subproj/cleanhookexec ]; then die "$@" II; fi
}
distcleanassert()
{
  if [ \! -e distcleanhookexec ]; then die "$@" DD; fi
  if [ -e bothcleanhookexec -o -e cleanhookexec ]; then die "$@" EE; fi
  if [ \! -e subdir/distcleanhookexec ]; then die "$@" FF; fi
  if [ -e subdir/bothcleanhookexec -o -e subdir/cleanhookexec ]; then die "$@" GG; fi
  if [ \! -e subproj/distcleanhookexec ]; then die "$@" HH; fi
  if [ -e subproj/bothcleanhookexec -o -e subproj/cleanhookexec ]; then die "$@" II; fi
}
cleanassert()
{
  if [ \! -e cleanhookexec ]; then die "$@" DD; fi
  if [ -e bothcleanhookexec -o -e distcleanhookexec ]; then die "$@" EE; fi
  if [ \! -e subdir/cleanhookexec ]; then die "$@" FF; fi
  if [ -e subdir/bothcleanhookexec -o -e subdir/distcleanhookexec ]; then die "$@" GG; fi
  if [ \! -e subproj/cleanhookexec ]; then die "$@" HH; fi
  if [ -e subproj/bothcleanhookexec -o -e subproj/distcleanhookexec ]; then die "$@" II; fi
}

hookclear
../../stirmake -a -bc > /dev/null 2>&1 || die "cmdfail1"
noobjnobin "err1"
bothcleanassert "err1"
(cd subdir; ../../../stirmake -p -ja) > /dev/null 2>&1 || die "cmdfail2"
bothexist "err2"
(cd subdir; ../../../stirmake -a -bc) > /dev/null 2>&1 || die "cmdfail3"
noobjnobin "err3"
bothcleanassert "err3"
../../stirmake -a -n > /dev/null 2>&1 || die "cmdfail3A"
noobjnobin "err3A"
bothcleanassert "err3A"
(cd subdir; ../../../stirmake -t -j2 ../all) > /dev/null 2>&1 || die "cmdfail4"
bothexist "err4"
hookclear
../../stirmake -a -b -ja > /dev/null 2>&1 || die "cmdfail5"
nobin "err5"
distcleanassert "err5"
(cd subproj; ../../../stirmake -p -ja -Ot ../all) > /dev/null 2>&1 || die "cmdfail6"
bothexist "err6"
hookclear
../../stirmake -a -c -j2 > /dev/null 2>&1 || die "cmdfail7"
noobj "err7"
cleanassert "err7"
../../stirmake -a -bc -Ol all > /dev/null 2>&1 || die "cmdfail8"
hookclear
bothexist "err8"
../../stirmake -a -bc -j2 -Or all > /dev/null 2>&1 || die "cmdfail9"
hookclear
bothexist "err9"
../../stirmake -a -bc -ja -On all > /dev/null 2>&1 || die "cmdfail10"
hookclear
bothexist "err10"

# Test building and cleaning a subdirectory
../../stirmake -a -bc > /dev/null 2>&1
(cd subdir; ../../../stirmake -t) > /dev/null 2>&1
if [ -e obj -o -e bin ]; then die "smkt fail 1"; fi
if [ \! -e subdir/obj -o \! -e subdir/bin ]; then die "smkt fail 2"; fi
if [ -e subproj/obj -o -e subproj/bin ]; then die "smkt fail 3"; fi
touch bin
touch obj
(cd subdir; ../../../stirmake -t -bc) > /dev/null 2>&1
if [ -e subdir/obj -o -e subdir/bin ]; then die "smkt fail 6"; fi
if [ \! -e obj -o \! -e bin ]; then die "smkt fail 7"; fi

# Test touch mode
../../stirmake -a -bc > /dev/null 2>&1
noobjnobin "err3"
hookclear
../../stirmake -a -T > /dev/null 2>&1
bothexist "err10"

../../stirmake -a -bc > /dev/null 2>&1
hookclear
echo "OK"
