#include <iostream>
#include "opcodes.h"
#include "stirbce.h"


int main(int argc, char **argv)
{
  std::vector<uint8_t> microprogram;
  stringtab st;

  scope *sc = new scope();
  memblock scmb(sc);
  scopes_lex[sc->lua] = scmb;

  size_t luaidx = st.add("return Stir.makelexcall(\"foo\", 54321, 222)");
  size_t lua2idx = st.add("return Stir.getlexval(\"bar\")");
  size_t lua3idx = st.add("return Stir.getlexval(\"baz\")");

  std::map<std::string, memblock> m;
  m["a"] = 1.0;
  m["b"] = 2.5;
  m["c"] = memblock(new std::vector<memblock>());
  memblock mb(new std::map<std::string, memblock>(m));

  lua_State *lua = sc->lua;

  mb.push_lua(lua);
  memblock mb2(lua);
  lua_pop(lua, 1);

  if (mb2.type != memblock::T_M)
  {
    std::cout << mb2.type << std::endl;
    std::terminate();
  }
  if (mb2.u.m->size() != 3)
  {
    std::cout << "T1" << std::endl;
    std::terminate();
  }
  if ((*mb2.u.m)["a"].type != memblock::T_D ||
      (*mb2.u.m)["a"].u.d != 1.0)
  {
    std::cout << "T2" << std::endl;
    std::terminate();
  }
  if ((*mb2.u.m)["b"].type != memblock::T_D ||
      (*mb2.u.m)["b"].u.d != 2.5)
  {
    std::cout << "T3" << std::endl;
    std::terminate();
  }
  if ((*mb2.u.m)["c"].type != memblock::T_V ||
      (*mb2.u.m)["c"].u.v->size() != 0)
  {
    std::cout << "T4 " << (*mb2.u.m)["c"].type << std::endl;
    std::terminate();
  }

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, luaidx);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB);
  microprogram.push_back(STIRBCE_OPCODE_LUAEVAL);
  microprogram.push_back(STIRBCE_OPCODE_DUMP);

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, lua2idx);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB);
  microprogram.push_back(STIRBCE_OPCODE_LUAEVAL);
  microprogram.push_back(STIRBCE_OPCODE_DUMP);

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, lua3idx);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB);
  microprogram.push_back(STIRBCE_OPCODE_LUAEVAL);
  microprogram.push_back(STIRBCE_OPCODE_DUMP);

  microprogram.push_back(STIRBCE_OPCODE_EXIT);

  size_t funidx = microprogram.size();

  microprogram.push_back(STIRBCE_OPCODE_FUN_HEADER);
  store_d(microprogram, 2); // 2 args

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 1);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK);
  microprogram.push_back(STIRBCE_OPCODE_DUMP);

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 12345); // retval
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0); // cnt of local variables
  microprogram.push_back(STIRBCE_OPCODE_RETEX);

  size_t fun2idx = microprogram.size();

  microprogram.push_back(STIRBCE_OPCODE_FUN_HEADER);
  store_d(microprogram, 0); // 0 args

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 12346); // retval
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0); // cnt of local variables
  microprogram.push_back(STIRBCE_OPCODE_RETEX);

  scope_global_dyn = scmb;
  sc->vars["foo"] = memblock(funidx, true);
  sc->vars["bar"] = memblock(fun2idx, true);
  sc->vars["baz"] = memblock(12347);

  microprograms[sc->lua] = std::make_tuple(&microprogram[0], microprogram.size(), &st);

  engine(&microprogram[0], microprogram.size(), st, lua, scmb);

  return 0;
}
