#ifndef _JSONYYUTILS_H_
#define _JSONYYUTILS_H_

#include <stdio.h>
#include <stdint.h>
#include "jsonyy.h"

#ifdef __cplusplus
extern "C" {
#endif

int jsonyydoparse(FILE *filein, struct jsonyy *jsonyy);

int jsonyydomemparse(char *filedata, size_t filesize, struct jsonyy *jsonyy);

int jsonyynameparse(const char *fname, struct jsonyy *jsonyy);

int jsonyydirparse(
  const char *argv0, const char *fname, struct jsonyy *jsonyy);

struct json_escaped_string jsonyy_escape_string(char *orig);

#ifdef __cplusplus
};
#endif

#endif

