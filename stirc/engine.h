#ifndef _ENGINE_H_
#define _ENGINE_H_

#include <stdint.h>
#include <stddef.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
};

int engine(lua_State *lua, memblock scope,
           std::vector<memblock> &stack,
           size_t ip = 0);

static inline int engine(lua_State *lua, memblock scope)
{
  std::vector<memblock> stack;
  return engine(lua, scope, stack);
}

#endif
