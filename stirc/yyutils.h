#ifndef _YYUTILS_H_
#define _YYUTILS_H_

#include <stdio.h>
#include <stdint.h>
#include "stiryy.h"

#ifdef __cplusplus
extern "C" {
#endif

int gitshas_has(const char *needle, size_t needle_len);
const char *gitversions_head(void);
const char *gitversion_get(void);
void gitversions(char *argv0);

int
engine_stringlist(struct abce *abce,
                  size_t ip,
                  const char *directive,
                  char ***strs, size_t *strsz,
                  int allow_nil,
                  int *was_array);

int stiryydoparse(FILE *filein, struct stiryy *stiryy);

#if !STIR_NO_MEMPARSE
void stiryydomemparse(char *filedata, size_t filesize, struct stiryy *stiryy);
#endif

int stiryynameparse(const char *fname, struct stiryy *stiryy, int require);

int stiryydirparse(
  const char *argv0, const char *fname, struct stiryy *stiryy, int require);

struct escaped_string yy_escape_string(char *orig, char **strendptr);

struct escaped_string yy_escape_string_single(char *orig, char **strendptr);

void file_escape_string(FILE *f, const char *str);

#ifdef __cplusplus
};
#endif

#endif

