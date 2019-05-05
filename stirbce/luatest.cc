#include <iostream>
#include "opcodes.h"
#include "stirbce.h"

std::vector<uint8_t> microprogram;
stringtab st;
memblock scope_global;

int lua_makecall(lua_State *lua) {
  if (lua_gettop(lua) == 0)
  {
    std::terminate();
  }
  const char *str = luaL_checkstring(lua, 1);
  int args = lua_gettop(lua) - 1;
  std::cout << "makecall" << std::endl;
  std::cout << "looking up " << str << std::endl;
  memblock symbol = scope_global.u.sc->recursive_lookup(str);
  std::vector<memblock> stack;
  std::cout << "symbol type " << symbol.type << std::endl;
  size_t ip = symbol.u.d;
  std::cout << "symbol ip " << ip << std::endl;


  for (int i = 0; i < args; i++)
  {
    std::cout << "pushing arg" << std::endl;
    stack.push_back(memblock(lua, 2 + i));
  }
  //stack.push_back(get_scope());
  if (microprogram[ip] != STIRBCE_OPCODE_FUN_HEADER)
  {
    std::terminate();
  }
  uint64_t u64 = (((unsigned long long)microprogram[ip+1])<<56) |
    (((unsigned long long)microprogram[ip+2])<<48) |
    (((unsigned long long)microprogram[ip+3])<<40) |
    (((unsigned long long)microprogram[ip+4])<<32) |
    (((unsigned long long)microprogram[ip+5])<<24) |
    (((unsigned long long)microprogram[ip+6])<<16) |
    (((unsigned long long)microprogram[ip+7])<<8) |
    (((unsigned long long)microprogram[ip+8])<<0);
  double d;
  memcpy(&d, &u64, 8);
  if (d != (double)args)
  {
    std::cerr << "arg count mismatch: " << d << " vs " << args << std::endl;
    std::terminate();
  }

  
  stack.push_back(-1); // return address
  engine(&microprogram[0], microprogram.size(), st, lua, scope_global, stack, ip+9);

  if (stack.size() != (size_t)args + 1)
  {
    std::terminate();
  }

  memblock rv = stack.back(); stack.pop_back();
  for (int i = 0; i < args; i++)
  {
    std::cout << "popping arg" << std::endl;
    stack.pop_back();
  }

  rv.push_lua(lua);

  std::cout << "makecall exit" << std::endl;

  return 1;
}

int luaopen_stir(lua_State *lua)
{
        static const luaL_Reg stir_lib[] = {
                {"makecall", lua_makecall},
                {NULL, NULL}};

        luaL_newlib(lua, stir_lib);
        return 1;
}

int main(int argc, char **argv)
{
  lua_State *lua = luaL_newstate();
  luaL_openlibs(lua);

  lua_pushcfunction(lua, luaopen_stir);
  lua_pushstring(lua, "Stir");
  lua_call(lua, 1, 1);
  lua_pushvalue(lua, -1);
  lua_setglobal(lua, "Stir");
  lua_pop(lua, 1);


  stringtab st;
  size_t luaidx = st.add("return Stir.makecall(\"foo\", 54321, 222)");

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
  // At this point, stack should be:
  // [arg, retaddr, retval, cnt of local variables], size 4

  memblock sc(new scope());
  scope_global = sc;
  sc.u.sc->vars["foo"] = memblock(funidx, true);

  engine(&microprogram[0], microprogram.size(), st, lua, sc);
  lua_close(lua);
  return 0;
}
