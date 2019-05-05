#include "opcodes.h"
#include "stirbce.h"

int main(int argc, char **argv)
{
  lua_State *lua = luaL_newstate();
  luaL_openlibs(lua);
  stringtab st;
  size_t luaidx = st.add("return {{2+3*4}}");
  std::vector<uint8_t> microprogram;

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

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, luaidx);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB);
  microprogram.push_back(STIRBCE_OPCODE_LUAEVAL);
  microprogram.push_back(STIRBCE_OPCODE_DUMP);
  engine(&microprogram[0], microprogram.size(), st, lua);
  lua_close(lua);
  return 0;
}
