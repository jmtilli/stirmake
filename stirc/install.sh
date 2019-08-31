#!/bin/sh

if [ '!' -x "stirmake" ]; then
  echo "stirmake not made"
  exit 1
fi

PREFIX="$1"

if [ "a$PREFIX" = "a" ]; then
  PREFIX="~/.local"
fi

P="$PREFIX"
H="`hostname`"

instbin()
{
  if [ -e "$P/bin/$1" ]; then
    echo -- ln "$P/bin/$1" "$P/bin/.$1.smkinstold.$$.$H"
  fi
  echo -- cp $1 "$P/bin/.$1.smkinstnew.$$.$H"
  echo -- mv "$P/bin/.$1.smkinstnew.$$.$H" "$P/bin/$1"
  if [ -e "$P/bin/.$1.smkinstold.$$.$H" ]; then
    echo -- rm "$P/bin/.$1.smkinstold.$$.$H"
  fi
}

instsym()
{
  if [ "`readlink "$P/bin/$1"`" != "stirmake" ]; then
    echo -- ln -s stirmake "$P/bin/.$1.smkinstnew.$$.$H"
    echo -- mv "$P/bin/.$1.smkinstnew.$$.$H" "$P/bin/$1"
  fi
}

instman()
{
  echo -- mkdir -p "$P/man/man$2"
  echo -- cp $1.$2 "$P/man/man$2/$1.$2"
}

# Ensure bin directory is there
echo -- mkdir -p "$P/bin"

# Install binary
instbin stirmake

# Install symlinks
instsym smka
instsym smkp
instsym smkt

# Install man page
instman stirmake 1
