#ifndef _CANON_H_
#define _CANON_H_

#include "stirutils.h"

char *pathcat1_buf(const char *old, char *buf, mysize_t bufsz);
char *pathcat2_buf(const char *old, const char *old2, char *buf, mysize_t bufsz);
char *pathcat2_buflen(const char *old, mysize_t oldlen, const char *old2, mysize_t old2len, char *buf, mysize_t bufsz);

size_t strcnt(const char *haystack, char needle);

char *canon(const char *old);
char *canon_buflen(const char *old, mysize_t oldlen, char *buf, mysize_t bufsz,
                   mysize_t *neulen);
static inline char *canon_buf(const char *old, char *buf, mysize_t bufsz)
{
  return canon_buflen(old, strlen(old), buf, bufsz, NULL);
}

char *construct_backpath(const char *frontpath);

char *neighpath(const char *path, const char *file);

char *neighpath_buflen(const char *path, mysize_t pathlen,
                       const char *file, mysize_t filelen,
                       char *buf, mysize_t bufsz, mysize_t *reslen);

#endif
