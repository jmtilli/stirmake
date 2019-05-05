#ifndef _ENGINE_H_
#define _ENGINE_H_

#include <stdint.h>
#include <stddef.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
};

int engine(const uint8_t *microprogram, size_t microsz,
           stringtab &st, lua_State *lua, memblock scope,
           std::vector<memblock> &stack,
           size_t ip = 0);

static inline int engine(const uint8_t *microprogram, size_t microsz,
           stringtab &st, lua_State *lua, memblock scope)
{
  std::vector<memblock> stack;
  return engine(microprogram, microsz, st, lua, scope, stack);
}

#endif
