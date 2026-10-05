#ifndef _INCYYUTILS_H_
#define _INCYYUTILS_H_

#include <stdio.h>
#include <stdint.h>
#include "incyy.h"
#include "stiryy.h"

#ifdef __cplusplus
extern "C" {
#endif

void incyydoparse(FILE *filein, struct incyy *incyy);
int incyymineparse(FILE *f, struct incyy *incyy);

#if !STIR_NO_MEMPARSE
void incyydomemparse(char *filedata, size_t filesize, struct incyy *incyy);
#endif

void incyynameparse(const char *fname, struct incyy *incyy, int require);

void incyydirparse(
  const char *argv0, const char *fname, struct incyy *incyy, int require);

struct escaped_string yy_escape_string(char *orig, char **strendptr);

struct escaped_string yy_escape_string_single(char *orig, char **strendptr);

#ifdef __cplusplus
};
#endif

#endif

