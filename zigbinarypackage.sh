#!/bin/sh

die()
{
  echo "$@"
  exit 1
}

if [ "$1" = "arm64" ]; then
  export CC="zig cc -target aarch64-linux-gnu.2.17"
  export PKGARCH="arm64-linux-glibc2.17"
  export DEBARCH="arm64"
elif [ "$1" = "arm32" ]; then
  export CC="zig cc -target arm-linux-gnueabihf.2.7"
  export PKGARCH="arm32hf-linux-glibc2.7"
  export DEBARCH="armhf"
elif [ "$1" = "amd64" ]; then
  export CC="zig cc -target x86_64-linux-gnu.2.7"
  export PKGARCH="amd64-linux-glibc2.7"
  export DEBARCH="amd64"
elif [ "$1" = "i386" ]; then
  export CC="zig cc -target x86-linux-gnu.2.7"
  export PKGARCH="i386-linux-glibc2.7"
  export DEBARCH="i386"
else
  die "Unsupported architecture: $1"
fi

tmpdir="`mktemp -d`"
pwd="`pwd`"
cd "$tmpdir" || die "Can't goto tmp dir"
git clone --recursive https://github.com/Aalto5G/stirmake || die "Can't clone"
cd stirmake/stirc || die "Can't chdir"
smka -ja || die "Can't compile"
# These are unnecessary and increase tarball size:
vname="`git describe --tags`"
nonvname="$vname"
case "$nonvname" in
  v*)
    nonvname="${vname#v}"
esac
chmod 755 stirmake || die "chmod fail"
chmod 644 stirmake.1 || die "chmod fail"
mv stirmake .. || die "mv fail"
mv smka .. || die "mv fail"
mv smkt .. || die "mv fail"
mv smkp .. || die "mv fail"
mv install.sh .. || die "mv fail"
mv stirmake.1 .. || die "mv fail"
cd ../.. || die "Can't chdir"
mv stirmake stirmake-"$vname" || die "Can't rename directory"
tar czvf stirmakebin-"$PKGARCH"-"$vname".tar.gz stirmake-"$vname"/stirmake stirmake-"$vname"/smka stirmake-"$vname"/smkt stirmake-"$vname"/smkp stirmake-"$vname"/install.sh stirmake-"$vname"/stirmake.1 || die "Can't create tar.gz"
mkdir -p stirmake_"$nonvname"_"$DEBARCH/usr/bin" || die "mkdir fail"
mkdir -p stirmake_"$nonvname"_"$DEBARCH/usr/share/man/man1" || die "mkdir fail"
mkdir -p stirmake_"$nonvname"_"$DEBARCH/DEBIAN" || die "mkdir fail"
cp -p stirmake-"$vname"/stirmake stirmake_"$nonvname"_"$DEBARCH/usr/bin" || die "cp fail"
ln -s stirmake stirmake_"$nonvname"_"$DEBARCH/usr/bin/smka" || die "ln fail"
ln -s stirmake stirmake_"$nonvname"_"$DEBARCH/usr/bin/smkt" || die "ln fail"
ln -s stirmake stirmake_"$nonvname"_"$DEBARCH/usr/bin/smkp" || die "ln fail"
cp -p stirmake-"$vname"/stirmake.1 stirmake_"$nonvname"_"$DEBARCH/usr/share/man/man1"
echo "Package: stirmake" > stirmake_"$nonvname"_"$DEBARCH/DEBIAN/control" || die "echo fail"
echo "Version: $nonvname" >> stirmake_"$nonvname"_"$DEBARCH/DEBIAN/control" || die "echo fail"
echo "Architecture: $DEBARCH" >> stirmake_"$nonvname"_"$DEBARCH/DEBIAN/control" || die "echo fail"
echo "Maintainer: Juha-Matti Tilli" >> stirmake_"$nonvname"_"$DEBARCH/DEBIAN/control" || die "echo fail"
echo "Description: Stirmake, scalable true implementation of recursive make, for easy multi-dir builds." >> stirmake_"$nonvname"_"$DEBARCH/DEBIAN/control" || die "echo fail"
dpkg-deb --build -Zgzip stirmake_"$nonvname"_"$DEBARCH" || die "dpkg-deb fail"
rm -rf stirmake-"$vname"
rm -rf stirmake_"$nonvname"_"$DEBARCH"
cd "$pwd" || die "Can't chdir back to old directory"
if [ -e "stirmakebin-$PKGARCH-$vname.tar.gz" ]; then
  die "Package stirmakebin-$PKGARCH-$vname.tar.gz already exists"
else
  mv "$tmpdir/stirmakebin-$PKGARCH-$vname.tar.gz" . || die "Can't move package"
fi
if [ -e stirmake_"$nonvname"_"$DEBARCH".deb ]; then
  die "Package stirmake_${nonvname}_$DEBARCH.deb already exists"
else
  mv "$tmpdir/stirmake_${nonvname}_$DEBARCH.deb" . || die "Can't move package"
fi
rmdir "$tmpdir" || die "Can't rm temporary dir"
echo
echo "Package created:"
echo "stirmakebin-$PKGARCH-$vname.tar.gz"
echo "stirmake_${nonvname}_${DEBARCH}.tar.gz"
