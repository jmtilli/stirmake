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

if [ '!' -w "$P" ]; then
  echo "No write permissions to $P"
  exit 1
fi
if [ '!' -d "$P" ]; then
  echo "Not a valid directory: $P"
  exit 1
fi

instbin()
{
  if [ -e "$P/bin/$1" ]; then
    ln "$P/bin/$1" "$P/bin/.$1.smkinstold.$$.$H" || exit 1
  fi
  cp "$1" "$P/bin/.$1.smkinstnew.$$.$H" || exit 1
  mv "$P/bin/.$1.smkinstnew.$$.$H" "$P/bin/$1" || exit 1
  if [ -e "$P/bin/.$1.smkinstold.$$.$H" ]; then
    # If you mount binaries across NFS, and run this command on the NFS server,
    # you might want to comment out this rm command.
    rm "$P/bin/.$1.smkinstold.$$.$H" || exit 1
  fi
}

instsym()
{
  if [ "`readlink "$P/bin/$1"`" != "stirmake" ]; then
    ln -s stirmake "$P/bin/.$1.smkinstnew.$$.$H" || exit 1
    mv "$P/bin/.$1.smkinstnew.$$.$H" "$P/bin/$1" || exit 1
  fi
}

instman()
{
  mkdir -p "$P/man/man$2" || exit 1
  cp "$1.$2" "$P/man/man$2/.$1.$2.smkinstnew.$$.$H" || exit 1
  mv "$P/man/man$2/.$1.$2.smkinstnew.$$.$H" "$P/man/man$2/$1.$2" || exit 1
}

instmansym()
{
  mkdir -p "$P/man/man$2" || exit 1
  if [ "`readlink "$P/man/man$2/$1.$2"`" != "stirmake.1" ]; then
    ln -s "stirmake.1" "$P/man/man$2/.$1.$2.smkinstnew.$$.$H" || exit 1
    mv "$P/man/man$2/.$1.$2.smkinstnew.$$.$H" "$P/man/man$2/$1.$2" || exit 1
  fi
}

# Ensure bin directory is there
mkdir -p "$P/bin" || exit 1

# Install binary
instbin stirmake

# Install symlinks
instsym smka
instsym smkp
instsym smkt

# Install man page
instman stirmake 1

# Install man page symlinks
instmansym smka 1
instmansym smkp 1
instmansym smkt 1

echo "All done, stirmake has been installed to $P"
