#!/bin/sh

(cd abce; sh bootstrapnomake.sh)

CC=cc
CFLAGS="-O3 -Wall -g"

die()
{
  echo "$@"
  exit 1
}

libobjs=""

for a in *.l; do
  base="`echo "$a"|sed 's/.l$//g'`"
  flex --outfile="$base.lex.c" --header-file="$base.lex.h" "$a" || die "flex"
done
for a in *.y; do
  base="`echo "$a"|sed 's/.y$//g'`"
  byacc -d -p "$base" -b "$base" -o "$base.tab.c" "$a" || die "byacc"
done
for a in *.c; do
  base="`echo "$a"|sed 's/.c$//g'`"
  if grep -q '^int main(int' "$a"; then
    true
  else
    libobjs="$libobjs $base.o"
  fi
  $CC $CFLAGS -Wno-sign-compare -Wno-missing-prototypes -c -o "$base.o" "$a" || die "cc"
done

rm -f libstirmake.a
ar rvs libstirmake.a $libobjs || die "ar"

$CC $CFLAGS -o stirmake stirmake.o libstirmake.a abce/libabce.a -lm -ldl || die "cclink"

rm -f smka
rm -f smkt
rm -f smkp
ln -s stirmake smka
ln -s stirmake smkt
ln -s stirmake smkp
