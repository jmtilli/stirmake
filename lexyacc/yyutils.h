#ifndef _YYUTILS_H_
#define _YYUTILS_H_

#include <stdio.h>
#include <stdint.h>
#include "stiryy.h"

void stiryydoparse(FILE *filein, struct stiryy *stiryy);

void stiryydomemparse(char *filedata, size_t filesize, struct stiryy *stiryy);

void stiryynameparse(const char *fname, struct stiryy *stiryy, int require);

void stiryydirparse(
  const char *argv0, const char *fname, struct stiryy *stiryy, int require);

struct escaped_string yy_escape_string(char *orig);

uint32_t yy_get_ip(char *orig);

#endif

