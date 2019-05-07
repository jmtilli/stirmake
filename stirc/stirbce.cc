#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <exception>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <math.h>
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
    std::terminate();
  }
  return *mb.u.sc;
}

int lua_makelexcall(lua_State *lua) {
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

  memblock rv = stack.back(); stack.pop_back();

  rv.push_lua(lua);

  std::cout << "makecall exit" << std::endl;

  return 1;
}
int lua_getlexval(lua_State *lua) {
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

  memblock rv = stack.back(); stack.pop_back();

  rv.push_lua(lua);

  std::cout << "getval exit" << std::endl;

  return 1;
}


int luaopen_stir(lua_State *lua)
{
        static const luaL_Reg stir_lib[] = {
                {"makelexcall", lua_makelexcall},
                {"getlexval", lua_getlexval},
                {NULL, NULL}};

        luaL_newlib(lua, stir_lib);
        return 1;
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
        std::cerr << "def" << std::endl;
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
  memblock mb = stack.back();
  stack.pop_back();
  if (mb.type != memblock::T_F)
  {
    std::terminate();
  }
  return mb.u.d;
}
int64_t get_reg(std::vector<memblock> &stack)
{
  memblock mb = stack.back();
  stack.pop_back();
  if (mb.type != memblock::T_REG)
  {
    std::terminate();
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
    std::terminate(); // FIXME error handling
  }
  off = s1.size() - os.size();
  if (s1.substr(off) != os)
  {
    std::terminate(); // FIXME error handling
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
  memblock mb = stack.back();
  stack.pop_back();
  if (mb.type != memblock::T_S)
  {
    std::terminate();
  }
  return *mb.u.s;
}
double get_dbl(std::vector<memblock> &stack)
{
  memblock mb = stack.back();
  stack.pop_back();
  if (mb.type != memblock::T_D && mb.type != memblock::T_B)
  {
    std::terminate();
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

int engine(lua_State *lua, memblock scope,
           std::vector<memblock> &stack, size_t ip)
{
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
      printf("illegal byte sequence\n");
      ret = -EILSEQ;
      break;
    }
    else if (likely((ophi & 0xE0) == 0xC0))
    {
      uint8_t oplo;
      if (unlikely(ip >= microsz_global))
      {
        printf("microcode overflow\n");
        ret = -EOVERFLOW;
        break;
      }
      oplo = microprogram_global[ip++];
      if (unlikely((oplo & 0xC0) != 0x80))
      {
        printf("illegal byte sequence\n");
        ret = -EILSEQ;
        break;
      }
      opcode = ((ophi&0x1F) << 6) | (oplo & 0x3F);
      if (unlikely(opcode < 128))
      {
        printf("illegal byte sequence\n");
        ret = -EILSEQ;
        break;
      }
    }
    else if (likely((ophi & 0xF0) == 0xE0))
    {
      uint8_t opmid, oplo;
      if (unlikely(ip >= microsz_global))
      {
        printf("microcode overflow\n");
        ret = -EOVERFLOW;
        break;
      }
      opmid = microprogram_global[ip++];
      if (unlikely((opmid & 0xC0) != 0x80))
      {
        printf("illegal byte sequence\n");
        ret = -EILSEQ;
        break;
      }
      oplo = microprogram_global[ip++];
      if (unlikely((oplo & 0xC0) != 0x80))
      {
        printf("illegal byte sequence\n");
        ret = -EILSEQ;
        break;
      }
      opcode = ((ophi&0xF) << 12) | ((opmid&0x3F) << 6) | (oplo & 0x3F);
      if (unlikely(opcode <= 0x7FF))
      {
        printf("illegal byte sequence\n");
        ret = -EILSEQ;
        break;
      }
    }
    else
    {
      printf("illegal byte sequence\n");
      ret = -EILSEQ;
      break;
    }
    printf("ip %zu instr %d\n", ip-1, (int)opcode);
    switch ((enum stirbce_opcode)opcode)
    {
      case STIRBCE_OPCODE_TYPE:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back(); stack.pop_back();
        stack.push_back((double)(int)mb.type);
        break;
      }
      case STIRBCE_OPCODE_JMP:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        jmp = get_i64(stack);
        if (unlikely(ip + jmp > microsz_global))
        {
          printf("microprogram overflow\n");
          ret = -EFAULT;
          break;
        }
        ip += jmp;
        break;
      case STIRBCE_OPCODE_LUAPUSH:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.back().push_lua(lua);
        break;
      }
      case STIRBCE_OPCODE_LUAEVAL:
      {
        int top = lua_gettop(lua);
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back();
        stack.pop_back();
        if (mb.type != memblock::T_S)
        {
          printf("not a string\n");
          ret = -EINVAL;
          break;
        }
        luaL_dostring(lua, mb.u.s->c_str());
        if (lua_gettop(lua) == top)
        {
          lua_pushnil(lua);
        }
        else if (lua_gettop(lua) != top + 1)
        {
          printf("lua multi-return not supported\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(lua);
        break;
      }
      case STIRBCE_OPCODE_CALL_IF_FUN:
      {
        if (unlikely(stack.size() < 1 || stack.size() >= stackbound))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        if (stack.back().type == memblock::T_F)
        {
          jmp = get_funaddr(stack);
          if (jmp < 0 || (size_t)jmp + 9 > microsz_global) // FIXME off-by-one?
          {
            printf("microprogram overflow\n");
            ret = -EFAULT;
            break;
          }
          printf("call, stack size %zu jmp %zu usz %zu\n", stack.size(), jmp, microsz_global);
          printf("instr %d\n", microprogram_global[jmp]);
          if (microprogram_global[jmp] != STIRBCE_OPCODE_FUN_HEADER)
          {
            printf("not a function\n");
            ret = -EINVAL;
            break;
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
            printf("function argument count not correct %g %g\n", dbl, (double)0.0);
            ret = -EINVAL;
            break;
          }
          stack.push_back(memblock(bp, false, true));
          stack.push_back(memblock(ip, false, true));
          bp = stack.size() - 2 - 0; // 0 arguments, 2 pushed registers in stack
          ip = jmp + 9;
        }
        break;
      }
      case STIRBCE_OPCODE_CALL:
      {
        int64_t argcnt;
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        argcnt = get_i64(stack);
        if (argcnt < 0)
        {
          printf("negative argument count\n");
          ret = -EINVAL;
          break;
        }
        jmp = get_funaddr(stack);
        if (jmp < 0 || (size_t)jmp + 9 > microsz_global) // FIXME off-by-one?
        {
          printf("microprogram overflow\n");
          ret = -EFAULT;
          break;
        }
        printf("call, stack size %zu jmp %zu usz %zu\n", stack.size(), jmp, microsz_global);
        printf("instr %d\n", microprogram_global[jmp]);
        if (microprogram_global[jmp] != STIRBCE_OPCODE_FUN_HEADER)
        {
          printf("not a function\n");
          ret = -EINVAL;
          break;
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
          printf("function argument count not correct %g %g\n", dbl, (double)argcnt);
          ret = -EINVAL;
          break;
        }
        stack.push_back(memblock(bp, false, true));
        stack.push_back(memblock(ip, false, true));
        bp = stack.size() - 2 - argcnt;
        ip = jmp + 9;
        break;
      }
      case STIRBCE_OPCODE_EXIT:
      {
        printf("exit, stack size %zu\n", stack.size());
        ip = microsz_global;
        break;
      }
      case STIRBCE_OPCODE_SUFSUB:
      {
        if (unlikely(stack.size() < 3))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        std::string ns = get_str(stack);
        std::string os = get_str(stack);
        memblock mb = stack.back(); stack.pop_back();
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
              printf("pathdir: list not only strings\n");
              ret = -EINVAL;
              break;
            }
            mb2.u.v->push_back(memblock(new std::string(sufsub(*it->u.s, os, ns))));
          }
        }
        else
        {
          printf("pathdir: neither vector nor string\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mb2);
        break;
      }
      case STIRBCE_OPCODE_SUFFILTER:
      {
        if (unlikely(stack.size() < 3))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        int reverse = get_dbl(stack) ? 1 : 0;
        std::string suf = get_str(stack);
        memblock mb = stack.back(); stack.pop_back();
        memblock mb2;
        if (mb.type == memblock::T_V)
        {
          mb2 = memblock(memblock(new std::vector<memblock>()));
          for (auto it = mb.u.v->begin(); it != mb.u.v->end(); it++)
          {
            if (it->type != memblock::T_S)
            {
              printf("pathdir: list not only strings\n");
              ret = -EINVAL;
              break;
            }
            if (!!endswith(*it->u.s, suf) == !!reverse)
            {
              mb2.u.v->push_back(*it);
            }
          }
        }
        else
        {
          printf("suffilter: arg not vector\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mb2);
        break;
      }
      case STIRBCE_OPCODE_PATH_SUFFIX:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back(); stack.pop_back();
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
              printf("pathdir: list not only strings\n");
              ret = -EINVAL;
              break;
            }
            mb2.u.v->push_back(memblock(new std::string(pathsuffix(*it->u.s))));
          }
        }
        else
        {
          printf("pathdir: neither vector nor string\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mb2);
        break;
      }
      case STIRBCE_OPCODE_PATH_BASENAME:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back(); stack.pop_back();
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
              printf("pathdir: list not only strings\n");
              ret = -EINVAL;
              break;
            }
            mb2.u.v->push_back(memblock(new std::string(pathbasename(*it->u.s))));
          }
        }
        else
        {
          printf("pathdir: neither vector nor string\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mb2);
        break;
      }
      case STIRBCE_OPCODE_PATH_NOTDIR:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back(); stack.pop_back();
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
              printf("pathdir: list not only strings\n");
              ret = -EINVAL;
              break;
            }
            mb2.u.v->push_back(memblock(new std::string(pathnotdir(*it->u.s))));
          }
        }
        else
        {
          printf("pathdir: neither vector nor string\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mb2);
        break;
      }
      case STIRBCE_OPCODE_PATH_DIR:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back(); stack.pop_back();
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
              printf("pathdir: list not only strings\n");
              ret = -EINVAL;
              break;
            }
            mb2.u.v->push_back(memblock(new std::string(pathdir(*it->u.s))));
          }
        }
        else
        {
          printf("pathdir: neither vector nor string\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mb2);
        break;
      }
      case STIRBCE_OPCODE_OUT:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
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
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        std::string str = get_str(stack);
        std::cerr << str << std::endl;
        ret = -EINTR;
        break;
      }
      case STIRBCE_OPCODE_DUMP:
      {
        printf("dump, stack size %zu\n", stack.size());
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back();
        stack.pop_back();
        mb.dump();
        std::cout << std::endl;
        break;
      }
      case STIRBCE_OPCODE_RETEX2:
      {
        printf("retex2/1, stack size %zu\n", stack.size());
        if (unlikely(stack.size() < 5))
        {
          printf("stack underflow1\n");
          ret = -EOVERFLOW;
          break;
        }
        int64_t cnt = get_i64(stack); // Count of local vars
        int64_t cntarg = get_i64(stack); // Count of args
        if (cnt < 0 || (size_t)(cntarg+cnt)+3 > stack.size())
        {
          printf("stack underflow2 %lld\n", (long long)cnt);
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back();
        stack.pop_back();
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
          printf("microprogram overflow: jmp %lld\n", (long long)jmp);
          ret = -EFAULT;
          break;
        }
        bp = get_reg(stack);
        printf("retex2/0, stack size %zu w/o retval\n", stack.size());
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
        printf("ret1, stack size %zu\n", stack.size());
        if (unlikely(stack.size() < 3))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back();
        stack.pop_back();
        jmp = get_reg(stack);
        if (jmp == -1)
        {
          jmp = microsz_global;
        }
        if (unlikely(jmp < 0 || (size_t)jmp > microsz_global))
        {
          printf("microprogram overflow\n");
          ret = -EFAULT;
          break;
        }
        bp = get_reg(stack);
        printf("ret, stack size %zu w/o retval\n", stack.size());
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
        ip = jmp;
        stack.push_back(mb);
        break;
      }
      case STIRBCE_OPCODE_IF_NOT_JMP:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        jmp = get_i64(stack);
        condition = get_i64(stack);
        if (unlikely(ip + jmp > microsz_global))
        {
          printf("microprogram overflow\n");
          ret = -EFAULT;
          break;
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
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.pop_back();
        break;
      case STIRBCE_OPCODE_POP_MANY:
        if (unlikely(stack.size() <= 0))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_i64(stack);
        if (val < 0 || unlikely(stack.size() < (size_t)val))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.resize(stack.size() - val);
        break;
      case STIRBCE_OPCODE_PUSH_STACK:
      {
        if (unlikely(stack.size() <= 0))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
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
        if (val < 0 || (size_t)val >= stack.size())
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        if ((size_t)val < bp)
        {
          printf("trying to access stack under bp\n");
          ret = -EPERM;
          break;
        }
        memblock mb = stack[val];
        stack.push_back(mb);
        break;
      }
      case STIRBCE_OPCODE_SET_STACK:
      {
        if (unlikely(stack.size() <= 0))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back();
        stack.pop_back();
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
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        if ((size_t)val < bp)
        {
          printf("trying to access stack under bp\n");
          ret = -EPERM;
          break;
        }
        stack[val] = mb;
        break;
      }
      case STIRBCE_OPCODE_SCOPE_NEW:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
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
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(memblock(new std::vector<memblock>()));
        break;
      }
      case STIRBCE_OPCODE_PUSH_STRINGTAB:
      {
        if (unlikely(stack.size() <= 0))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_i64(stack);
        if (unlikely(stack.size() >= stackbound))
        {
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        if (unlikely(val < 0 || (size_t)val >= st_global->blocks.size()))
        {
          printf("stringtab overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(st_global->blocks.at(val));
        break;
      }
      case STIRBCE_OPCODE_PUSH_NEW_DICT:
      {
        if (unlikely(stack.size() >= stackbound))
        {
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(memblock(new std::map<std::string, memblock>()));
        break;
      }
      case STIRBCE_OPCODE_PUSH_DBL:
      {
        if (unlikely(ip+8 >= microsz_global))
        {
          printf("microprogram overflow\n");
          ret = -EFAULT;
          break;
        }
        if (unlikely(stack.size() >= stackbound))
        {
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
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
      case STIRBCE_OPCODE_FUN_HEADER:
      {
        printf("ran into function without being called\n");
        ret = -EFAULT;
        break;
        if (unlikely(ip+8 >= microsz_global))
        {
          printf("microprogram overflow\n");
          ret = -EFAULT;
          break;
        }
        if (unlikely(stack.size() >= stackbound))
        {
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
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
          printf("function argument count not an integer\n");
          ret = -EINVAL;
          break;
        }
        ip += 8;
        break;
      }
      case STIRBCE_OPCODE_STR_UPPER:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
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
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back(); stack.pop_back();
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
          case memblock::T_REG: oss << "register: " << mb.u.d; break;
          default: oss << "UNKNOWN"; break;
        }
        stack.push_back(memblock(new std::string(oss.str())));
        break;
      }
      case STIRBCE_OPCODE_TONUMBER:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        std::string s = get_str(stack); // FIXME if it's already number?
        size_t idx = 0;
        double dval = std::stod(s, &idx);
        if (idx != s.length() || s.length() == 0)
        {
          printf("string not a number\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(dval);
        break;
      }
      case STIRBCE_OPCODE_STR_LOWER:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        std::string s = get_str(stack);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        stack.push_back(memblock(new std::string(s)));
        break;
      }
      case STIRBCE_OPCODE_STR_REVERSE:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        std::string s = get_str(stack);
        std::reverse(s.begin(), s.end());
        stack.push_back(memblock(new std::string(s)));
        break;
      }
      case STIRBCE_OPCODE_STR_EQ:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        std::string s = get_str(stack);
        std::string s2 = get_str(stack);
        stack.push_back(!!(val == val2));
        break;
      }
      case STIRBCE_OPCODE_CALL_EQ:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        val2 = get_dbl(stack);
        stack.push_back(!!(val == val2));
        break;
      case STIRBCE_OPCODE_CALL_LOGICAL_NOT:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        stack.push_back(!val);
        break;
      case STIRBCE_OPCODE_CALL_BITWISE_NOT:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_i64(stack);
        stack.push_back(~val);
        break;
      case STIRBCE_OPCODE_SQRT:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(sqrt(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_LOG:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(log(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_EXP:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(exp(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_TAN:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(tan(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_SIN:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(sin(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_COS:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(cos(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_ATAN:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(atan(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_ASIN:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(asin(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_ACOS:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(acos(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_CEIL:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(ceil(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_FLOOR:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(floor(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_FP_CLASSIFY:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
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
            std::terminate();
        }
        break;
      case STIRBCE_OPCODE_ABS:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(abs(get_dbl(stack)));
        break;
      case STIRBCE_OPCODE_CALL_UNARY_MINUS:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        stack.push_back(-val);
        break;
      case STIRBCE_OPCODE_CALL_NE:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        val2 = get_dbl(stack);
        stack.push_back(!!(val != val2));
        break;
      case STIRBCE_OPCODE_CALL_LT:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        val2 = get_dbl(stack);
        stack.push_back(!!(val < val2));
        break;
      case STIRBCE_OPCODE_CALL_GT:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        val2 = get_dbl(stack);
        stack.push_back(!!(val > val2));
        break;
      case STIRBCE_OPCODE_CALL_LE:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        val2 = get_dbl(stack);
        stack.push_back(!!(val <= val2));
        break;
      case STIRBCE_OPCODE_CALL_GE:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        val2 = get_dbl(stack);
        stack.push_back(!!(val >= val2));
        break;
      case STIRBCE_OPCODE_CALL_LOGICAL_AND:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_i64(stack);
        val2 = get_i64(stack);
        stack.push_back(!!(val && val2));
        break;
      case STIRBCE_OPCODE_CALL_LOGICAL_OR:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_i64(stack);
        val2 = get_i64(stack);
        stack.push_back(!!(val || val2));
        break;
      case STIRBCE_OPCODE_CALL_BITWISE_AND:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_i64(stack);
        val2 = get_i64(stack);
        stack.push_back((val & val2));
        break;
      case STIRBCE_OPCODE_CALL_BITWISE_OR:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_i64(stack);
        val2 = get_i64(stack);
        stack.push_back((val | val2));
        break;
      case STIRBCE_OPCODE_CALL_BITWISE_XOR:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_i64(stack);
        val2 = get_i64(stack);
        stack.push_back((val ^ val2));
        break;
      case STIRBCE_OPCODE_CALL_ADD:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        val2 = get_dbl(stack);
        stack.push_back(val + val2);
        break;
      case STIRBCE_OPCODE_CALL_SUB:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        val2 = get_dbl(stack);
        stack.push_back(val - val2);
        break;
      case STIRBCE_OPCODE_CALL_MUL:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        val2 = get_dbl(stack);
        stack.push_back(val * val2);
        break;
      case STIRBCE_OPCODE_CALL_DIV:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_dbl(stack);
        val2 = get_dbl(stack);
        if (unlikely(val2 == 0))
        {
          printf("div by 0\n");
          ret = -EDOM;
          break;
        }
        stack.push_back(val / val2);
        break;
      case STIRBCE_OPCODE_CALL_MOD:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_i64(stack);
        val2 = get_i64(stack);
        if (unlikely(val2 == 0))
        {
          printf("div by 0\n");
          ret = -EDOM;
          break;
        }
        stack.push_back(val % val2);
        break;
      case STIRBCE_OPCODE_CALL_SHL:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_i64(stack);
        val2 = get_i64(stack);
        stack.push_back(val << val2);
        break;
      case STIRBCE_OPCODE_CALL_SHR:
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        val = get_i64(stack);
        val2 = get_i64(stack);
        stack.push_back(val >> val2);
        break;

      case STIRBCE_OPCODE_DICTLEN:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_M)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mbar.u.m->size());
        break;
      }
      case STIRBCE_OPCODE_DICTDEL:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock str = stack.back(); stack.pop_back();
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_M || str.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        mbar.u.m->erase(*str.u.s);
        break;
      }
      case STIRBCE_OPCODE_DICTSET:
      {
        if (unlikely(stack.size() < 3))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbit = stack.back(); stack.pop_back();
        memblock str = stack.back(); stack.pop_back();
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_M || str.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        (*mbar.u.m)[*str.u.s] = mbit;
        break;
      }
      case STIRBCE_OPCODE_DICTNEXT_SAFE:
      {
        if (unlikely(stack.size() < 2 || stack.size() >= stackbound))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock str = stack.back(); stack.pop_back(); // lastkey
        memblock mbar = stack.back();
        if (mbar.type != memblock::T_M)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
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
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        break;
      }
      case STIRBCE_OPCODE_SCOPE_HAS:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock str = stack.back(); stack.pop_back();
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_SC || str.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mbar.u.sc->recursive_has(*str.u.s));
        break;
      }
      case STIRBCE_OPCODE_DICTHAS:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock str = stack.back(); stack.pop_back();
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_M || str.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(memblock(mbar.u.m->find(*str.u.s) != mbar.u.m->end()));
        break;
      }
      case STIRBCE_OPCODE_DICTGET:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock str = stack.back(); stack.pop_back();
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_M || str.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
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
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_V)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mbar.u.v->size());
        break;
      }
      case STIRBCE_OPCODE_LISTSET:
      {
        if (unlikely(stack.size() < 3))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbit = stack.back(); stack.pop_back();
        int64_t nr = get_i64(stack);
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_V)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        // negative indexing
        if (nr < 0)
        {
          nr += mbar.u.v->size();
        }
        if (nr < 0 || (size_t)nr >= mbar.u.v->size())
        {
          printf("overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        mbar.u.v->at(nr) = mbit;
        break;
      }
      case STIRBCE_OPCODE_DUP_NONRECURSIVE:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_V && mbar.type != memblock::T_M)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
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
          std::terminate();
        }
        break;
      }
      case STIRBCE_OPCODE_LISTSPLICE:
      {
        if (unlikely(stack.size() < 3))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        int64_t end = get_i64(stack);
        int64_t start = get_i64(stack);
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_V)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
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
          printf("overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        if (end < 0 || (size_t)end > mbar.u.v->size())
        {
          printf("overflow\n");
          ret = -EOVERFLOW;
          break;
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
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        int64_t nr = get_i64(stack);
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_V)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        // negative indexing
        if (nr < 0)
        {
          nr += mbar.u.v->size();
        }
        if (nr < 0 || (size_t)nr >= mbar.u.v->size())
        {
          printf("overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(mbar.u.v->at(nr));
        break;
      }
      case STIRBCE_OPCODE_LISTPOP:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_V)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mbar.u.v->back());
        mbar.u.v->pop_back();
        break;
      }
      case STIRBCE_OPCODE_APPEND_MAINTAIN:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbit = stack.back(); stack.pop_back();
        memblock mbar = stack.back();
        if (mbar.type != memblock::T_V)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        mbar.u.v->push_back(mbit);
        break;
      }
      case STIRBCE_OPCODE_APPENDALL_MAINTAIN:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbit = stack.back(); stack.pop_back();
        if (mbit.type != memblock::T_V)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        memblock mbar = stack.back();
        if (mbar.type != memblock::T_V)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        std::copy(mbit.u.v->begin(), mbit.u.v->end(),
                  std::back_inserter(*mbar.u.v));
        break;
      }
      case STIRBCE_OPCODE_APPEND:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbit = stack.back(); stack.pop_back();
        memblock mbar = stack.back(); stack.pop_back();
        if (mbar.type != memblock::T_V)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        mbar.u.v->push_back(mbit);
        break;
      }
      case STIRBCE_OPCODE_PUSH_NIL:
      {
        if (unlikely(stack.size() >= stackbound))
        {
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(memblock());
        break;
      }
      case STIRBCE_OPCODE_PUSH_TRUE:
      {
        if (unlikely(stack.size() >= stackbound))
        {
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(memblock(true));
        break;
      }
      case STIRBCE_OPCODE_PUSH_FALSE:
      {
        if (unlikely(stack.size() >= stackbound))
        {
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(memblock(false));
        break;
      }
      case STIRBCE_OPCODE_FUNIFY:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        if (stack.back().type != memblock::T_D)
        {
          printf("not a double\n");
          ret = -EINVAL;
          break;
        }
        stack.back().type = memblock::T_F;
        break;
      }
      case STIRBCE_OPCODE_GETSCOPE_DYN:
      {
        if (unlikely(stack.size() >= stackbound))
        {
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(scope);
        break;
      }
      case STIRBCE_OPCODE_SCOPEVAR:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        // RFE swap order?
        memblock mbsc = stack.back(); stack.pop_back();
        memblock mbs = stack.back(); stack.pop_back();
        if (mbsc.type != memblock::T_SC)
        {
          printf("arg not scope\n");
          ret = -EINVAL;
          break;
        }
        if (mbs.type != memblock::T_S)
        {
          printf("arg not str\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mbsc.u.sc->recursive_lookup(*mbs.u.s));
        break;
      }
      case STIRBCE_OPCODE_FUN_JMP_ADDR:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbit = stack.back(); stack.pop_back();
        if (mbit.type != memblock::T_F)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(memblock(0.0)); // FIXME implement
        std::terminate();
        break;
      }
      case STIRBCE_OPCODE_STRREP:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        int64_t ival = get_i64(stack);
        memblock mbstr = stack.back(); stack.pop_back();
        if (mbstr.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        std::ostringstream oss;
        for (int64_t it = 0; it < ival; it++)
        {
          oss << *mbstr.u.s;
        }
        stack.push_back(memblock(new std::string(oss.str())));
        break;
      }
      case STIRBCE_OPCODE_STRWORDCNT:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbspec = stack.back(); stack.pop_back();
        memblock mborig = stack.back(); stack.pop_back();
        if (mbspec.type != memblock::T_S || mborig.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(wordcnt(*mborig.u.s, *mbspec.u.s));
        break;
      }
      case STIRBCE_OPCODE_PATH_SIMPLIFY:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mborig = stack.back(); stack.pop_back();
        if (mborig.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(memblock(new std::string(path_simplify(*mborig.u.s))));
        break;
      }
      case STIRBCE_OPCODE_STRWORD:
      {
        if (unlikely(stack.size() < 3))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        int64_t wordidx = get_i64(stack);
        memblock mbspec = stack.back(); stack.pop_back();
        memblock mborig = stack.back(); stack.pop_back();
        if (mbspec.type != memblock::T_S || mborig.type != memblock::T_S || wordidx < 0)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(memblock(new std::string(wordget(*mborig.u.s, *mbspec.u.s, wordidx))));
        break;
      }
      case STIRBCE_OPCODE_STRWORDLIST:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbspec = stack.back(); stack.pop_back();
        memblock mborig = stack.back(); stack.pop_back();
        memblock mbar(new std::vector<memblock>());
        if (mbspec.type != memblock::T_S || mborig.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
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
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbspec = stack.back(); stack.pop_back();
        memblock mborig = stack.back(); stack.pop_back();
        if (mbspec.type != memblock::T_S || mborig.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(memblock(new std::string(strstrip(*mborig.u.s, *mbspec.u.s))));
        break;
      }
      case STIRBCE_OPCODE_STRSTR:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbneedle = stack.back(); stack.pop_back();
        memblock mbhaystack = stack.back(); stack.pop_back();
        if (mbneedle.type != memblock::T_S || mbhaystack.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mystrstr(*mbhaystack.u.s, *mbneedle.u.s));
        break;
      }
      case STIRBCE_OPCODE_STRAPPEND:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbextend = stack.back(); stack.pop_back();
        memblock mbbase = stack.back(); stack.pop_back();
        if (mbbase.type != memblock::T_S || mbextend.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(memblock(new std::string(*mbbase.u.s + *mbextend.u.s)));
        break;
      }
      case STIRBCE_OPCODE_STRSUB:
      {
        if (unlikely(stack.size() < 3))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        int64_t a = get_i64(stack);
        int64_t b = get_i64(stack);
        memblock mbbase = stack.back(); stack.pop_back();
        if (mbbase.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        if (b < a ||
            a < 0 || (size_t)a > mbbase.u.s->length() ||
            b < 0 || (size_t)b > mbbase.u.s->length())
        {
          printf("string index error\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(memblock(new std::string(mbbase.u.s->substr(a, b-a))));
        break;
      }
      case STIRBCE_OPCODE_STR_FROMCHR:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        int64_t a = get_i64(stack);
        if (a < 0 || a > 255)
        {
          printf("string index error\n");
          ret = -EINVAL;
          break;
        }
        char buf[2] = {(char)(unsigned char)a, 0};
        stack.push_back(memblock(new std::string(buf, 1)));
        break;
      }
      case STIRBCE_OPCODE_STRSET:
      {
        if (unlikely(stack.size() < 3))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        int64_t ch = get_i64(stack);
        int64_t a = get_i64(stack);
        memblock mbbase = stack.back(); stack.pop_back();
        if (mbbase.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        if (ch < 0 || ch >= 256)
        {
          printf("char value error\n");
          ret = -EINVAL;
          break;
        }
        if (a < 0 || (size_t)a >= mbbase.u.s->length())
        {
          printf("string index error\n");
          ret = -EINVAL;
          break;
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
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        int64_t a = get_i64(stack);
        memblock mbbase = stack.back(); stack.pop_back();
        if (mbbase.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        if (a < 0 || (size_t)a >= mbbase.u.s->length())
        {
          printf("string index error\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(memblock((double)(unsigned char)(*mbbase.u.s)[a]));
        break;
      }
      case STIRBCE_OPCODE_STRLISTJOIN:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbbase = stack.back(); stack.pop_back();
        std::string joiner = get_str(stack);
        std::ostringstream oss;
        if (mbbase.type != memblock::T_V)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        for (auto it = mbbase.u.v->begin(); it != mbbase.u.v->end(); it++)
        {
          if (it != mbbase.u.v->begin())
          {
            oss << joiner;
          }
          if (it->type != memblock::T_S)
          {
            printf("invalid type\n");
            ret = -EINVAL;
            break;
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
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbbase = stack.back(); stack.pop_back();
        if (mbbase.type != memblock::T_S)
        {
          printf("invalid type\n");
          ret = -EINVAL;
          break;
        }
        stack.push_back(mbbase.u.s->length());
        break;
      }
      case STIRBCE_OPCODE_SCOPEVAR_SET:
      {
        if (unlikely(stack.size() < 2))
        {
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mbval = stack.back(); stack.pop_back();
        memblock mbs = stack.back(); stack.pop_back();
        memblock mbsc = stack.back(); stack.pop_back();
        if (mbsc.type != memblock::T_SC)
        {
          printf("arg not scope\n");
          ret = -EINVAL;
          break;
        }
        if (mbs.type != memblock::T_S)
        {
          printf("arg not str\n");
          ret = -EINVAL;
          break;
        }
        mbsc.u.sc->vars[*mbs.u.s] = mbval;
        break;
      }
#if 1
      default:
        printf("invalid opcode\n");
        ret = -EILSEQ;
        break;
#endif
    }
  }
  return ret;
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
