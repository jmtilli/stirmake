#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include "mygetline.h"

// FIXME what if the line contains '\0' byte? should use getc not fgets
ssize_t mygetline(char **lineptr, size_t *n, FILE *f)
{
  char *ret;
  size_t len;
  size_t off = 0;
  if (!*lineptr || !*n)
  {
    size_t newcap = 1024;
    char *newptr = malloc(newcap);
    if (!newptr)
    {
      errno = ENOMEM;
      return -1;
    }
    newptr[0] = '\0';
    *lineptr = newptr;
    *n = newcap;
  }
  for (;;)
  {
    size_t newcap;
    char *newptr;
    ret = fgets(&(*lineptr)[off], (((*n)-off)>INT_MAX) ? INT_MAX : ((*n)-off), f);
    if (!ret)
    {
      return off ? (ssize_t)off : (-1);
    }
    len = strlen(&(*lineptr)[off]);
    if (len && (*lineptr)[off+len-1] == '\n')
    {
      return (ssize_t)(len+off);
    }
    if (len < (*n) - off - 1)
    {
      return (ssize_t)(len+off);
    }
    off += len;
    newcap = 2*(*n);
    if (newcap < 1024)
    {
      newcap = 1024;
    }
    newptr = realloc(*lineptr, newcap);
    if (!newptr)
    {
      errno = ENOMEM;
      return -1;
    }
    *lineptr = newptr;
    *n = newcap;
  }
}

#if 0
int main(int argc, char **argv)
{
  char *ptr = NULL;
  size_t n = 0;
  printf("%zd\n", mygetline(&ptr, &n, stdin));
  printf("%s", ptr);
}
#endif
