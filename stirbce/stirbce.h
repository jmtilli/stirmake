#ifndef _STIRBCE_H_
#define _STIRBCE_H_

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <exception>
#include <iostream>
#include "opcodes.h"
#include "engine.h"
#include "errno.h"

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

double get_dbl(std::vector<memblock> &stack);
int64_t get_i64(std::vector<memblock> &stack);

memblock &
get_stackloc(int64_t stackloc, size_t bp, std::vector<memblock> &stack);

int engine(const uint8_t *microprogram, size_t microsz,
           stringtab &st);

void store_d(std::vector<uint8_t> &v, double d);

#endif
