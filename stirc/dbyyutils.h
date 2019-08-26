#ifndef _DBYYUTILS_H_
#define _DBYYUTILS_H_

#include <stdio.h>
#include <stdint.h>
#include "dbyy.h"
#include "stiryy.h"

#ifdef __cplusplus
extern "C" {
#endif

int dbyydoparse(FILE *filein, struct dbyy *dbyy);

int dbyydomemparse(char *filedata, size_t filesize, struct dbyy *dbyy);

int dbyynameparse(const char *fname, struct dbyy *dbyy);

int dbyydirparse(
  const char *argv0, const char *fname, struct dbyy *dbyy);

#ifdef __cplusplus
};
#endif

#endif

