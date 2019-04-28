#ifndef _OPCODES_H_
#define _OPCODES_H_

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <memory>
#include <vector>
#include <map>

class memblock {
 public:
  enum {
    T_D, T_S, T_V, T_M
  } type;
  union {
    double d;
    std::string *s;
    std::vector<memblock> *v;
    std::map<std::string, memblock> *m;
  } u;
  size_t *refc;

  memblock(): type(T_D)
  {
    u.d = 0;
  }
  memblock(double d): type(T_D)
  {
    u.d = d;
  }
  memblock(std::vector<memblock> *v): type(T_V)
  {
    u.v = v;
    refc = new size_t(1);
  }
  memblock(std::map<std::string, memblock> *m): type(T_M)
  {
    u.m = m;
    refc = new size_t(1);
  }
  memblock(const memblock &other)
  {
    type = other.type;
    u = other.u;
    refc = other.refc;
    if (type == T_D)
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
  }
  ~memblock() {
    if (type == T_D)
    {
      return;
    }
    if (--*refc == 0)
    {
      switch (type)
      {
        case T_S:
          delete u.s;
          break;
        case T_V:
          delete u.v;
          break;
        case T_M:
          delete u.m;
          break;
      }
    }
    delete refc;
  }
};

enum stirbce_opcode {
  STIRBCE_OPCODE_PUSH_DBL = 1, //
  STIRBCE_OPCODE_CALL_EQ = 5, //
  STIRBCE_OPCODE_CALL_NE = 6, //
  STIRBCE_OPCODE_CALL_LOGICAL_AND = 7, //
  STIRBCE_OPCODE_CALL_LOGICAL_OR = 8, //
  STIRBCE_OPCODE_CALL_LOGICAL_NOT = 9, //
  STIRBCE_OPCODE_CALL_BITWISE_AND = 10, //
  STIRBCE_OPCODE_CALL_BITWISE_OR = 11, //
  STIRBCE_OPCODE_CALL_BITWISE_XOR = 12, //
  STIRBCE_OPCODE_CALL_BITWISE_NOT = 13, //
  STIRBCE_OPCODE_CALL_LT = 14, //
  STIRBCE_OPCODE_CALL_GT = 15, //
  STIRBCE_OPCODE_CALL_LE = 16, //
  STIRBCE_OPCODE_CALL_GE = 17, //
  STIRBCE_OPCODE_CALL_SHL = 18, //
  STIRBCE_OPCODE_CALL_SHR = 19, //
  STIRBCE_OPCODE_CALL_ADD = 20, //
  STIRBCE_OPCODE_CALL_SUB = 21, //
  STIRBCE_OPCODE_CALL_MUL = 22, //
  STIRBCE_OPCODE_CALL_DIV = 23, //
  STIRBCE_OPCODE_CALL_MOD = 24, //
  STIRBCE_OPCODE_CALL_UNARY_MINUS = 25, //
  STIRBCE_OPCODE_IF_NOT_JMP = 26, //
  STIRBCE_OPCODE_JMP = 27, //
  STIRBCE_OPCODE_CALL = 28, //
  STIRBCE_OPCODE_RET = 29,
  STIRBCE_OPCODE_NOP = 30,
  STIRBCE_OPCODE_POP = 34,
  STIRBCE_OPCODE_POP_MANY = 35,
  STIRBCE_OPCODE_PUSH_STACK = 36,
  STIRBCE_OPCODE_SET_STACK = 37,
  STIRBCE_OPCODE_PUSH_NEW_ARRAY = 38,
  STIRBCE_OPCODE_PUSH_NEW_DICT = 39,
  STIRBCE_OPCODE_EXIT = 40,
};

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
