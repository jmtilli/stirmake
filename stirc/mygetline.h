#ifndef _MYGETLINE_H_
#define _MYGETLINE_H_

#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

ssize_t mygetline(char **lineptr, size_t *n, FILE *f);

#endif
