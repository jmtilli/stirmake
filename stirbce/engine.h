#ifndef _ENGINE_H_
#define _ENGINE_H_

#include <stdint.h>
#include <stddef.h>

int engine(const uint8_t *microprogram, size_t microsz, class stringtab &st);

#endif
