#ifndef _CANON_H_
#define _CANON_H_

size_t strcnt(const char *haystack, char needle);

char *canon(const char *old);

char *construct_backpath(const char *frontpath);

#endif
