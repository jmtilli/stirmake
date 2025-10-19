#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

export CC="${CC:-cc}"
export CFLAGS="${CFLAGS:--O3 -Wall -g}"
export LDFLAGS="${LDFLAGS:-}"

(cd abce; sh bootstrapnomake.sh) || die "abce"

libobjs=""

echo "#ifndef _GIT_H_" > git.h
echo "#define _GIT_H_" >> git.h
echo "static const char *gitshas[] = {" >> git.h
git log --pretty=format:\"%h\", --abbrev=40 >> git.h
echo "};" >> git.h
echo -n "static const char *gitversion = \"" >> git.h
echo -n `git describe --tags|sed 's/^v//g'` >> git.h
echo "\";" >> git.h
echo "#endif" >> git.h

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

$CC $CFLAGS $LDFLAGS -o stirmake stirmake.o libstirmake.a abce/libabce.a -lm -ldl || die "cclink"

rm -f smka
rm -f smkt
rm -f smkp
ln -s stirmake smka
ln -s stirmake smkt
ln -s stirmake smkp
