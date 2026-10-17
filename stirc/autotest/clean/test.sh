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
  if [ -e build ]; then die "$@" CC2; fi
}
noobj()
{
  if [ -e obj -o \! -e bin ]; then die "$@" AA; fi
  if [ -e subdir/obj -o \! -e subdir/bin ]; then die "$@" BB; fi
  if [ -e subproj/obj -o \! -e subproj/bin ]; then die "$@" CC; fi
  if [ -e build ]; then die "$@" CC2; fi
}
nobin()
{
  if [ \! -e obj -o -e bin ]; then die "$@" AA; fi
  if [ \! -e subdir/obj -o -e subdir/bin ]; then die "$@" BB; fi
  if [ \! -e subproj/obj -o -e subproj/bin ]; then die "$@" CC; fi
  if [ \! -e build/a.txt ]; then die "$@" CC2; fi
}
bothexist()
{
  if [ \! -e obj -o \! -e bin ]; then die "$@" AA; fi
  if [ \! -e subdir/obj -o \! -e subdir/bin ]; then die "$@" BB; fi
  if [ \! -e subproj/obj -o \! -e subproj/bin ]; then die "$@" CC; fi
  if [ \! -e build/a.txt ]; then die "$@" CC2; fi
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
../../smka -bc > /dev/null 2>&1 || die "cmdfail1"
noobjnobin "err1"
bothcleanassert "err1"
(cd subdir; ../../../smkp -ja) > /dev/null 2>&1 || die "cmdfail2"
bothexist "err2"
../../smka -bc > /dev/null 2>&1 || die "cmdfail3"
noobjnobin "err3"
bothcleanassert "err3"
(cd subdir; ../../../smkt -j2 ../all) > /dev/null 2>&1 || die "cmdfail4"
bothexist "err4"
hookclear
../../smka -b -ja > /dev/null 2>&1 || die "cmdfail5"
nobin "err5"
distcleanassert "err5"
(cd subproj; ../../../smkp -ja -Ot ../all) > /dev/null 2>&1 || die "cmdfail6"
bothexist "err6"
hookclear
../../smka -c -j2 > /dev/null 2>&1 || die "cmdfail7"
noobj "err7"
cleanassert "err7"
../../smka -bc -Ol all > /dev/null 2>&1 || die "cmdfail8"
hookclear
bothexist "err8"
../../smka -bc -j2 -Or all > /dev/null 2>&1 || die "cmdfail9"
hookclear
bothexist "err9"
../../smka -bc -ja -On all > /dev/null 2>&1 || die "cmdfail10"
hookclear
bothexist "err10"

# Test building and cleaning a subdirectory
../../smka -bc > /dev/null 2>&1
(cd subdir; ../../../smkt) > /dev/null 2>&1
if [ -e obj -o -e bin ]; then die "smkt fail 1"; fi
if [ \! -e subdir/obj -o \! -e subdir/bin ]; then die "smkt fail 2"; fi
if [ -e subproj/obj -o -e subproj/bin ]; then die "smkt fail 3"; fi
if [ \! -e build/a.txt ]; then die "smkt fail 4"; fi
touch bin
touch obj
(cd subdir; ../../../smkt -bc) > /dev/null 2>&1
if [ -e build/a.txt ]; then die "smkt fail 5"; fi
if [ -e subdir/obj -o -e subdir/bin ]; then die "smkt fail 6"; fi
if [ \! -e obj -o \! -e bin ]; then die "smkt fail 7"; fi

../../smka -bc > /dev/null 2>&1
hookclear
echo "OK"
