#ifndef _JSONYY_H_
#define _JSONYY_H_

#include <stddef.h>
#include "abce/abcetrees.h"
#include "abce/abce.h"

struct jsonyy {
  struct abce *abce;
};

struct json_escaped_string {
  size_t sz;
  char *str;
};

#endif
