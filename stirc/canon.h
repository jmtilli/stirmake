#ifndef _CANON_H_
#define _CANON_H_

size_t strcnt(const char *haystack, char needle);

char *canon(const char *old);
char *canon_buf(const char *old, char *buf, size_t bufsz);

char *construct_backpath(const char *frontpath);

char *neighpath(const char *path, const char *file);

#endif
