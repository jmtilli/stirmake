#ifndef _DBYYUTILS_H_
#define _DBYYUTILS_H_

#include <stdio.h>
#include <stdint.h>
#include "dbyy.h"
#include "stiryy.h"

#ifdef __cplusplus
extern "C" {
#endif

void dbyydoparse(FILE *filein, struct dbyy *dbyy);

void dbyydomemparse(char *filedata, size_t filesize, struct dbyy *dbyy);

void dbyynameparse(const char *fname, struct dbyy *dbyy, int require);

void dbyydirparse(
  const char *argv0, const char *fname, struct dbyy *dbyy, int require);

#ifdef __cplusplus
};
#endif

#endif

