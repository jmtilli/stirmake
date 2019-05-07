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

void incyydomemparse(char *filedata, size_t filesize, struct incyy *incyy);

void incyynameparse(const char *fname, struct incyy *incyy, int require);

void incyydirparse(
  const char *argv0, const char *fname, struct incyy *incyy, int require);

struct escaped_string yy_escape_string(char *orig);

struct escaped_string yy_escape_string_single(char *orig);

#ifdef __cplusplus
};
#endif

#endif

