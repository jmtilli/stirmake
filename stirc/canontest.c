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
  printf("%s\n", canon("."));
  printf("\n");
  printf("%s\n", construct_backpath("a/../b/c.txt"));
  //printf("%s\n", construct_backpath("a/../../b/c.txt"));
  printf("%s\n", construct_backpath("a/./b/./c.txt"));
#if 0
  printf("%s\n", construct_backpath("/a/../b/c.txt"));
  //printf("%s\n", construct_backpath("/a/../../b/c.txt"));
  printf("%s\n", construct_backpath("/a/./b/./c.txt"));
  printf("%s\n", construct_backpath("//a/../b/c.txt"));
  //printf("%s\n", construct_backpath("//a/../../b/c.txt"));
  printf("%s\n", construct_backpath("//a/./b/./c.txt"));
  printf("%s\n", construct_backpath("///a/../b/c.txt"));
  //printf("%s\n", construct_backpath("///a/../../b/c.txt"));
  printf("%s\n", construct_backpath("///a/./b/./c.txt"));
  printf("%s\n", construct_backpath("///a/../b///c.txt"));
  //printf("%s\n", construct_backpath("///a/../../b///c.txt"));
  printf("%s\n", construct_backpath("///a/./b/.///c.txt"));
  printf("%s\n", construct_backpath("/"));
  printf("%s\n", construct_backpath("//"));
  printf("%s\n", construct_backpath("///"));
#endif
  printf("%s\n", construct_backpath("a/"));
  printf("%s\n", construct_backpath("a//"));
  printf("%s\n", construct_backpath("a///"));
  //printf("%s\n", construct_backpath("../../a/"));
  //printf("%s\n", construct_backpath("../../a//"));
  //printf("%s\n", construct_backpath("../../a///"));
  printf("%s\n", construct_backpath("."));
  return 0;
}
