#!/bin/sh

if [ '!' -x "stirmake" ]; then
  echo "stirmake not made"
  exit 1
fi

PREFIX="$1"

if [ "a$PREFIX" = "a" ]; then
  PREFIX=~/.local
fi

P="$PREFIX"
H="`hostname`"

instbin()
{
  if [ -e "$P/bin/$1" ]; then
    ln "$P/bin/$1" "$P/bin/.$1.smkinstold.$$.$H"
  fi
  cp "$1" "$P/bin/.$1.smkinstnew.$$.$H"
  mv "$P/bin/.$1.smkinstnew.$$.$H" "$P/bin/$1"
  if [ -e "$P/bin/.$1.smkinstold.$$.$H" ]; then
    rm "$P/bin/.$1.smkinstold.$$.$H"
  fi
}

instsym()
{
  if [ "`readlink "$P/bin/$1"`" != "stirmake" ]; then
    ln -s stirmake "$P/bin/.$1.smkinstnew.$$.$H"
    mv "$P/bin/.$1.smkinstnew.$$.$H" "$P/bin/$1"
  fi
}

instman()
{
  mkdir -p "$P/man/man$2"
  cp "$1.$2" "$P/man/man$2/$1.$2"
}

# Ensure bin directory is there
mkdir -p "$P/bin"

# Install binary
instbin stirmake

# Install symlinks
instsym smka
instsym smkp
instsym smkt

# Install man page
instman stirmake 1

echo "All done, stirmake has been installed to $P"
