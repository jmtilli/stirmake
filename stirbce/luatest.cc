#include "opcodes.h"

int main(int argc, char **argv)
{
  lua_State *lua = luaL_newstate();
  luaL_openlibs(lua);

  std::map<std::string, memblock> m;
  m["a"] = 1.0;
  m["b"] = 2.5;
  m["c"] = new std::vector<memblock>();
  memblock mb(new std::map<std::string, memblock>(m));

  mb.push_lua(lua);
  memblock mb2(lua);
  lua_pop(lua, 1);

  if (mb2.type != memblock::T_M)
  {
    std::terminate();
  }
  if (mb2.u.m->size() != 3)
  {
    std::terminate();
  }
  if ((*mb2.u.m)["a"].type != memblock::T_D ||
      (*mb2.u.m)["a"].u.d != 1.0)
  {
    std::terminate();
  }
  if ((*mb2.u.m)["b"].type != memblock::T_D ||
      (*mb2.u.m)["b"].u.d != 2.5)
  {
    std::terminate();
  }
  if ((*mb2.u.m)["c"].type != memblock::T_V ||
      (*mb2.u.m)["c"].u.v->size() != 0)
  {
    std::terminate();
  }
  lua_close(lua);
  return 0;
}
