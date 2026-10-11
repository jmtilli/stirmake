#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include "mygetline.h"

#undef ALT

#ifdef ALT
ssize_t mygetline(char **lineptr, size_t *n, FILE *f)
{
  size_t off = 0;
  int ch;
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
    if (off + 8 + 1 > (*n))
    {
      size_t newcap = 2*(*n);
      char *newptr;
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

    ch = getc_unlocked(f);
    if (ch == EOF || ch == '\n') break;
    (*lineptr)[off++] = (char)(unsigned char)ch;

    ch = getc_unlocked(f);
    if (ch == EOF || ch == '\n') break;
    (*lineptr)[off++] = (char)(unsigned char)ch;

    ch = getc_unlocked(f);
    if (ch == EOF || ch == '\n') break;
    (*lineptr)[off++] = (char)(unsigned char)ch;

    ch = getc_unlocked(f);
    if (ch == EOF || ch == '\n') break;
    (*lineptr)[off++] = (char)(unsigned char)ch;

    ch = getc_unlocked(f);
    if (ch == EOF || ch == '\n') break;
    (*lineptr)[off++] = (char)(unsigned char)ch;

    ch = getc_unlocked(f);
    if (ch == EOF || ch == '\n') break;
    (*lineptr)[off++] = (char)(unsigned char)ch;

    ch = getc_unlocked(f);
    if (ch == EOF || ch == '\n') break;
    (*lineptr)[off++] = (char)(unsigned char)ch;

    ch = getc_unlocked(f);
    if (ch == EOF || ch == '\n') break;
    (*lineptr)[off++] = (char)(unsigned char)ch;
  }
  if (ch == '\n')
  {
    (*lineptr)[off++] = (char)(unsigned char)ch;
  }
  (*lineptr)[off] = '\0';
  if (off == 0)
  {
    return -1;
  }
  return (ssize_t)off;
}
#else

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
#endif
