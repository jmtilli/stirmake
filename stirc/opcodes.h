#ifndef _OPCODES_H_
#define _OPCODES_H_

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <memory>
#include <vector>
#include <map>
#include <iostream>
#include <tuple>
#include "opcodesonly.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
};

class scope;

class memblock {
 public:
  enum {
    T_D, T_S, T_V, T_M, T_F, T_N, T_B, T_SC, T_REG
  } type;
  union {
    double d;
    scope *sc;
    std::string *s;
    std::vector<memblock> *v;
    std::map<std::string, memblock> *m;
  } u;
  size_t *refc;

  memblock(): type(T_N), refc(NULL)
  {
    u.d = 0;
  }
  explicit memblock(bool b): type(T_B), refc(NULL)
  {
    u.d = b ? 1.0 : 0.0;
  }
  memblock(unsigned ui): type(T_D), refc(NULL)
  {
    u.d = ui;
  }
  memblock(int i): type(T_D), refc(NULL)
  {
    u.d = i;
  }
  memblock(long unsigned ui): type(T_D), refc(NULL)
  {
    u.d = ui;
  }
  memblock(long int i): type(T_D), refc(NULL)
  {
    u.d = i;
  }
  memblock(double d): type(T_D), refc(NULL)
  {
    u.d = d;
  }
  memblock(double d, bool isfn): type(isfn ? T_F : T_D), refc(NULL)
  {
    u.d = d;
  }
  memblock(double d, bool isfn, bool isreg): type(isreg ? T_REG : T_D), refc(NULL)
  {
    if (isfn && isreg)
    {
      std::terminate();
    }
    u.d = d;
  }
  explicit memblock(scope *sc): type(T_SC)
  {
    std::cout << "MEMBLOCK FOR SCOPE" << std::endl;
    u.sc = sc;
    refc = new size_t(1);
  }
  explicit memblock(std::string *s): type(T_S)
  {
    u.s = s;
    refc = new size_t(1);
  }
  explicit memblock(std::vector<memblock> *v): type(T_V)
  {
    u.v = v;
    refc = new size_t(1);
  }
  explicit memblock(std::map<std::string, memblock> *m): type(T_M)
  {
    u.m = m;
    refc = new size_t(1);
  }
  memblock(const memblock &other)
  {
    type = other.type;
    u = other.u;
    refc = other.refc;
    if (type == T_D || type == T_F || type == T_N || type == T_B || type == T_REG)
    {
      return;
    }
    ++*refc;
  }
  void swap(memblock &other)
  {
    std::swap(type, other.type);
    std::swap(u, other.u);
    std::swap(refc, other.refc);
  }
  memblock &operator=(const memblock &other)
  {
    memblock copy(other);
    swap(copy);
    return *this;
  }
  ~memblock();
  void dump(void)
  {
    switch (type)
    {
      case T_S:
        std::cout << "\"" << *u.s << "\"";
        break;
      case T_SC:
        std::cout << "scope";
        break;
      case T_D:
        std::cout << u.d;
        break;
      case T_V:
        std::cout << "[";
        for (size_t i = 0; i != u.v->size(); i++)
        {
          u.v->at(i).dump();
          std::cout << ", ";
        }
        std::cout << "]";
        break;
      case T_B:
        std::cout << (u.d ? "true" : "false");
        break;
      case T_N:
        std::cout << "nil";
        break;
      case T_M:
        for (auto it = u.m->begin(); it != u.m->end(); it++)
        {
          std::cout << "\"" << *u.s << "\": ";
          it->second.dump();
          std::cout << ", ";
        }
        break;
      case T_F:
        std::cerr << "fn" << std::endl;
        std::terminate(); // FIXME better error handling
      case T_REG:
        std::cerr << "reg" << std::endl;
        std::terminate(); // FIXME better error handling
    }
  }
  void push_lua(lua_State *lua)
  {
    switch (type)
    {
      case T_S:
        lua_pushlstring(lua, u.s->data(), u.s->length());
        break;
      case T_D:
        lua_pushnumber(lua, u.d);
        break;
      case T_V:
        lua_newtable(lua);
        for (size_t i = 0; i != u.v->size(); i++)
        {
          lua_pushnumber(lua, i + 1);
          u.v->at(i).push_lua(lua);
          lua_settable(lua, -3);
        }
        break;
      case T_B:
        lua_pushboolean(lua, u.d ? 1 : 0);
        break;
      case T_N:
        lua_pushnil(lua);
        break;
      case T_M:
        lua_newtable(lua);
        for (auto it = u.m->begin(); it != u.m->end(); it++)
        {
          lua_pushlstring(lua, it->first.data(), it->first.length());
          it->second.push_lua(lua);
          lua_settable(lua, -3);
        }
        break;
      case T_SC:
        std::cerr << "sc" << std::endl;
        std::terminate(); // FIXME better error handling
      case T_F:
        std::cerr << "fn" << std::endl;
        std::terminate(); // FIXME better error handling
      case T_REG:
        std::cerr << "reg" << std::endl;
        std::terminate(); // FIXME better error handling
    }
  }
  memblock(lua_State *lua, int idx = -1)
  {
    int t = lua_type(lua, idx);
    refc = NULL;
    switch (t) {
      case LUA_TSTRING:
      {
        const char *s;
        size_t l;
        type = T_S;
        s = lua_tolstring(lua, idx, &l);
        u.s = new std::string(s, l);
        refc = new size_t(1);
        break;
      }
      case LUA_TBOOLEAN:
      {
        type = T_B;
        u.d = lua_toboolean(lua, idx) ? 1.0 : 0.0;
        break;
      }
      case LUA_TNUMBER:
      {
        type = T_D;
        u.d = lua_tonumber(lua, idx);
        break;
      }
      case LUA_TTABLE:
      {
        size_t len = lua_objlen(lua, -1); // in newer versions, lua_rawlen
        refc = new size_t(1);
        if (len)
        {
          type = T_V;
          u.v = new std::vector<memblock>();
          for (size_t i = 0; i < len; i++)
          {
            lua_pushnumber(lua, i + 1);
            lua_gettable(lua, idx - 1);
            u.v->push_back(memblock(lua));
            lua_pop(lua, 1);
          }
        }
        else
        {
          type = T_M;
          u.m = new std::map<std::string, memblock>();
          lua_pushnil(lua);
          while (lua_next(lua, idx - 1) != 0)
          {
            size_t l;
            const char *s = lua_tolstring(lua, -2, &l);
            std::string str(s, l);
            (*u.m)[str] = memblock(lua);
            lua_pop(lua, 1);
          }
          if (u.m->empty()) // Exception: empty table is always an array
          {
            delete u.m;
            type = T_V;
            u.v = new std::vector<memblock>();
          }
        }
        break;
      }
      case LUA_TNIL:
      {
        type = T_N;
        u.d = 0;
        break;
      }
      case LUA_TFUNCTION:
        std::cerr << "func" << std::endl;
        std::terminate(); // FIXME better error handling
      case LUA_TUSERDATA:
        std::cerr << "ud" << std::endl;
        std::terminate(); // FIXME better error handling
      case LUA_TTHREAD:
        std::cerr << "thr" << std::endl;
        std::terminate(); // FIXME better error handling
      case LUA_TLIGHTUSERDATA:
        std::cerr << "lud" << std::endl;
        std::terminate(); // FIXME better error handling
    }
  }
};

extern std::map<lua_State*, memblock> scopes_lex;
extern memblock scope_global_dyn;
static inline memblock &get_lexscope_by_lua(lua_State *lua)
{
  if (scopes_lex.find(lua) == scopes_lex.end())
  {
    std::terminate();
  }
  memblock &mb = scopes_lex[lua];
  return mb;
}
static inline scope &get_dynscope(void)
{
  memblock &mb = scope_global_dyn;
  if (mb.type != memblock::T_SC)
  {
    std::terminate();
  }
  return *mb.u.sc;
}

int luaopen_stir(lua_State *lua);

class scope {
  public:
    scope *parent;
    bool holey;
    std::map<std::string, memblock> vars;
    lua_State *lua;

    scope(): parent(NULL), holey(true), lua(nullptr) {
      lua = luaL_newstate();
      luaL_openlibs(lua);
      lua_pushcfunction(lua, luaopen_stir);
      lua_pushstring(lua, "Stir");
      lua_call(lua, 1, 1);
      lua_pushvalue(lua, -1);
      lua_setglobal(lua, "Stir");
      lua_pop(lua, 1);
    }
    scope(scope *parent, bool holey = false): parent(parent), holey(holey) {
      lua = luaL_newstate();
      luaL_openlibs(lua);
      lua_pushcfunction(lua, luaopen_stir);
      lua_pushstring(lua, "Stir");
      lua_call(lua, 1, 1);
      lua_pushvalue(lua, -1);
      lua_setglobal(lua, "Stir");
      lua_pop(lua, 1);
    }
    ~scope() {
      lua_close(lua);
    }
    scope(const scope &other) = delete;
    scope &operator=(const scope &other) = delete;

    memblock recursive_lookup(const std::string &name)
    {
      if (vars.find(name) != vars.end())
      {
        return vars[name];
      }
      if (parent && !holey)
      {
        return parent->recursive_lookup(name);
      }
      std::cerr << "var " << name << " not found" << std::endl;
      std::terminate();
    }
};

class stringtab {
  public:
    std::vector<memblock> blocks;
    std::map<std::string, size_t> idxs;
    size_t addNonString(memblock mb)
    {
      size_t oldsz = blocks.size();
      blocks.push_back(mb);
      return oldsz;
    }
    size_t add(const std::string &s)
    {
      if (idxs.find(s) != idxs.end())
      {
        return idxs[s];
      }
      size_t oldsz = blocks.size();
      blocks.push_back(memblock(new std::string(s)));
      idxs[s] = oldsz;
      return oldsz;
    }
};

extern std::map<lua_State*, std::tuple<const uint8_t*, size_t, stringtab*> > microprograms;

static inline std::tuple<const uint8_t*, size_t, stringtab*> get_microprogram_by_lua(lua_State *lua)
{
  if (microprograms.find(lua) == microprograms.end())
  {
    std::terminate();
  }
  return microprograms[lua];
}

static inline const char *hr_opcode(uint8_t opcode)
{
  switch (opcode)
  {
    case STIRBCE_OPCODE_PUSH_DBL: return "STIRBCE_OPCODE_PUSH_DBL";
    case STIRBCE_OPCODE_CALL_EQ: return "STIRBCE_OPCODE_CALL_EQ";
    case STIRBCE_OPCODE_CALL_NE: return "STIRBCE_OPCODE_CALL_NE";
    case STIRBCE_OPCODE_CALL_LOGICAL_AND: return "STIRBCE_OPCODE_CALL_LOGICAL_AND";
    case STIRBCE_OPCODE_CALL_LOGICAL_OR: return "STIRBCE_OPCODE_CALL_LOGICAL_OR";
    case STIRBCE_OPCODE_CALL_LOGICAL_NOT: return "STIRBCE_OPCODE_CALL_LOGICAL_NOT";
    case STIRBCE_OPCODE_CALL_BITWISE_AND: return "STIRBCE_OPCODE_CALL_BITWISE_AND";
    case STIRBCE_OPCODE_CALL_BITWISE_OR: return "STIRBCE_OPCODE_CALL_BITWISE_OR";
    case STIRBCE_OPCODE_CALL_BITWISE_XOR: return "STIRBCE_OPCODE_CALL_BITWISE_XOR";
    case STIRBCE_OPCODE_CALL_BITWISE_NOT: return "STIRBCE_OPCODE_CALL_BITWISE_NOT";
    case STIRBCE_OPCODE_CALL_LT: return "STIRBCE_OPCODE_CALL_LT";
    case STIRBCE_OPCODE_CALL_GT: return "STIRBCE_OPCODE_CALL_GT";
    case STIRBCE_OPCODE_CALL_LE: return "STIRBCE_OPCODE_CALL_LE";
    case STIRBCE_OPCODE_CALL_GE: return "STIRBCE_OPCODE_CALL_GE";
    case STIRBCE_OPCODE_CALL_SHL: return "STIRBCE_OPCODE_CALL_SHL";
    case STIRBCE_OPCODE_CALL_SHR: return "STIRBCE_OPCODE_CALL_SHR";
    case STIRBCE_OPCODE_CALL_ADD: return "STIRBCE_OPCODE_CALL_ADD";
    case STIRBCE_OPCODE_CALL_SUB: return "STIRBCE_OPCODE_CALL_SUB";
    case STIRBCE_OPCODE_CALL_MUL: return "STIRBCE_OPCODE_CALL_MUL";
    case STIRBCE_OPCODE_CALL_DIV: return "STIRBCE_OPCODE_CALL_DIV";
    case STIRBCE_OPCODE_CALL_MOD: return "STIRBCE_OPCODE_CALL_MOD";
    case STIRBCE_OPCODE_CALL_UNARY_MINUS: return "STIRBCE_OPCODE_CALL_UNARY_MINUS";
    case STIRBCE_OPCODE_IF_NOT_JMP: return "STIRBCE_OPCODE_IF_NOT_JMP_FWD";
    case STIRBCE_OPCODE_JMP: return "STIRBCE_OPCODE_JMP_FWD";
    case STIRBCE_OPCODE_NOP: return "STIRBCE_OPCODE_NOP";
    case STIRBCE_OPCODE_POP: return "STIRBCE_OPCODE_POP";
    case STIRBCE_OPCODE_POP_MANY: return "STIRBCE_OPCODE_POP_MANY";
    default: abort();
  }
  abort();
}

static inline void print_microprogram(const uint8_t *microprogram, size_t sz)
{
  size_t ip = 0;
  uint8_t opcode;
  while (ip < sz)
  {
    opcode = microprogram[ip++];
    switch (opcode)
    {
      case STIRBCE_OPCODE_PUSH_DBL:
      {
        if (ip + 8 >= sz)
        {
          abort();
        }
        printf("%s ", hr_opcode(opcode));
        uint64_t u64 = (((unsigned long long)microprogram[ip+0])<<56) |
          (((unsigned long long)microprogram[ip+1])<<48) |
          (((unsigned long long)microprogram[ip+2])<<40) |
          (((unsigned long long)microprogram[ip+3])<<32) |
          (((unsigned long long)microprogram[ip+4])<<24) |
          (((unsigned long long)microprogram[ip+5])<<16) |
          (((unsigned long long)microprogram[ip+6])<<8) |
          (((unsigned long long)microprogram[ip+7])<<0);
        double d;
        memcpy(&d, &u64, 8);
        printf("%g\n", d);
        ip += 8;
        break;
      }
      default:
        printf("%s\n", hr_opcode(opcode));
        break;
    }
  }
}

#endif
