#include <stdio.h>
#include "canon.h"

int main(int argc, char **argv)
{
  printf("%s\n", canon("a/../b/c.txt"));
  printf("%s\n", canon("a/../../b/c.txt"));
  printf("%s\n", canon("a/./b/./c.txt"));
  printf("%s\n", canon("/a/../b/c.txt"));
  printf("%s\n", canon("/a/../../b/c.txt"));
  printf("%s\n", canon("/a/./b/./c.txt"));
  printf("%s\n", canon("//a/../b/c.txt"));
  printf("%s\n", canon("//a/../../b/c.txt"));
  printf("%s\n", canon("//a/./b/./c.txt"));
  printf("%s\n", canon("///a/../b/c.txt"));
  printf("%s\n", canon("///a/../../b/c.txt"));
  printf("%s\n", canon("///a/./b/./c.txt"));
  printf("%s\n", canon("///a/../b///c.txt"));
  printf("%s\n", canon("///a/../../b///c.txt"));
  printf("%s\n", canon("///a/./b/.///c.txt"));
  printf("%s\n", canon("/"));
  printf("%s\n", canon("//"));
  printf("%s\n", canon("///"));
  printf("%s\n", canon("a/"));
  printf("%s\n", canon("a//"));
  printf("%s\n", canon("a///"));
  printf("%s\n", canon("../../a/"));
  printf("%s\n", canon("../../a//"));
  printf("%s\n", canon("../../a///"));
  return 0;
}
