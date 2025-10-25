#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

tmpdir="`mktemp -d`"
pwd="`pwd`"
cd "$tmpdir" || die "Can't goto tmp dir"
git clone --recursive https://github.com/Aalto5G/stirmake || die "Can't clone"
cd stirmake/stirc || die "Can't chdir"
smka git.h || die "Can't create git.h"
rm .stir.db || die "Can't remove .stir.db"
# These are unnecessary and increase tarball size:
rm -rf longdep1
rm -rf longdep2
rm -rf longdep3
vname="`git describe --tags`"
rm -rf ../.git
rm -rf abce/.git
cd ../.. || die "Can't chdir"
mv stirmake stirmake-"$vname" || die "Can't rename dirctory"
tar czvf stirmake-"$vname".tar.gz stirmake-"$vname" || die "Can't create tar.gz"
rm -rf stirmake-"$vname"
cd "$pwd" || die "Can't chdir back to old directory"
if [ -e "stirmake-$vname.tar.gz" ]; then
  die "Package stirmake-$vname.tar.gz already exists"
else
  mv "$tmpdir/stirmake-$vname.tar.gz" . || die "Can't move package"
fi
rmdir "$tmpdir" || die "Can't rm temporary dir"
echo
echo "Package created:"
echo "stirmake-$vname.tar.gz"
