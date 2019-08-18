#ifndef _YYUTILS_H_
#define _YYUTILS_H_

#include <stdio.h>
#include <stdint.h>
#include "stiryy.h"

#ifdef __cplusplus
extern "C" {
#endif


int stiryydoparse(FILE *filein, struct stiryy *stiryy);

void stiryydomemparse(char *filedata, size_t filesize, struct stiryy *stiryy);

int stiryynameparse(const char *fname, struct stiryy *stiryy, int require);

int stiryydirparse(
  const char *argv0, const char *fname, struct stiryy *stiryy, int require);

struct escaped_string yy_escape_string(char *orig);

struct escaped_string yy_escape_string_single(char *orig);

uint32_t yy_get_ip(char *orig);

#ifdef __cplusplus
};
#endif

#endif

