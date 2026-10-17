#!/bin/sh

die()
{
  echo "$@"
  rm -f a b c d e f g h i j k l m n o p q r s t u v w x y z
  exit 1
}

for a in Stirfile*; do
  rm -f a b c d e f g h i j k l m n o p q r s t u v w x y z
  ../../stirmake -f "$a" > /dev/null 2>&1 && die "Execution of $a succeeded unexpectedly"
done

rm -f a b c d e f g h i j k l m n o p q r s t u v w x y z

echo OK
