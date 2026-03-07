#ifndef _STIRCOMMON_H_
#define _STIRCOMMON_H_

#include "stiryy.h"
#include <stddef.h>

int add_rule_yy(struct stiryy_main *main, struct tgt *tgts, size_t tgtsz,
                struct dep *deps, size_t depsz,
                struct cmdsrc *shells,
                int phony, int rectgt, int detouch, int maybe, int dist,
                int cleanhook, int distcleanhook, int bothcleanhook,
                int deponly,
                char *prefix, size_t scopeidx, int lineno);

int add_dep_after_parsing_stage(char **tgts, size_t tgtsz,
                                char **deps, size_t depsz,
                                char *prefix,
                                int rec, int orderonly, int wait);

void *my_memrchr(const void *s, int c, size_t n);

static inline int sizecmp(size_t size1, size_t size2)
{
  if (size1 > size2)
  {
    return 1;
  }
  if (size1 < size2)
  {
    return -1;
  }
  return 0;
}

extern int debug;

#endif
