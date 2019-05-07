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
           std::vector<std::string> &backtrace,
           size_t ip = 0);

static inline int engine(lua_State *lua, memblock scope,
                         std::vector<memblock> &stack,
                         size_t ip = 0)
{
  std::vector<std::string> bt;
  return engine(lua, scope, stack, bt, ip);
}

static inline int engine(lua_State *lua, memblock scope, std::vector<std::string> &backtrace)
{
  std::vector<memblock> stack;
  return engine(lua, scope, stack, backtrace, 0);
}

static inline int engine(lua_State *lua, memblock scope)
{
  std::vector<memblock> stack;
  std::vector<std::string> bt;
  return engine(lua, scope, stack, bt, 0);
}

#endif
