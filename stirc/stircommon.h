#ifndef _STIRCOMMON_H_
#define _STIRCOMMON_H_

#include "stiryy.h"

void add_rule(struct tgt *tgts, size_t tgtsz,
              struct dep *deps, size_t depsz,
              struct cmdsrc *shells,
              int phony, int rectgt, int detouch, int maybe, int dist,
              int cleanhook, int distcleanhook, int bothcleanhook,
              char *prefix, size_t scopeidx);

int add_dep_after_parsing_stage(char **tgts, size_t tgtsz,
                                char **deps, size_t depsz,
                                char *prefix,
                                int rec, int orderonly);

void *my_memrchr(const void *s, int c, size_t n);

#endif