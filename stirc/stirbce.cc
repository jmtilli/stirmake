#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <exception>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <math.h>
#include <glob.h>
#include "opcodes.h"
#include "engine.h"
#include "errno.h"
#include "stirbce.h"

std::map<lua_State*, memblock> scopes_lex;
memblock scope_global_dyn;
stringtab *st_global;
const uint8_t *microprogram_global;
size_t microsz_global;

scope &scopy(const memblock &mb)
{
  if (mb.type != memblock::T_SC)
  {
    throw memblock_type_error("not a scope");
  }
  return *mb.u.sc;
}

extern "C"
int lua_makelexcall(lua_State *lua) {
  try {
    if (lua_gettop(lua) == 0)
    {
      std::terminate();
    }
    const char *str = luaL_checkstring(lua, 1);
    int args = lua_gettop(lua) - 1;
    std::cout << "makecall" << std::endl;
    std::cout << "looking up " << str << std::endl;
    memblock sc = get_lexscope_by_lua(lua);
    memblock symbol = scopy(sc).recursive_lookup(str);
    std::vector<memblock> stack;
    std::cout << "symbol type " << symbol.type << std::endl;
    size_t ip = symbol.u.d;
    std::cout << "symbol ip " << ip << std::endl;
    const uint8_t *microprogram = microprogram_global;
    const size_t microsz = microsz_global;
    stringtab *st = st_global;


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


    stack.push_back(memblock(-1, false, true)); // base pointer
    stack.push_back(memblock(-1, false, true)); // return address
    engine(lua, sc, stack, ip+9);

    if (stack.size() != (size_t)1)
    {
      std::terminate();
    }

    memblock rv = std::move(stack.back()); stack.pop_back();

    rv.push_lua(lua);

    std::cout << "makecall exit" << std::endl;

    return 1;
  }
  catch(...)
  {
    std::terminate();
  }
}

extern "C"
int lua_getlexval(lua_State *lua) {
  try {
    if (lua_gettop(lua) == 0)
    {
      std::terminate();
    }
    const char *str = luaL_checkstring(lua, 1);
    int args = lua_gettop(lua) - 1;
    if (args != 0)
    {
      std::terminate();
    }
    std::cout << "getval" << std::endl;
    std::cout << "looking up " << str << std::endl;
    memblock sc = get_lexscope_by_lua(lua);
    memblock symbol = scopy(sc).recursive_lookup(str);
    std::vector<memblock> stack;
    std::cout << "symbol type " << symbol.type << std::endl;
    const uint8_t *microprogram = microprogram_global;
    const size_t microsz = microsz_global;
    stringtab *st = st_global;

    if (symbol.type == memblock::T_F)
    {
      size_t ip = symbol.u.d;
      std::cout << "symbol ip " << ip << std::endl;
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
      if (d != (double)0.0)
      {
        std::cerr << "arg count mismatch: " << d << " vs " << 0.0 << std::endl;
        std::terminate();
      }
      stack.push_back(memblock(-1, false, true)); // base pointer
      stack.push_back(memblock(-1, false, true)); // return address
      engine(lua, sc, stack, ip+9);
      if (stack.size() != 1)
      {
        std::terminate();
      }
    }
    else
    {
      stack.push_back(symbol);
    }

    //stack.push_back(get_scope());

    memblock rv = std::move(stack.back()); stack.pop_back();

    rv.push_lua(lua);

    std::cout << "getval exit" << std::endl;

    return 1;
  }
  catch(...)
  {
    std::terminate();
  }
}


extern "C"
int luaopen_stir(lua_State *lua)
{
        static const luaL_Reg stir_lib[] = {
                {"makelexcall", lua_makelexcall},
                {"getlexval", lua_getlexval},
                {NULL, NULL}};

        luaL_newlib(lua, stir_lib);
        return 1;
}

void memblock::push_lua(lua_State *lua)
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
      throw std::invalid_argument("is a scope");
    case T_IOS:
      throw std::invalid_argument("is an iostream");
    case T_F:
      throw std::invalid_argument("is a function");
    case T_REG:
      throw std::invalid_argument("is a register");
  }
}

void memblock::dump(void)
{
  switch (type)
  {
    case T_S:
      std::cout << "\"" << *u.s << "\"";
      break;
    case T_SC:
      std::cout << "scope";
      break;
    case T_IOS:
      std::cout << "iostream";
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
      throw std::invalid_argument("is a function");
    case T_REG:
      throw std::invalid_argument("is a register");
  }
}

memblock::memblock(lua_State *lua, int idx)
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
      throw std::invalid_argument("is a lua function");
    case LUA_TUSERDATA:
      throw std::invalid_argument("is a lua userdata");
    case LUA_TTHREAD:
      throw std::invalid_argument("is a lua thread");
    case LUA_TLIGHTUSERDATA:
      throw std::invalid_argument("is a lua lightuserdata");
  }
}

memblock::~memblock()
{
  if (type == T_D || type == T_F || type == T_N || type == T_B || type == T_REG)
  {
    return;
  }
  if (--*refc == 0)
  {
    switch (type)
    {
      case T_IOS:
        delete u.ios;
        break;
      case T_SC:
        delete u.sc;
        break;
      case T_S:
        delete u.s;
        break;
      case T_V:
        delete u.v;
        break;
      case T_M:
        delete u.m;
        break;
      default:
        std::terminate();
    }
    delete refc;
  }
}

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#if 0
static int store_u64(uint64_t *s, uint64_t sp, uint8_t *v, uint64_t vsz, uint64_t loc, uint64_t val)
{
  if ((~loc) < loc)
  {
    loc = ~loc;
    if (unlikely(loc >= sp))
    {
      return 0;
    }
    s[sp - loc - 1] = val;
    return 1;
  }
  if (loc > (vsz-8)/8)
  {
    return 0;
  }
  v[loc*8+0] = val>>56;
  v[loc*8+2] = val>>48;
  v[loc*8+3] = val>>40;
  v[loc*8+3] = val>>32;
  v[loc*8+4] = val>>24;
  v[loc*8+5] = val>>16;
  v[loc*8+6] = val>>8;
  v[loc*8+7] = val;
  return 1;
}
static uint64_t load_u64(uint64_t *s, uint64_t sp, const uint8_t *v, uint64_t vsz, uint64_t loc)
{
  if ((~loc) < loc)
  {
    loc = ~loc;
    if (unlikely(loc >= sp))
    {
      return 0;
    }
    return s[sp - loc - 1];
  }
  if (loc > (vsz-8)/8)
  {
    return 0;
  }
  return (((uint64_t)v[loc*8+0])<<56) |
         (((uint64_t)v[loc*8+1])<<48) |
         (((uint64_t)v[loc*8+2])<<40) |
         (((uint64_t)v[loc*8+3])<<32) |
         (((uint64_t)v[loc*8+4])<<24) |
         (((uint64_t)v[loc*8+5])<<16) |
         (((uint64_t)v[loc*8+6])<<8) |
         v[loc*8 + 7];
}
#endif

int64_t get_funaddr(std::vector<memblock> &stack)
{
  memblock mb = std::move(stack.back()); stack.pop_back();
  if (mb.type != memblock::T_F)
  {
    throw memblock_type_error("not a function address");
  }
  return mb.u.d;
}
int64_t get_reg(std::vector<memblock> &stack)
{
  memblock mb = std::move(stack.back()); stack.pop_back();
  if (mb.type != memblock::T_REG)
  {
    throw memblock_type_error("not a register");
  }
  return mb.u.d;
}

bool endswith(const std::string &s1, std::string os)
{
  size_t off;
  if (s1.size() < os.size())
  {
    return false;
  }
  off = s1.size() - os.size();
  if (s1.substr(off) != os)
  {
    return false;
  }
  return true;
}

class WordIterator {
  public:
    std::string base;
    std::string sep;
    size_t idx;

    WordIterator(std::string base, std::string sep):
      base(base), sep(sep), idx(0)
    {
      idx = base.find_first_not_of(sep, idx);
    }

    std::string operator*(void)
    {
      size_t idx2 = base.find_first_of(sep, idx);
      if (idx2 == std::string::npos)
      {
        return base.substr(idx);
      }
      return base.substr(idx, idx2 - idx);
    }

    bool end()
    {
      return idx == std::string::npos;
    }

    const WordIterator &operator++(void) // prefix
    {
      idx = base.find_first_of(sep, idx);
      if (idx == std::string::npos)
      {
        return *this;
      }
      idx = base.find_first_not_of(sep, idx);
      return *this;
    }
    WordIterator operator++(int) // postfix
    {
      WordIterator res(*this);
      ++(*this);
      return res;
    }
};

std::string strfmt(const std::string &fmt, std::vector<memblock> argsrev)
{
  std::ostringstream ress;
  size_t curpos = 0;
  for (;;)
  {
    size_t newposstart = fmt.find("%", curpos);
    std::ostringstream tmpss;
    std::ostringstream tmpss2;
    long int w = -1;
    long int prec = -1;
    if (newposstart == std::string::npos)
    {
      ress << fmt.substr(curpos);
      break;
    }
    ress << fmt.substr(curpos, newposstart - curpos);
    curpos = newposstart;
    if (curpos >= fmt.size() - 1)
    {
      throw std::invalid_argument("invalid format string");
    }
    if (argsrev.empty())
    {
      throw std::invalid_argument("too few arguments");
    }
    curpos++;
    if (isdigit(fmt[curpos]))
    {
      char *endptr;
      w = strtol(&fmt[curpos], &endptr, 10);
      curpos += (endptr - &fmt[curpos]);
    }
    if (fmt[curpos] == '.')
    {
      curpos++;
      if (!isdigit(fmt[curpos]))
      {
        throw std::invalid_argument("invalid format string");
      }
      char *endptr;
      prec = strtol(&fmt[curpos], &endptr, 10);
      curpos += (endptr - &fmt[curpos]);
    }
    // FIXME verify sanity of format
    switch (fmt[curpos])
    {
      case 'E':
        tmpss << std::uppercase;
      case 'e':
        if (argsrev.back().type != memblock::T_D)
        {
          throw std::invalid_argument("arg not numeric");
        }
        tmpss << std::scientific;
        if (prec >= 0)
        {
          tmpss << std::setprecision(prec);
        }
        tmpss << argsrev.back().u.d;
        if (w >= 0)
        {
          tmpss2 << std::setw(w);
        }
        tmpss2 << tmpss.str();
        ress << tmpss.str();
        break;
      case 'f':
        if (argsrev.back().type != memblock::T_D)
        {
          throw std::invalid_argument("arg not numeric");
        }
        tmpss << std::fixed;
        if (prec >= 0)
        {
          tmpss << std::setprecision(prec);
        }
        tmpss << argsrev.back().u.d;
        if (w >= 0)
        {
          tmpss2 << std::setw(w);
        }
        tmpss2 << tmpss.str();
        ress << tmpss.str();
        break;
      case 'g':
        if (argsrev.back().type != memblock::T_D)
        {
          throw std::invalid_argument("arg not numeric");
        }
        if (prec >= 0)
        {
          tmpss << std::setprecision(prec);
        }
        tmpss << argsrev.back().u.d;
        if (w >= 0)
        {
          tmpss2 << std::setw(w);
        }
        tmpss2 << tmpss.str();
        ress << tmpss.str();
        break;
      case 'd':
      case 'i':
        if (argsrev.back().type != memblock::T_D)
        {
          throw std::invalid_argument("arg not numeric");
        }
        if (prec >= 0)
        {
          tmpss << std::setfill('0') << std::setw(prec);
        }
        tmpss << (int64_t)argsrev.back().u.d;
        if (w >= 0)
        {
          tmpss2 << std::setw(w);
        }
        tmpss2 << tmpss.str();
        ress << tmpss.str();
        break;
      case 'c':
        if (argsrev.back().type != memblock::T_D)
        {
          throw std::invalid_argument("arg not numeric");
        }
        if (prec >= 0)
        {
          tmpss << std::setfill(' ') << std::setw(prec);
        }
        tmpss << (char)argsrev.back().u.d;
        if (w >= 0)
        {
          tmpss2 << std::setw(w);
        }
        tmpss2 << tmpss.str();
        ress << tmpss2.str();
        break;
      case 'X': // hex uppercase
        tmpss << std::uppercase;
      case 'x': // hex
        if (argsrev.back().type != memblock::T_D)
        {
          throw std::invalid_argument("arg not numeric");
        }
        if (prec >= 0)
        {
          tmpss << std::setfill('0') << std::setw(prec);
        }
        tmpss << std::hex << (uint64_t)argsrev.back().u.d;
        if (w >= 0)
        {
          tmpss2 << std::setw(w);
        }
        tmpss2 << tmpss.str();
        ress << tmpss2.str();
        break;
      case 'o': // octal
        if (argsrev.back().type != memblock::T_D)
        {
          throw std::invalid_argument("arg not numeric");
        }
        if (prec >= 0)
        {
          tmpss << std::setfill('0') << std::setw(prec);
        }
        tmpss << std::oct << (uint64_t)argsrev.back().u.d;
        if (w >= 0)
        {
          tmpss2 << std::setw(w);
        }
        tmpss2 << tmpss.str();
        ress << tmpss2.str();
        break;
      case 'u':
        if (argsrev.back().type != memblock::T_D)
        {
          throw std::invalid_argument("arg not numeric");
        }
        if (prec >= 0)
        {
          tmpss << std::setfill('0') << std::setw(prec);
        }
        tmpss << (uint64_t)argsrev.back().u.d;
        if (w >= 0)
        {
          tmpss2 << std::setw(w);
        }
        tmpss2 << tmpss.str();
        ress << tmpss2.str();
        break;
      case 's':
        if (argsrev.back().type != memblock::T_S)
        {
          throw std::invalid_argument("arg not string");
        }
        if (prec >= 0)
        {
          tmpss << std::setfill('0') << std::setw(prec);
        }
        tmpss << *argsrev.back().u.s;
        if (w >= 0)
        {
          tmpss2 << std::setw(w);
        }
        tmpss2 << tmpss.str();
        ress << tmpss2.str();
        break;
      case 'a':
        throw std::invalid_argument("unsupported %%a format, TODO implement");
      case 'A':
        throw std::invalid_argument("unsupported %%A format, TODO implement");
      case 'q':
        throw std::invalid_argument("unsupported %%q format, TODO implement");
      default:
        throw std::invalid_argument("unsupported format type");
    }
    argsrev.pop_back();
  }
  return ress.str();
}

std::string strreplace(const std::string &haystack, const std::string &needle, const std::string &replacement)
{
  std::ostringstream ress;
  size_t curpos = 0;
  for (;;)
  {
    size_t newposstart = haystack.find(needle, curpos);
    if (newposstart == std::string::npos)
    {
      ress << haystack.substr(curpos);
      break;
    }
    ress << haystack.substr(curpos, newposstart - curpos);
    ress << replacement;
    curpos = newposstart + needle.length();
  }
  return ress.str();
}

std::string path_simplify(const std::string &base)
{
  bool isabs = false;
  std::vector<std::string> components;
  std::string res;
  if (base.length() == 0)
  {
    throw std::invalid_argument("invalid path");
  }
  if (base[0] == '/')
  {
    isabs = true;
  }
  for (auto it = WordIterator(base, "/"); !it.end(); it++)
  {
    if (*it == ".")
    {
      continue;
    }
    if (*it == "..")
    {
      if (components.size() > 0 && components.back() != "..")
      {
        components.pop_back();
      }
      else if (!isabs)
      {
        components.push_back(*it);
      }
      continue;
    }
    components.push_back(*it);
  }
  if (isabs)
  {
    res += "/";
  }
  for (auto it = components.begin(); it != components.end(); it++)
  {
    if (it != components.begin())
    {
      res += "/";
    }
    res += *it;
  }
  return res;
}


size_t wordcnt(const std::string &base, const std::string &sep)
{
  size_t ret = 0;
  for (auto it = WordIterator(base, sep); !it.end(); it++)
  {
    ret++;
  }
  return ret;
}
std::string wordget(const std::string &base, const std::string &sep, size_t word)
{
  size_t ret = 0;
  for (auto it = WordIterator(base, sep); !it.end(); it++)
  {
    if (ret == word)
    {
      return *it;
    }
    ret++;
  }
  throw std::out_of_range("word index");
}



std::string strstrip(const std::string &orig, const std::string &specials)
{
  size_t pos2 = orig.find_last_not_of(" \t");
  size_t pos1 = orig.find_first_not_of(" \t");
  if (pos2 != std::string::npos && pos1 != std::string::npos)
  {
    return orig.substr(0, pos2+1).substr(pos1);
  }
  else
  {
    return "";
  }
}
double mystrstr(const std::string &haystack, const std::string &needle)
{
  size_t off;
  off = haystack.find(needle);
  if (off == std::string::npos)
  {
    return -1;
  }
  return off;
}
std::string sufsub(const std::string &s1, std::string os, std::string ns)
{
  size_t off;
  if (s1.size() < os.size())
  {
    throw std::invalid_argument("too short string");
  }
  off = s1.size() - os.size();
  if (s1.substr(off) != os)
  {
    throw std::invalid_argument("suffix not found");
  }
  return s1.substr(0, off) + ns;
}
std::string pathsuffix(const std::string &s1)
{
  size_t off = s1.rfind('.');
  if (off == std::string::npos)
  {
    return "";
  }
  return s1.substr(off);
}
std::string pathbasename(const std::string &s1)
{
  size_t off = s1.rfind('.');
  if (off == std::string::npos)
  {
    return s1;
  }
  return s1.substr(0, off);
}
std::string pathnotdir(const std::string &s1)
{
  size_t off = s1.rfind('/');
  if (off == std::string::npos)
  {
    return s1;
  }
  return s1.substr(off + 1);
}
std::string pathdir(const std::string &s1)
{
  size_t off = s1.rfind('/');
  if (off == std::string::npos)
  {
    return "./";
  }
  return s1.substr(0, off + 1);
}

std::string get_str(std::vector<memblock> &stack)
{
  memblock mb = std::move(stack.back()); stack.pop_back();
  if (mb.type != memblock::T_S)
  {
    throw memblock_type_error("not a string");
  }
  return *mb.u.s;
}
double get_dbl(std::vector<memblock> &stack)
{
  memblock mb = std::move(stack.back()); stack.pop_back();
  if (mb.type != memblock::T_D && mb.type != memblock::T_B)
  {
    throw memblock_type_error("not a double or boolean");
  }
  return mb.u.d;
}
int64_t get_i64(std::vector<memblock> &stack)
{
  return get_dbl(stack);
}

memblock &
get_stackloc(int64_t stackloc, size_t bp, std::vector<memblock> &stack)
{
  if (stackloc >= 0)
  {
    return stack.at(bp + stackloc);
  }
  return stack.at(stack.size() + stackloc);
}

std::string fun_stringify(size_t ip)
{
  while (ip < microsz_global && microprogram_global[ip] != STIRBCE_OPCODE_FUN_TRAILER)
  {
    if (microprogram_global[ip] == STIRBCE_OPCODE_PUSH_DBL)
    {
      ip += 9;
    }
    else if (microprogram_global[ip] == STIRBCE_OPCODE_FUN_HEADER)
    {
      return "forgotten trailer 1";
    }
    else
    {
      ip++;
    }
  }
  if (microprogram_global[ip] != STIRBCE_OPCODE_FUN_TRAILER || ip+9 > microsz_global)
  {
    return "forgotten trailer 2";
  }
  uint64_t u64 = 
                ((((uint64_t)microprogram_global[ip+1])<<56) |
                (((uint64_t)microprogram_global[ip+2])<<48) |
                (((uint64_t)microprogram_global[ip+3])<<40) |
                (((uint64_t)microprogram_global[ip+4])<<32) |
                (((uint64_t)microprogram_global[ip+5])<<24) |
                (((uint64_t)microprogram_global[ip+6])<<16) |
                (((uint64_t)microprogram_global[ip+7])<<8) |
                            microprogram_global[ip+8]);
  double dbl;
  size_t idx;
  memcpy(&dbl, &u64, 8);
  idx = dbl;
  if (idx >= st_global->blocks.size())
  {
    return "invalid trailer 1";
  }
  memblock &mb = st_global->blocks.at(idx);
  if (mb.type != memblock::T_S)
  {
    return "invalid trailer 2";
  }
  return *mb.u.s;
}

void stringescape(const std::string &str, std::ostream &oss)
{
  oss << "\"";
  for (auto it = str.begin(); it != str.end(); it++)
  {
    char ch = *it;
    if (ch == '\\')
      oss << "\\\\";
    else if (ch == '\n')
      oss << "\\n";
    else if (ch == '\r')
      oss << "\\r";
    else if (ch == '\t')
      oss << "\\t";
    else if (ch == '\b')
      oss << "\\b";
    else if (ch == '\f')
      oss << "\\f";
    else if (ch == '"')
      oss << "\\\"";
    else if ((unsigned char)ch <= 0x0F)
      oss << "\\u000" << std::hex << (int)(unsigned char)ch << std::dec;
    else if ((unsigned char)ch <= 0x1F)
      oss << "\\u001" << std::hex << (int)(unsigned char)(ch-0x10) << std::dec;
    else
      oss << ch;
  }
  oss << str;
  oss << "\"";
}

void tojson(memblock mb, std::ostream &oss)
{
  switch (mb.type)
  {
    case memblock::T_S:
      stringescape(*mb.u.s, oss);
      break;
    case memblock::T_N:
      oss << "null";
      break;
    case memblock::T_B:
      oss << (mb.u.d ? "true" : "false");
      break;
    case memblock::T_D:
      if ((double)(int64_t)mb.u.d == mb.u.d)
      {
        oss << (int64_t)mb.u.d;
      }
      else if (isnan(mb.u.d) || isinf(mb.u.d))
      {
        throw std::invalid_argument("non-finite number");
      }
      else
      {
        oss << std::setprecision(20) << mb.u.d;
      }
      break;
    case memblock::T_V:
      oss << "[";
      for (auto it = mb.u.v->begin(); it != mb.u.v->end(); it++)
      {
        if (it != mb.u.v->begin())
        {
          oss << ",";
        }
        tojson(*it, oss);
      }
      oss << "]";
      break;
    case memblock::T_M:
      oss << "{";
      for (auto it = mb.u.m->begin(); it != mb.u.m->end(); it++)
      {
        if (it != mb.u.m->begin())
        {
          oss << ",";
        }
        stringescape(it->first, oss);
        oss << ":";
        tojson(it->second, oss);
      }
      oss << "}";
      break;
    default:
      throw std::invalid_argument("can't serialize to JSON");
  }
}

std::string tojson(memblock mb)
{
  std::ostringstream oss;
  tojson(mb, oss);
  return oss.str();
}

void create_backtrace(std::vector<memblock> &stack, std::vector<std::string> &backtrace, size_t ip)
{
  backtrace.push_back(fun_stringify(ip));
  while (stack.size() >= 2)
  {
    if (stack[stack.size() - 1].type == memblock::T_REG &&
        stack[stack.size() - 2].type == memblock::T_REG)
    {
      backtrace.push_back(fun_stringify(stack[stack.size() - 1].u.d));
      stack.pop_back();
      stack.pop_back();
      continue;
    }
    stack.pop_back();
  }
}

memblock file_get(memblock &stream, double delim, double maxcnt)
{
  size_t toget = 0;
  char delim_ch = 0;
  int delim_nan = 0;
  int maxcnt_inf = 0;
  memblock mb;
  if (stream.type != memblock::T_IOS)
  {
    throw memblock_type_error("file_get: arg not stream");
  }
  if (stream.u.ios->ios == NULL)
  {
    throw std::runtime_error("file_flush: arg already closed");
  }
  if (isnan(delim))
  {
    delim_nan = 1;
  }
  else
  {
    delim_ch = (char)(unsigned char)delim;
    if ((double)(unsigned char)delim_ch != delim)
    {
      throw std::invalid_argument("not a character");
    }
  }
  if (isinf(maxcnt))
  {
    if (maxcnt < 0)
    {
      throw std::invalid_argument("maxcnt < 0");
    }
    maxcnt_inf = 1;
  }
  else if (!isfinite(maxcnt) || maxcnt < 0)
  {
    throw std::invalid_argument("maxcnt negative infinity");
  }
  else
  {
    toget = maxcnt;
    if ((double)toget != maxcnt)
    {
      throw std::invalid_argument("maxcnt not an integer");
    }
  }
  if (!delim_nan && !maxcnt_inf) // delim set && maxcnt set
  {
    throw std::invalid_argument("delim&maxcnt set not supported yet");
  }
  if (delim_nan && maxcnt_inf) // delim not set && maxcnt not set
  {
    std::ostringstream oss;
    std::vector<char> buf;
    buf.resize(32768);
    while (stream.u.ios->ios->good())
    {
      stream.u.ios->ios->read(&buf[0], buf.size());
      oss.write(&buf[0], stream.u.ios->ios->gcount());
    }
    mb = memblock(new std::string(oss.str()));
  }
  else if (!delim_nan) // delim set
  {
    std::string str;
    getline(*stream.u.ios->ios, str, delim_ch);
    mb = memblock(new std::string(str));
  }
  else // maxcnt set
  {
    std::vector<char> buf;
    buf.resize(toget);
    stream.u.ios->ios->read(&buf[0], toget);
    buf.resize(stream.u.ios->ios->gcount());
    mb = memblock(new std::string(&buf[0], buf.size()));
  }
  if ((stream.u.ios->ios->fail() || stream.u.ios->ios->eof()) &&
      !stream.u.ios->ios->bad())
  {
    stream.u.ios->ios->clear();
  }
  if (stream.u.ios->ios->fail())
  {
    throw io_error("file_get: stream in error");
  }
  //stream.u.ios->ios->seekp(act_len, std::ios_base::cur); // RFE correct?
  stream.u.ios->ios->clear();
  return mb;
}

memblock file_open(const std::string &name, int64_t mode)
{
  std::unique_ptr<std::iostream> ptr;
  std::ios_base::openmode cppmode;
  if ((mode&3) == 1)
  {
    cppmode = std::ios_base::in;
  }
  else if ((mode&3) == 2)
  {
    cppmode = std::ios_base::out;
  }
  else if ((mode&3) == 3)
  {
    cppmode = std::ios_base::in | std::ios_base::out;
  }
  else
  {
    throw std::invalid_argument("invalid file open mode");
  }
  if (mode&(1<<2))
  {
    if ((mode&3) == 1)
    {
      throw std::invalid_argument("can't append to read-only file");
    }
    cppmode |= std::ios_base::app;
  }
  if (mode&(1<<3))
  {
    if ((mode&3) == 1)
    {
      throw std::invalid_argument("can't truncate read-only file");
    }
    cppmode |= std::ios_base::trunc;
  }
  ptr.reset(new std::fstream(name.c_str(), cppmode));
  if (!ptr->good())
  {
    throw io_error("can't open file");
  }
  memblock mb(new ioswrapper(ptr.release()));
  return mb;
}

std::string mbtostring(memblock &mb)
{
  std::ostringstream oss;
  switch (mb.type)
  {
    case memblock::T_D: oss << mb.u.d; break;
    case memblock::T_B: oss << (mb.u.d ? "true" : "false"); break;
    case memblock::T_F: oss << "function: " << mb.u.d; break;
    case memblock::T_V: oss << "vector: " << (void*)(mb.u.v); break;
    case memblock::T_M: oss << "map: " << (void*)(mb.u.m); break;
    case memblock::T_N: oss << "null"; break;
    case memblock::T_S: oss << mb.u.s; break;
    case memblock::T_SC: oss << "scope: " << (void*)(mb.u.sc); break;
    case memblock::T_IOS: oss << "iostream: " << (void*)(mb.u.ios); break;
    case memblock::T_REG: oss << "register: " << mb.u.d; break;
    default: oss << "UNKNOWN"; break;
  }
  return oss.str();
}

int engine(lua_State *lua, memblock scope,
           std::vector<memblock> &stack, std::vector<std::string> &backtrace, size_t ip)
{
  try {
    try {
      // 0.53 us / execution
      int ret = 0;
      int64_t val, val2, condition, jmp;
      uint16_t opcode = 0;
      size_t bp = 0;
      const size_t stackbound = 131072;

      ret = -EAGAIN;
      while (ret == -EAGAIN && ip < microsz_global)
      {
        uint8_t ophi = microprogram_global[ip++];
        if (likely(ophi < 128))
        {
          opcode = ophi;
        }
        else if (unlikely((ophi & 0xC0) == 0x80))
        {
          throw illegal_sequence("illegal byte sequence");
        }
        else if (likely((ophi & 0xE0) == 0xC0))
        {
          uint8_t oplo;
          if (unlikely(ip >= microsz_global))
          {
            throw std::overflow_error("microcode overflow");
          }
          oplo = microprogram_global[ip++];
          if (unlikely((oplo & 0xC0) != 0x80))
          {
            throw illegal_sequence("illegal byte sequence");
          }
          opcode = ((ophi&0x1F) << 6) | (oplo & 0x3F);
          if (unlikely(opcode < 128))
          {
            throw illegal_sequence("illegal byte sequence");
          }
        }
        else if (likely((ophi & 0xF0) == 0xE0))
        {
          uint8_t opmid, oplo;
          if (unlikely(ip >= microsz_global))
          {
            throw illegal_sequence("illegal byte sequence");
          }
          opmid = microprogram_global[ip++];
          if (unlikely((opmid & 0xC0) != 0x80))
          {
            throw illegal_sequence("illegal byte sequence");
          }
          oplo = microprogram_global[ip++];
          if (unlikely((oplo & 0xC0) != 0x80))
          {
            throw illegal_sequence("illegal byte sequence");
          }
          opcode = ((ophi&0xF) << 12) | ((opmid&0x3F) << 6) | (oplo & 0x3F);
          if (unlikely(opcode <= 0x7FF))
          {
            throw illegal_sequence("illegal byte sequence");
          }
        }
        else
        {
          throw illegal_sequence("illegal byte sequence");
        }
        std::cout << "ip " << (ip-1) << " instr " << (int)opcode << std::endl;
        switch ((enum stirbce_opcode)opcode)
        {
          case STIRBCE_OPCODE_TYPE:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            stack.push_back((double)(int)mb.type);
            break;
          }
          case STIRBCE_OPCODE_JMP:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            jmp = get_i64(stack);
            if (unlikely(ip + jmp > microsz_global))
            {
              throw std::overflow_error("microprogram overflow");
            }
            ip += jmp;
            break;
          case STIRBCE_OPCODE_LUAPUSH:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.back().push_lua(lua);
            break;
          }
          case STIRBCE_OPCODE_LUAEVAL:
          {
            int top = lua_gettop(lua);
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            if (mb.type != memblock::T_S)
            {
              throw memblock_type_error("not a string");
            }
            luaL_dostring(lua, mb.u.s->c_str());
            if (lua_gettop(lua) == top)
            {
              lua_pushnil(lua);
            }
            else if (lua_gettop(lua) != top + 1)
            {
              throw std::runtime_error("lua multi-return not supported");
            }
            stack.push_back(lua);
            break;
          }
          case STIRBCE_OPCODE_CALL_IF_FUN:
          {
            if (unlikely(stack.size() < 1 || stack.size() >= stackbound))
            {
              throw std::underflow_error("stack underflow");
            }
            if (stack.back().type == memblock::T_F)
            {
              jmp = get_funaddr(stack);
              if (jmp < 0 || (size_t)jmp + 9 > microsz_global) // FIXME off-by-one?
              {
                throw std::overflow_error("microprogram overflow");
              }
              std::cout << "call, stack size " << stack.size() << " jmp " << jmp << " usz " << microsz_global << std::endl;
              std::cout << "instr " << (int)microprogram_global[jmp] << std::endl;
              if (microprogram_global[jmp] != STIRBCE_OPCODE_FUN_HEADER)
              {
                throw std::invalid_argument("not a function address");
              }
              uint64_t u64 = 
                            ((((uint64_t)microprogram_global[jmp+1])<<56) |
                            (((uint64_t)microprogram_global[jmp+2])<<48) |
                            (((uint64_t)microprogram_global[jmp+3])<<40) |
                            (((uint64_t)microprogram_global[jmp+4])<<32) |
                            (((uint64_t)microprogram_global[jmp+5])<<24) |
                            (((uint64_t)microprogram_global[jmp+6])<<16) |
                            (((uint64_t)microprogram_global[jmp+7])<<8) |
                                        microprogram_global[jmp+8]);
              double dbl;
              memcpy(&dbl, &u64, 8);
              if (dbl != (double)0.0)
              {
                throw std::invalid_argument("function argument count not correct");
              }
              stack.push_back(memblock(bp, false, true));
              stack.push_back(memblock(ip, false, true));
              bp = stack.size() - 2 - 0; // 0 arguments, 2 pushed registers in stack
              ip = jmp + 9;
            }
            break;
          }
          case STIRBCE_OPCODE_STRFMT:
          {
            int64_t varargcnt;
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            varargcnt = get_i64(stack);
            if (varargcnt < 0)
            {
              throw std::invalid_argument("negative argument count");
            }
            if (unlikely(stack.size() < 1 + (size_t)varargcnt))
            {
              throw std::underflow_error("stack underflow");
            }
            std::vector<memblock> argsrev;
            for (int64_t idx = 0; idx < varargcnt; idx++)
            {
              argsrev.push_back(stack.back());
              stack.pop_back();
            }
            std::string fmtstr = get_str(stack);
            stack.push_back(memblock(new std::string(strfmt(fmtstr, argsrev))));
            break;
          }
          case STIRBCE_OPCODE_CALL:
          {
            int64_t argcnt;
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            argcnt = get_i64(stack);
            if (argcnt < 0)
            {
              throw std::invalid_argument("negative argument count");
            }
            jmp = get_funaddr(stack);
            if (jmp < 0 || (size_t)jmp + 9 > microsz_global) // FIXME off-by-one?
            {
              throw std::overflow_error("microprogram overflow");
            }
            std::cout << "call, stack size " << stack.size() << " jmp " << jmp << " usz " << microsz_global << std::endl;
            std::cout << "instr " << (int)microprogram_global[jmp] << std::endl;
            if (microprogram_global[jmp] != STIRBCE_OPCODE_FUN_HEADER)
            {
              throw std::invalid_argument("not a function");
            }
            uint64_t u64 = 
                          ((((uint64_t)microprogram_global[jmp+1])<<56) |
                          (((uint64_t)microprogram_global[jmp+2])<<48) |
                          (((uint64_t)microprogram_global[jmp+3])<<40) |
                          (((uint64_t)microprogram_global[jmp+4])<<32) |
                          (((uint64_t)microprogram_global[jmp+5])<<24) |
                          (((uint64_t)microprogram_global[jmp+6])<<16) |
                          (((uint64_t)microprogram_global[jmp+7])<<8) |
                                      microprogram_global[jmp+8]);
            double dbl;
            memcpy(&dbl, &u64, 8);
            if (dbl != (double)argcnt)
            {
              throw std::invalid_argument("function argument count not correct");
            }
            stack.push_back(memblock(bp, false, true));
            stack.push_back(memblock(ip, false, true));
            bp = stack.size() - 2 - argcnt;
            ip = jmp + 9;
            break;
          }
          case STIRBCE_OPCODE_EXIT:
          {
            std::cout << "exit, stack size " << stack.size() << std::endl;
            ip = microsz_global;
            break;
          }
          case STIRBCE_OPCODE_SUFSUB:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            std::string ns = get_str(stack);
            std::string os = get_str(stack);
            memblock mb = std::move(stack.back()); stack.pop_back();
            memblock mb2;
            if (mb.type == memblock::T_S)
            {
              mb2 = memblock(memblock(new std::string(sufsub(*mb.u.s, os, ns))));
            }
            else if (mb.type == memblock::T_V)
            {
              mb2 = memblock(memblock(new std::vector<memblock>()));
              for (auto it = mb.u.v->begin(); it != mb.u.v->end(); it++)
              {
                if (it->type != memblock::T_S)
                {
                  throw memblock_type_error("pathdir: list not only strings");
                }
                mb2.u.v->push_back(memblock(new std::string(sufsub(*it->u.s, os, ns))));
              }
            }
            else
            {
              throw memblock_type_error("pathdir: neither vector nor string");
            }
            stack.push_back(mb2);
            break;
          }
          case STIRBCE_OPCODE_SUFFILTER:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            int reverse = get_dbl(stack) ? 1 : 0;
            std::string suf = get_str(stack);
            memblock mb = std::move(stack.back()); stack.pop_back();
            memblock mb2;
            if (mb.type == memblock::T_V)
            {
              mb2 = memblock(memblock(new std::vector<memblock>()));
              for (auto it = mb.u.v->begin(); it != mb.u.v->end(); it++)
              {
                if (it->type != memblock::T_S)
                {
                  throw memblock_type_error("suffilter: list not only strings");
                }
                if (!!endswith(*it->u.s, suf) == !!reverse)
                {
                  mb2.u.v->push_back(*it);
                }
              }
            }
            else
            {
              throw memblock_type_error("suffilter: arg not vector");
            }
            stack.push_back(mb2);
            break;
          }
          case STIRBCE_OPCODE_JSON_ENCODE:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            stack.push_back(memblock(new std::string(tojson(mb))));
            break;
          }
          case STIRBCE_OPCODE_PATH_SUFFIX:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            memblock mb2;
            if (mb.type == memblock::T_S)
            {
              mb2 = memblock(memblock(new std::string(pathsuffix(*mb.u.s))));
            }
            else if (mb.type == memblock::T_V)
            {
              mb2 = memblock(memblock(new std::vector<memblock>()));
              for (auto it = mb.u.v->begin(); it != mb.u.v->end(); it++)
              {
                if (it->type != memblock::T_S)
                {
                  throw memblock_type_error("pathsuffix: list not only strings");
                }
                mb2.u.v->push_back(memblock(new std::string(pathsuffix(*it->u.s))));
              }
            }
            else
            {
              throw memblock_type_error("pathsuffix: neither vector not string");
            }
            stack.push_back(mb2);
            break;
          }
          case STIRBCE_OPCODE_PATH_BASENAME:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            memblock mb2;
            if (mb.type == memblock::T_S)
            {
              mb2 = memblock(memblock(new std::string(pathbasename(*mb.u.s))));
            }
            else if (mb.type == memblock::T_V)
            {
              mb2 = memblock(memblock(new std::vector<memblock>()));
              for (auto it = mb.u.v->begin(); it != mb.u.v->end(); it++)
              {
                if (it->type != memblock::T_S)
                {
                  throw memblock_type_error("basename: list not only strings");
                }
                mb2.u.v->push_back(memblock(new std::string(pathbasename(*it->u.s))));
              }
            }
            else
            {
              throw memblock_type_error("basename: neither vector nor string");
            }
            stack.push_back(mb2);
            break;
          }
          case STIRBCE_OPCODE_PATH_NOTDIR:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            memblock mb2;
            if (mb.type == memblock::T_S)
            {
              mb2 = memblock(memblock(new std::string(pathnotdir(*mb.u.s))));
            }
            else if (mb.type == memblock::T_V)
            {
              mb2 = memblock(memblock(new std::vector<memblock>()));
              for (auto it = mb.u.v->begin(); it != mb.u.v->end(); it++)
              {
                if (it->type != memblock::T_S)
                {
                  throw memblock_type_error("notdir: list not only strings");
                }
                mb2.u.v->push_back(memblock(new std::string(pathnotdir(*it->u.s))));
              }
            }
            else
            {
              throw memblock_type_error("notdir: neither vector nor string");
            }
            stack.push_back(mb2);
            break;
          }
          case STIRBCE_OPCODE_PATH_DIR:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            memblock mb2;
            if (mb.type == memblock::T_S)
            {
              mb2 = memblock(memblock(new std::string(pathdir(*mb.u.s))));
            }
            else if (mb.type == memblock::T_V)
            {
              mb2 = memblock(memblock(new std::vector<memblock>()));
              for (auto it = mb.u.v->begin(); it != mb.u.v->end(); it++)
              {
                if (it->type != memblock::T_S)
                {
                  throw memblock_type_error("dir: list not only strings");
                }
                mb2.u.v->push_back(memblock(new std::string(pathdir(*it->u.s))));
              }
            }
            else
            {
              throw memblock_type_error("dir: neither vector nor string");
            }
            stack.push_back(mb2);
            break;
          }
          case STIRBCE_OPCODE_OUT:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            std::string str = get_str(stack);
            val = get_dbl(stack);
            if (val)
            {
              std::cerr << str << std::endl;
            }
            else
            {
              std::cout << str << std::endl;
            }
            break;
          }
          case STIRBCE_OPCODE_ERROR:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            std::string str = get_str(stack);
            std::cerr << str << std::endl;
            throw custom_error("error instruction encountered");
            break;
          }
          case STIRBCE_OPCODE_DUMP:
          {
            std::cout << "dump, stack size " << stack.size() << std::endl;
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            mb.dump();
            std::cout << std::endl;
            break;
          }
          case STIRBCE_OPCODE_RETEX2:
          {
            std::cout << "retex2/1, stack size " << stack.size() << std::endl;
            if (unlikely(stack.size() < 5))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t cnt = get_i64(stack); // Count of local vars
            if (cnt < 0)
            {
              throw std::invalid_argument("cnt < 0");
            }
            int64_t cntarg = get_i64(stack); // Count of args
            if (cntarg < 0)
            {
              throw std::invalid_argument("cntarg < 0");
            }
            if ((size_t)(cntarg+cnt)+3 > stack.size())
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            for (int i = 0; i < cnt; i++)
            {
              stack.pop_back(); // Clear local variable block
            }
            jmp = get_reg(stack);
            if (jmp == -1)
            {
              jmp = microsz_global;
            }
            if (unlikely(jmp < 0 || (size_t)jmp > microsz_global))
            {
              throw std::overflow_error("microprogram overflow");
            }
            bp = get_reg(stack);
            std::cout << "retex2/0, stack size " << stack.size() << " w/o retval" << std::endl;
#if 0
            if (mb.type == memblock::T_V)
            {
              std::cout << "[ ";
              for (auto it = mb.u.v->begin(); it != mb.u.v->end(); it++)
              {
                memblock mb2 = *it;
                if (mb2.type == memblock::T_V)
                {
                  std::cout << "[ ";
                  for (auto it2 = mb2.u.v->begin(); it2 != mb2.u.v->end(); it2++)
                  {
                    memblock mb3 = *it2;
                    if (mb3.type == memblock::T_S)
                    {
                      std::cout << *mb3.u.s << ", ";
                    }
                    else
                    {
                      std::cout << "UNKNOWN, ";
                    }
                  }
                  std::cout << "], ";
                }
                else
                {
                  std::cout << "UNKNOWN, ";
                }
              }
              std::cout << "];";
              std::cout << std::endl;
            }
            else if (mb.type == memblock::T_D)
            {
              std::cout << "double: " << mb.u.d << std::endl;
            }
            else
            {
              std::cout << "UNKNOWN: " << mb.type << std::endl;
            }
#endif
            ip = jmp;
            for (int i = 0; i < cntarg; i++)
            {
              stack.pop_back(); // Clear args
            }
            stack.push_back(mb);
            break;
          }
          case STIRBCE_OPCODE_RET:
          {
            std::cout << "ret1, stack size " << stack.size() << std::endl;
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            jmp = get_reg(stack);
            if (jmp == -1)
            {
              jmp = microsz_global;
            }
            if (unlikely(jmp < 0 || (size_t)jmp > microsz_global))
            {
              throw std::overflow_error("microprogram overflow");
            }
            bp = get_reg(stack);
            std::cout << "ret, stack size " << stack.size() << " w/o retval" << std::endl;
#if 0
            if (mb.type == memblock::T_V)
            {
              std::cout << "[ ";
              for (auto it = mb.u.v->begin(); it != mb.u.v->end(); it++)
              {
                memblock mb2 = *it;
                if (mb2.type == memblock::T_V)
                {
                  std::cout << "[ ";
                  for (auto it2 = mb2.u.v->begin(); it2 != mb2.u.v->end(); it2++)
                  {
                    memblock mb3 = *it2;
                    if (mb3.type == memblock::T_S)
                    {
                      std::cout << *mb3.u.s << ", ";
                    }
                    else
                    {
                      std::cout << "UNKNOWN, ";
                    }
                  }
                  std::cout << "], ";
                }
                else
                {
                  std::cout << "UNKNOWN, ";
                }
              }
              std::cout << "];";
              std::cout << std::endl;
            }
            else
            {
              std::cout << "UNKNOWN: " << mb.type << std::endl;
            }
#endif
            ip = jmp;
            stack.push_back(mb);
            break;
          }
          case STIRBCE_OPCODE_IF_NOT_JMP:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            jmp = get_i64(stack);
            condition = get_i64(stack);
            if (unlikely(ip + jmp > microsz_global))
            {
              throw std::overflow_error("microprogram overflow");
            }
            if (!condition)
            {
              ip += jmp;
            }
            break;
          case STIRBCE_OPCODE_NOP:
            break;
          case STIRBCE_OPCODE_POP:
            if (unlikely(stack.size() <= 0))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.pop_back();
            break;
          case STIRBCE_OPCODE_POP_MANY:
            if (unlikely(stack.size() <= 0))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            if (val < 0 || unlikely(stack.size() < (size_t)val))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.resize(stack.size() - val);
            break;
          case STIRBCE_OPCODE_PUSH_STACK:
          {
            if (unlikely(stack.size() <= 0))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            if (val < 0)
            {
              val = stack.size() + val;
            }
            else
            {
              val = bp + val;
            }
            if (val < 0)
            {
              throw std::range_error("val < 0");
            }
            if ((size_t)val >= stack.size())
            {
              throw std::range_error("val >= stacksize");
            }
            if ((size_t)val < bp)
            {
              throw std::range_error("trying to access stack under bp");
            }
            memblock mb = stack[val];
            stack.push_back(mb);
            break;
          }
          case STIRBCE_OPCODE_SET_STACK:
          {
            if (unlikely(stack.size() <= 0))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            val = get_i64(stack);
            if (val < 0)
            {
              val = stack.size() + val;
            }
            else
            {
              val = bp + val;
            }
            if (val < 0 || (size_t)val >= stack.size())
            {
              throw std::range_error("stack underflow");
            }
            if ((size_t)val < bp)
            {
              throw std::range_error("trying to access stack under bp");
            }
            stack[val] = mb;
            break;
          }
          case STIRBCE_OPCODE_SCOPE_NEW:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            bool holey = get_dbl(stack) ? 1 : 0;
            bool parentlex = get_dbl(stack) ? 1 : 0;
            stack.push_back(memblock(new class scope(parentlex ? scope : scope_global_dyn, holey))); // TODO rename arg
            break;
          }
          case STIRBCE_OPCODE_PUSH_NEW_ARRAY:
          {
            if (unlikely(stack.size() >= stackbound))
            {
              throw std::overflow_error("stack overflow");
            }
            stack.push_back(memblock(new std::vector<memblock>()));
            break;
          }
          case STIRBCE_OPCODE_PUSH_STRINGTAB:
          {
            if (unlikely(stack.size() <= 0))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            if (unlikely(val < 0 || (size_t)val >= st_global->blocks.size()))
            {
              throw std::range_error("can't access st[val]");
            }
            stack.push_back(st_global->blocks.at(val));
            break;
          }
          case STIRBCE_OPCODE_PUSH_NEW_DICT:
          {
            if (unlikely(stack.size() >= stackbound))
            {
              throw std::overflow_error("stack overflow");
            }
            stack.push_back(memblock(new std::map<std::string, memblock>()));
            break;
          }
          case STIRBCE_OPCODE_PUSH_DBL:
          {
            if (unlikely(ip+8 >= microsz_global))
            {
              throw std::overflow_error("microprogram overflow");
            }
            if (unlikely(stack.size() >= stackbound))
            {
              throw std::overflow_error("stack overflow");
            }
            uint64_t u64 = 
                          ((((uint64_t)microprogram_global[ip+0])<<56) |
                          (((uint64_t)microprogram_global[ip+1])<<48) |
                          (((uint64_t)microprogram_global[ip+2])<<40) |
                          (((uint64_t)microprogram_global[ip+3])<<32) |
                          (((uint64_t)microprogram_global[ip+4])<<24) |
                          (((uint64_t)microprogram_global[ip+5])<<16) |
                          (((uint64_t)microprogram_global[ip+6])<<8) |
                                      microprogram_global[ip+7]);
            double dbl;
            memcpy(&dbl, &u64, 8);
            stack.push_back(dbl);
            ip += 8;
            break;
          }
          case STIRBCE_OPCODE_FUN_TRAILER:
          {
            ip -= 1;
            throw std::runtime_error("ran into function trailer, forgotten return?");
          }
          case STIRBCE_OPCODE_FUN_HEADER:
          {
            throw std::runtime_error("ran into function trailer, forgotten return?");
            if (unlikely(ip+8 >= microsz_global))
            {
              throw std::overflow_error("microprogram overflow");
            }
            if (unlikely(stack.size() >= stackbound))
            {
              throw std::overflow_error("stack overflow");
            }
            uint64_t u64 = 
                          ((((uint64_t)microprogram_global[ip+0])<<56) |
                          (((uint64_t)microprogram_global[ip+1])<<48) |
                          (((uint64_t)microprogram_global[ip+2])<<40) |
                          (((uint64_t)microprogram_global[ip+3])<<32) |
                          (((uint64_t)microprogram_global[ip+4])<<24) |
                          (((uint64_t)microprogram_global[ip+5])<<16) |
                          (((uint64_t)microprogram_global[ip+6])<<8) |
                                      microprogram_global[ip+7]);
            double dbl;
            memcpy(&dbl, &u64, 8);
            if ((double)(unsigned)dbl != dbl)
            {
              throw std::invalid_argument("function argument count not an integer");
            }
            ip += 8;
            break;
          }
          case STIRBCE_OPCODE_STR_UPPER:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            std::string s = get_str(stack);
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            stack.push_back(memblock(new std::string(s)));
            break;
          }
          case STIRBCE_OPCODE_TOSTRING:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mb = std::move(stack.back()); stack.pop_back();
            stack.push_back(memblock(new std::string(mbtostring(mb))));
            break;
          }
          case STIRBCE_OPCODE_TONUMBER:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            std::string s = get_str(stack); // FIXME if it's already number?
            size_t idx = 0;
            double dval = std::stod(s, &idx);
            if (idx != s.length() || s.length() == 0)
            {
              throw std::invalid_argument("string not a number");
            }
            stack.push_back(dval);
            break;
          }
          case STIRBCE_OPCODE_STR_LOWER:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            std::string s = get_str(stack);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            stack.push_back(memblock(new std::string(s)));
            break;
          }
          case STIRBCE_OPCODE_PATH_ABSOLUTIFY:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t i = get_i64(stack);
            std::string s = get_str(stack);
            if (i) // boolean value: follow symlinks
            {
              char *res = realpath(s.c_str(), NULL);
              if (res == NULL)
              {
                throw std::runtime_error("realpath failed: file cannot be looked up");
              }
              std::string s2(res);
              free(res);
              stack.push_back(memblock(new std::string(s2)));
            }
            else
            {
              char *res = realpath(".", NULL);
              if (res == NULL)
              {
                throw std::runtime_error("realpath failed: current directory cannot be looked up");
              }
              std::string s2(res);
              free(res);
              if (s2.length() == 0)
              {
                throw std::runtime_error("can't lookup current directory");
              }
              stack.push_back(memblock(new std::string(path_simplify(s2 + "/" + s))));
            }
            break;
          }
          case STIRBCE_OPCODE_STR_REVERSE:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            std::string s = get_str(stack);
            std::reverse(s.begin(), s.end());
            stack.push_back(memblock(new std::string(s)));
            break;
          }
          case STIRBCE_OPCODE_STR_CMP:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            std::string s = get_str(stack);
            std::string s2 = get_str(stack);
            int cmpres;
            if (s < s2)
            {
              cmpres = -1;
            }
            else if (s == s2)
            {
              cmpres = 0;
            }
            else
            {
              cmpres = 1;
            }
            stack.push_back(cmpres);
            break;
          }
          case STIRBCE_OPCODE_CALL_EQ:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            val2 = get_dbl(stack);
            stack.push_back(!!(val == val2));
            break;
          case STIRBCE_OPCODE_CALL_LOGICAL_NOT:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            stack.push_back(!val);
            break;
          case STIRBCE_OPCODE_CALL_BITWISE_NOT:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            stack.push_back(~val);
            break;
          case STIRBCE_OPCODE_SQRT:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(sqrt(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_LOG:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(log(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_EXP:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(exp(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_TAN:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(tan(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_SIN:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(sin(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_COS:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(cos(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_ATAN:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(atan(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_ASIN:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(asin(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_ACOS:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(acos(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_CEIL:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(ceil(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_FLOOR:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(floor(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_FP_CLASSIFY:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            switch (fpclassify(get_dbl(stack)))
            {
              case FP_ZERO:
                stack.push_back(0);
                break;
              case FP_NAN:
                stack.push_back(1);
                break;
              case FP_SUBNORMAL:
                stack.push_back(2);
                break;
              case FP_NORMAL:
                stack.push_back(3);
                break;
              case FP_INFINITE:
                stack.push_back(4); // FIXME #define constants for these
                break;
              default:
                throw std::runtime_error("not a known fp class");
            }
            break;
          case STIRBCE_OPCODE_ABS:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            stack.push_back(abs(get_dbl(stack)));
            break;
          case STIRBCE_OPCODE_CALL_UNARY_MINUS:
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            stack.push_back(-val);
            break;
          case STIRBCE_OPCODE_CALL_NE:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            val2 = get_dbl(stack);
            stack.push_back(!!(val != val2));
            break;
          case STIRBCE_OPCODE_CALL_LT:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            val2 = get_dbl(stack);
            stack.push_back(!!(val < val2));
            break;
          case STIRBCE_OPCODE_CALL_GT:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            val2 = get_dbl(stack);
            stack.push_back(!!(val > val2));
            break;
          case STIRBCE_OPCODE_CALL_LE:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            val2 = get_dbl(stack);
            stack.push_back(!!(val <= val2));
            break;
          case STIRBCE_OPCODE_CALL_GE:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            val2 = get_dbl(stack);
            stack.push_back(!!(val >= val2));
            break;
          case STIRBCE_OPCODE_CALL_LOGICAL_AND:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            val2 = get_i64(stack);
            stack.push_back(!!(val && val2));
            break;
          case STIRBCE_OPCODE_CALL_LOGICAL_OR:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            val2 = get_i64(stack);
            stack.push_back(!!(val || val2));
            break;
          case STIRBCE_OPCODE_CALL_BITWISE_AND:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            val2 = get_i64(stack);
            stack.push_back((val & val2));
            break;
          case STIRBCE_OPCODE_CALL_BITWISE_OR:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            val2 = get_i64(stack);
            stack.push_back((val | val2));
            break;
          case STIRBCE_OPCODE_CALL_BITWISE_XOR:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            val2 = get_i64(stack);
            stack.push_back((val ^ val2));
            break;
          case STIRBCE_OPCODE_CALL_ADD:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            val2 = get_dbl(stack);
            stack.push_back(val + val2);
            break;
          case STIRBCE_OPCODE_CALL_SUB:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            val2 = get_dbl(stack);
            stack.push_back(val - val2);
            break;
          case STIRBCE_OPCODE_CALL_MUL:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            val2 = get_dbl(stack);
            stack.push_back(val * val2);
            break;
          case STIRBCE_OPCODE_CALL_DIV:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_dbl(stack);
            val2 = get_dbl(stack);
            if (unlikely(val2 == 0))
            {
              throw std::domain_error("division by 0");
            }
            stack.push_back(val / val2);
            break;
          case STIRBCE_OPCODE_CALL_MOD:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            val2 = get_i64(stack);
            if (unlikely(val2 == 0))
            {
              throw std::domain_error("division by 0");
            }
            stack.push_back(val % val2);
            break;
          case STIRBCE_OPCODE_CALL_SHL:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            val2 = get_i64(stack);
            stack.push_back(val << val2);
            break;
          case STIRBCE_OPCODE_CALL_SHR:
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            val = get_i64(stack);
            val2 = get_i64(stack);
            stack.push_back(val >> val2);
            break;

          case STIRBCE_OPCODE_DICTLEN:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_M)
            {
              throw memblock_type_error("dictlen: not a map");
            }
            stack.push_back(mbar.u.m->size());
            break;
          }
          case STIRBCE_OPCODE_DICTDEL:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock str = std::move(stack.back()); stack.pop_back();
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_M)
            {
              throw memblock_type_error("dictdel: not a map");
            }
            if (str.type != memblock::T_S)
            {
              throw memblock_type_error("dictdel: not a string");
            }
            mbar.u.m->erase(*str.u.s);
            break;
          }
          case STIRBCE_OPCODE_DICTSET_MAINTAIN:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbit = std::move(stack.back()); stack.pop_back();
            memblock str = std::move(stack.back()); stack.pop_back();
            memblock mbar = stack.back();
            if (mbar.type != memblock::T_M)
            {
              throw memblock_type_error("dictdel: not a map");
            }
            if (str.type != memblock::T_S)
            {
              throw memblock_type_error("dictdel: not a string");
            }
            (*mbar.u.m)[*str.u.s] = mbit;
            break;
          }
          case STIRBCE_OPCODE_DICTSET:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbit = std::move(stack.back()); stack.pop_back();
            memblock str = std::move(stack.back()); stack.pop_back();
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_M)
            {
              throw memblock_type_error("dictdel: not a map");
            }
            if (str.type != memblock::T_S)
            {
              throw memblock_type_error("dictdel: not a string");
            }
            (*mbar.u.m)[*str.u.s] = mbit;
            break;
          }
          case STIRBCE_OPCODE_DICTNEXT_SAFE:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            if (unlikely(stack.size() >= stackbound))
            {
              throw std::overflow_error("stack overflow");
            }
            memblock str = std::move(stack.back()); stack.pop_back(); // lastkey
            memblock mbar = stack.back();
            if (mbar.type != memblock::T_M)
            {
              throw memblock_type_error("dictnext_safe: not a map");
            }
            if (str.type == memblock::T_N)
            {
              auto it = mbar.u.m->begin();
              if (it != mbar.u.m->end())
              {
                stack.push_back(memblock(new std::string(it->first))); // key
                stack.push_back(it->second); // value
              }
              else
              {
                stack.push_back(memblock()); // key
                stack.push_back(memblock()); // value
              }
            }
            else if (str.type == memblock::T_S)
            {
              auto it = mbar.u.m->upper_bound(*str.u.s);
              if (it != mbar.u.m->end())
              {
                stack.push_back(memblock(new std::string(it->first))); // key
                stack.push_back(it->second); // value
              }
              else
              {
                stack.push_back(memblock()); // key
                stack.push_back(memblock()); // value
              }
            }
            else
            {
              throw memblock_type_error("dictnext_safe: invalid type");
            }
            break;
          }
          case STIRBCE_OPCODE_SCOPE_HAS:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock str = std::move(stack.back()); stack.pop_back();
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_SC || str.type != memblock::T_S)
            {
              throw memblock_type_error("scope_has: invalid type");
            }
            stack.push_back(mbar.u.sc->recursive_has(*str.u.s));
            break;
          }
          case STIRBCE_OPCODE_DICTHAS:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock str = std::move(stack.back()); stack.pop_back();
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_M)
            {
              throw memblock_type_error("dicthas: not a map");
            }
            if (str.type != memblock::T_S)
            {
              throw memblock_type_error("dicthas: not a string");
            }
            stack.push_back(memblock(mbar.u.m->find(*str.u.s) != mbar.u.m->end()));
            break;
          }
          case STIRBCE_OPCODE_DICTGET:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock str = std::move(stack.back()); stack.pop_back();
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_M)
            {
              throw memblock_type_error("dictget: not a map");
            }
            if (str.type != memblock::T_S)
            {
              throw memblock_type_error("dictget: not a string");
            }
            if (mbar.u.m->find(*str.u.s) == mbar.u.m->end())
            {
              stack.push_back(memblock());
              break;
            }
            stack.push_back((*mbar.u.m)[*str.u.s]);
            break;
          }


          case STIRBCE_OPCODE_LISTLEN:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_V)
            {
              throw memblock_type_error("listlen: not a vector");
            }
            stack.push_back(mbar.u.v->size());
            break;
          }
          case STIRBCE_OPCODE_LISTSET:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbit = std::move(stack.back()); stack.pop_back();
            int64_t nr = get_i64(stack);
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_V)
            {
              throw memblock_type_error("listset: not a vector");
            }
            // negative indexing
            if (nr < 0)
            {
              nr += mbar.u.v->size();
            }
            if (nr < 0 || (size_t)nr >= mbar.u.v->size())
            {
              throw std::range_error("can't access v[nr]");
            }
            mbar.u.v->at(nr) = mbit;
            break;
          }
          case STIRBCE_OPCODE_DUP_NONRECURSIVE:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_V && mbar.type != memblock::T_M)
            {
              throw memblock_type_error("dup_nonrecursive: neither vector nor map");
            }
            if (mbar.type == memblock::T_V)
            {
              memblock mbar2(new std::vector<memblock>());
              std::copy(mbar.u.v->begin(), mbar.u.v->end(), std::back_inserter(*mbar2.u.v));
              stack.push_back(mbar2);
            }
            else if (mbar.type == memblock::T_M)
            {
              memblock mbd2(new std::map<std::string, memblock>(*mbar.u.m));
              stack.push_back(mbd2);
            }
            else
            {
              throw memblock_type_error("not a vector or map");
            }
            break;
          }
          case STIRBCE_OPCODE_LISTSPLICE:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t end = get_i64(stack);
            int64_t start = get_i64(stack);
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_V)
            {
              throw memblock_type_error("listsplice: not a vector");
            }
            // negative indexing, RFE support NaN
            if (start < 0)
            {
              start += mbar.u.v->size();
            }
            if (end < 0)
            {
              end += mbar.u.v->size();
            }
            if (start < 0 || (size_t)start > mbar.u.v->size())
            {
              throw std::range_error("invalid splice start");
            }
            if (end < 0 || (size_t)end > mbar.u.v->size())
            {
              throw std::range_error("invalid splice end");
            }
            memblock mbar2 = memblock(new std::vector<memblock>());
            for (size_t i = start; i < (size_t)end; i++)
            {
              mbar2.u.v->push_back(mbar.u.v->at(i));
            }
            stack.push_back(mbar2);
            break;
          }
          case STIRBCE_OPCODE_LISTGET:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t nr = get_i64(stack);
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_V)
            {
              throw memblock_type_error("listget: not a vector");
            }
            // negative indexing
            if (nr < 0)
            {
              nr += mbar.u.v->size();
            }
            if (nr < 0 || (size_t)nr >= mbar.u.v->size())
            {
              throw std::range_error("can't access v[nr]");
            }
            stack.push_back(mbar.u.v->at(nr));
            break;
          }
          case STIRBCE_OPCODE_LISTPOP:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_V)
            {
              throw memblock_type_error("listpop: not a vector");
            }
            stack.push_back(mbar.u.v->back());
            mbar.u.v->pop_back();
            break;
          }
          case STIRBCE_OPCODE_APPEND_MAINTAIN:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbit = std::move(stack.back()); stack.pop_back();
            memblock mbar = stack.back();
            if (mbar.type != memblock::T_V)
            {
              throw memblock_type_error("append_maintain: not a vector");
            }
            mbar.u.v->push_back(mbit);
            break;
          }
          case STIRBCE_OPCODE_APPENDALL_MAINTAIN:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbit = std::move(stack.back()); stack.pop_back();
            if (mbit.type != memblock::T_V)
            {
              throw memblock_type_error("appendall_maintain: arg 2 not a vector");
            }
            memblock mbar = stack.back();
            if (mbar.type != memblock::T_V)
            {
              throw memblock_type_error("appendall_maintain: arg 1 not a vector");
            }
            std::copy(mbit.u.v->begin(), mbit.u.v->end(),
                      std::back_inserter(*mbar.u.v));
            break;
          }
          case STIRBCE_OPCODE_APPEND:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbit = std::move(stack.back()); stack.pop_back();
            memblock mbar = std::move(stack.back()); stack.pop_back();
            if (mbar.type != memblock::T_V)
            {
              throw memblock_type_error("append: not a vector");
            }
            mbar.u.v->push_back(mbit);
            break;
          }
          case STIRBCE_OPCODE_PUSH_NIL:
          {
            if (unlikely(stack.size() >= stackbound))
            {
              throw std::overflow_error("stack overflow");
            }
            stack.push_back(memblock());
            break;
          }
          case STIRBCE_OPCODE_PUSH_TRUE:
          {
            if (unlikely(stack.size() >= stackbound))
            {
              throw std::overflow_error("stack overflow");
            }
            stack.push_back(memblock(true));
            break;
          }
          case STIRBCE_OPCODE_PUSH_FALSE:
          {
            if (unlikely(stack.size() >= stackbound))
            {
              throw std::overflow_error("stack overflow");
            }
            stack.push_back(memblock(false));
            break;
          }
          case STIRBCE_OPCODE_FUNIFY:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            if (stack.back().type != memblock::T_D)
            {
              throw memblock_type_error("funify: not a double");
            }
            stack.back().type = memblock::T_F;
            break;
          }
          case STIRBCE_OPCODE_GETSCOPE_DYN:
          {
            if (unlikely(stack.size() >= stackbound))
            {
              throw std::overflow_error("stack overflow");
            }
            stack.push_back(scope);
            break;
          }
          case STIRBCE_OPCODE_SCOPEVAR:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            // RFE swap order?
            memblock mbsc = std::move(stack.back()); stack.pop_back();
            memblock mbs = std::move(stack.back()); stack.pop_back();
            if (mbsc.type != memblock::T_SC)
            {
              throw memblock_type_error("scopevar: not a scope");
            }
            if (mbs.type != memblock::T_S)
            {
              throw memblock_type_error("scopevar: not a string");
            }
            stack.push_back(mbsc.u.sc->recursive_lookup(*mbs.u.s));
            break;
          }
          case STIRBCE_OPCODE_STRREP:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t ival = get_i64(stack);
            memblock mbstr = std::move(stack.back()); stack.pop_back();
            if (mbstr.type != memblock::T_S)
            {
              throw memblock_type_error("strrep: not a string");
            }
            std::ostringstream oss;
            for (int64_t it = 0; it < ival; it++)
            {
              oss << *mbstr.u.s;
            }
            stack.push_back(memblock(new std::string(oss.str())));
            break;
          }
          case STIRBCE_OPCODE_STRGSUB:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbreplace = std::move(stack.back()); stack.pop_back();
            memblock mbfind = std::move(stack.back()); stack.pop_back();
            memblock mborig = std::move(stack.back()); stack.pop_back();
            if (mbfind.type != memblock::T_S || mborig.type != memblock::T_S || mbreplace.type != memblock::T_S)
            {
              throw memblock_type_error("strgsub: not all strings");
            }
            stack.push_back(memblock(new std::string(strreplace(*mborig.u.s, *mbfind.u.s, *mbreplace.u.s))));
            break;
          }
          case STIRBCE_OPCODE_STRWORDCNT:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbspec = std::move(stack.back()); stack.pop_back();
            memblock mborig = std::move(stack.back()); stack.pop_back();
            if (mbspec.type != memblock::T_S || mborig.type != memblock::T_S)
            {
              throw memblock_type_error("strwordcnt: not all strings");
            }
            stack.push_back(wordcnt(*mborig.u.s, *mbspec.u.s));
            break;
          }
          case STIRBCE_OPCODE_PATH_SIMPLIFY:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mborig = std::move(stack.back()); stack.pop_back();
            if (mborig.type != memblock::T_S)
            {
              throw memblock_type_error("path_simplify: not a string");
            }
            stack.push_back(memblock(new std::string(path_simplify(*mborig.u.s))));
            break;
          }
          case STIRBCE_OPCODE_STRWORD:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t wordidx = get_i64(stack);
            memblock mbspec = std::move(stack.back()); stack.pop_back();
            memblock mborig = std::move(stack.back()); stack.pop_back();
            if (mbspec.type != memblock::T_S || mborig.type != memblock::T_S)
            {
              throw memblock_type_error("strword: not a string");
            }
            if (wordidx < 0)
            {
              throw memblock_type_error("strword: wordidx < 0");
            }
            stack.push_back(memblock(new std::string(wordget(*mborig.u.s, *mbspec.u.s, wordidx))));
            break;
          }
          case STIRBCE_OPCODE_GLOB:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mborig = std::move(stack.back()); stack.pop_back();
            memblock mbar(new std::vector<memblock>());
            if (mborig.type != memblock::T_S)
            {
              throw memblock_type_error("glob: not a string");
            }
            glob_t globbuf;
            globbuf.gl_offs = 0;
            glob(mborig.u.s->c_str(), 0, NULL, &globbuf);
            for (size_t i = 0; i < globbuf.gl_pathc; i++)
            {
              mbar.u.v->push_back(memblock(new std::string(globbuf.gl_pathv[i])));
            }
            globfree(&globbuf);
            stack.push_back(mbar);
            break;
          }
          case STIRBCE_OPCODE_STRWORDLIST:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbspec = std::move(stack.back()); stack.pop_back();
            memblock mborig = std::move(stack.back()); stack.pop_back();
            memblock mbar(new std::vector<memblock>());
            if (mbspec.type != memblock::T_S || mborig.type != memblock::T_S)
            {
              throw memblock_type_error("strwordlist: not all strings");
            }
            for (auto it = WordIterator(*mborig.u.s, *mbspec.u.s); !it.end(); it++)
            {
              mbar.u.v->push_back(memblock(new std::string(*it)));
            }
            stack.push_back(mbar);
            break;
          }
          case STIRBCE_OPCODE_STRSTRIP:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbspec = std::move(stack.back()); stack.pop_back();
            memblock mborig = std::move(stack.back()); stack.pop_back();
            if (mbspec.type != memblock::T_S || mborig.type != memblock::T_S)
            {
              throw memblock_type_error("strstrip: not all strings");
            }
            stack.push_back(memblock(new std::string(strstrip(*mborig.u.s, *mbspec.u.s))));
            break;
          }
          case STIRBCE_OPCODE_STRSTR:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbneedle = std::move(stack.back()); stack.pop_back();
            memblock mbhaystack = std::move(stack.back()); stack.pop_back();
            if (mbneedle.type != memblock::T_S || mbhaystack.type != memblock::T_S)
            {
              throw memblock_type_error("strstr: not all strings");
            }
            stack.push_back(mystrstr(*mbhaystack.u.s, *mbneedle.u.s));
            break;
          }
          case STIRBCE_OPCODE_STRAPPEND:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbextend = std::move(stack.back()); stack.pop_back();
            memblock mbbase = std::move(stack.back()); stack.pop_back();
            if (mbbase.type != memblock::T_S || mbextend.type != memblock::T_S)
            {
              throw memblock_type_error("strappend: not all strings");
            }
            stack.push_back(memblock(new std::string(*mbbase.u.s + *mbextend.u.s)));
            break;
          }
          case STIRBCE_OPCODE_STRSUB:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t a = get_i64(stack);
            int64_t b = get_i64(stack);
            memblock mbbase = std::move(stack.back()); stack.pop_back();
            if (mbbase.type != memblock::T_S)
            {
              throw memblock_type_error("strsub: not a string");
            }
            if (b < a ||
                a < 0 || (size_t)a > mbbase.u.s->length() ||
                b < 0 || (size_t)b > mbbase.u.s->length())
            {
              throw std::range_error("strsub: string index error");
            }
            stack.push_back(memblock(new std::string(mbbase.u.s->substr(a, b-a))));
            break;
          }
          case STIRBCE_OPCODE_STR_FROMCHR:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t a = get_i64(stack);
            if (a < 0 || a > 255)
            {
              throw std::range_error("str_fromchr: not a character");
            }
            char buf[2] = {(char)(unsigned char)a, 0};
            stack.push_back(memblock(new std::string(buf, 1)));
            break;
          }
          case STIRBCE_OPCODE_STRSET:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t ch = get_i64(stack);
            int64_t a = get_i64(stack);
            memblock mbbase = std::move(stack.back()); stack.pop_back();
            if (mbbase.type != memblock::T_S)
            {
              throw memblock_type_error("strset: not a string");
            }
            if (ch < 0 || ch >= 256)
            {
              throw std::range_error("str_fromchr: not a character");
            }
            if (a < 0 || (size_t)a >= mbbase.u.s->length())
            {
              throw std::range_error("str_fromchr: string index error");
            }
            std::string copy(*mbbase.u.s);
            copy[a] = (char)(unsigned char)ch;
            stack.push_back(memblock(new std::string(copy)));
            break;
          }
          case STIRBCE_OPCODE_STRGET:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t a = get_i64(stack);
            memblock mbbase = std::move(stack.back()); stack.pop_back();
            if (mbbase.type != memblock::T_S)
            {
              throw memblock_type_error("strget: not a string");
            }
            if (a < 0 || (size_t)a >= mbbase.u.s->length())
            {
              throw std::range_error("strget: string index error");
            }
            stack.push_back(memblock((double)(unsigned char)(*mbbase.u.s)[a]));
            break;
          }
          case STIRBCE_OPCODE_STRLISTJOIN:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbbase = std::move(stack.back()); stack.pop_back();
            std::string joiner = get_str(stack);
            std::ostringstream oss;
            if (mbbase.type != memblock::T_V)
            {
              throw memblock_type_error("strlistjoin: not a vector");
            }
            for (auto it = mbbase.u.v->begin(); it != mbbase.u.v->end(); it++)
            {
              if (it != mbbase.u.v->begin())
              {
                oss << joiner;
              }
              if (it->type != memblock::T_S)
              {
                throw memblock_type_error("strlistjoin: not a string");
              }
              oss << it->u.s;
            }
            if (ret != -EAGAIN)
            {
              stack.push_back(memblock(new std::string(oss.str())));
            }
            break;
          }
          case STIRBCE_OPCODE_STRLEN:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbbase = std::move(stack.back()); stack.pop_back();
            if (mbbase.type != memblock::T_S)
            {
              throw memblock_type_error("strlen: not a string");
            }
            stack.push_back(mbbase.u.s->length());
            break;
          }
          case STIRBCE_OPCODE_FILE_OPEN:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t mode = get_i64(stack);
            std::string name = get_str(stack);
            stack.push_back(file_open(name, mode));
            break;
          }
          case STIRBCE_OPCODE_FILE_CLOSE:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock stream = std::move(stack.back()); stack.pop_back();
            if (stream.type != memblock::T_IOS)
            {
              throw memblock_type_error("file_close: arg not stream");
            }
            if (stream.u.ios->ios == NULL)
            {
              throw std::runtime_error("file_close: arg already closed");
            }
            stream.u.ios->close();
            break;
          }
          case STIRBCE_OPCODE_FILE_FLUSH:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock stream = std::move(stack.back()); stack.pop_back();
            if (stream.type != memblock::T_IOS)
            {
              throw memblock_type_error("file_flush: arg not stream");
            }
            if (stream.u.ios->ios == NULL)
            {
              throw std::runtime_error("file_flush: arg already closed");
            }
            stream.u.ios->ios->flush();
            break;
          }
          case STIRBCE_OPCODE_FILE_SEEK_TELL:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            int64_t whence = get_i64(stack);
            int64_t amt = get_i64(stack);
            memblock stream = std::move(stack.back()); stack.pop_back();
            if (stream.type != memblock::T_IOS)
            {
              throw memblock_type_error("file_seek_tell: arg not a stream");
            }
            if (stream.u.ios->ios == NULL)
            {
              throw std::runtime_error("file_seek_tell: arg already closed");
            }
            if (whence == -1)
            {
              stream.u.ios->ios->seekg(amt, std::ios_base::end); // RFE correct?
              //stream.u.ios->ios->seekp(amt, std::ios_base::end); // RFE correct?
            }
            else if (whence == 1)
            {
              stream.u.ios->ios->seekg(amt, std::ios_base::beg); // RFE correct?
              //stream.u.ios->ios->seekp(amt, std::ios_base::beg); // RFE correct?
            }
            else if (whence == 0)
            {
              stream.u.ios->ios->seekg(amt, std::ios_base::cur); // RFE correct?
              //stream.u.ios->ios->seekp(amt, std::ios_base::cur); // RFE correct?
            }
            else
            {
              throw std::invalid_argument("invalid whence");
            }
            // FIXME errors
            stack.push_back((double)stream.u.ios->ios->tellg());
            stream.u.ios->ios->clear();
            break;
          }
          case STIRBCE_OPCODE_FILE_WRITE:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            std::string out = get_str(stack);
            memblock stream = std::move(stack.back()); stack.pop_back();
            if (stream.type != memblock::T_IOS)
            {
              throw memblock_type_error("file_write: arg not stream");
            }
            if (stream.u.ios->ios == NULL)
            {
              throw std::runtime_error("file_flush: arg already closed");
            }
            (*stream.u.ios->ios) << out;
            if (stream.u.ios->ios->fail())
            {
              throw io_error("file_write: stream out error");
            }
            //stream.u.ios->ios->seekg(out.size(), std::ios_base::cur); // RFE correct?
            stream.u.ios->ios->clear();
            break;
          }
          case STIRBCE_OPCODE_FILE_GET:
          {
            if (unlikely(stack.size() < 3))
            {
              throw std::underflow_error("stack underflow");
            }
            double delim = get_dbl(stack);
            double maxcnt = get_dbl(stack);
            memblock stream = std::move(stack.back()); stack.pop_back();
            stack.push_back(file_get(stream, delim, maxcnt));
            break;
          }
          case STIRBCE_OPCODE_MEMFILE_IOPEN:
          {
            if (unlikely(stack.size() < 1))
            {
              throw std::underflow_error("stack underflow");
            }
            std::string str = get_str(stack);
            std::unique_ptr<std::iostream> ptr(new std::stringstream(str, std::ios_base::in));
            stack.push_back(memblock(ptr.release()));
            break;
          }
          case STIRBCE_OPCODE_SCOPEVAR_SET:
          {
            if (unlikely(stack.size() < 2))
            {
              throw std::underflow_error("stack underflow");
            }
            memblock mbval = std::move(stack.back()); stack.pop_back();
            memblock mbs = std::move(stack.back()); stack.pop_back();
            memblock mbsc = std::move(stack.back()); stack.pop_back();
            if (mbsc.type != memblock::T_SC)
            {
              throw memblock_type_error("scopevar_set: arg not scope");
            }
            if (mbs.type != memblock::T_S)
            {
              throw memblock_type_error("scopevar_set: arg not str");
            }
            mbsc.u.sc->vars[*mbs.u.s] = std::move(mbval);
            break;
          }
  #if 1
          default:
            throw illegal_sequence("invalid opcode");
  #endif
        }
      }
      return ret;
    }
    catch (...)
    {
      create_backtrace(stack, backtrace, ip);
      throw;
    }
  }
  catch (custom_error) { /* ignore */ }
  return 0;
}

void store_d(std::vector<uint8_t> &v, double d)
{
  uint64_t val;
  memcpy(&val, &d, 8);
  v.push_back(val>>56);
  v.push_back(val>>48);
  v.push_back(val>>40);
  v.push_back(val>>32);
  v.push_back(val>>24);
  v.push_back(val>>16);
  v.push_back(val>>8);
  v.push_back(val);
}
