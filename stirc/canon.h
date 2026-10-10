#ifndef _CANON_H_
#define _CANON_H_

char *pathcat1_buf(const char *old, char *buf, size_t bufsz);
char *pathcat2_buf(const char *old, const char *old2, char *buf, size_t bufsz);
char *pathcat2_buflen(const char *old, size_t oldlen, const char *old2, size_t old2len, char *buf, size_t bufsz);

size_t strcnt(const char *haystack, char needle);

char *canon(const char *old);
char *canon_buflen(const char *old, size_t oldlen, char *buf, size_t bufsz,
                   size_t *neulen);
static inline char *canon_buf(const char *old, char *buf, size_t bufsz)
{
  return canon_buflen(old, strlen(old), buf, bufsz, NULL);
}

char *construct_backpath(const char *frontpath);

char *neighpath(const char *path, const char *file);

#endif
