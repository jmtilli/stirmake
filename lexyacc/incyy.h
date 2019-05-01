#ifndef _INCYY_H_
#define _INCYY_H_

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

struct incyy {
};

static inline void incyy_free(struct incyy *incyy)
{
  memset(incyy, 0, sizeof(*incyy));
}

#endif
