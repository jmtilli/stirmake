#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <exception>
#include "opcodes.h"
#include "engine.h"
#include "errno.h"

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

double get_dbl(std::vector<memblock> &stack)
{
  memblock mb = stack.back();
  stack.pop_back();
  if (mb.type != memblock::T_D)
  {
    std::terminate();
  }
  return mb.u.d;
}
int64_t get_i64(std::vector<memblock> &stack)
{
  return get_dbl(stack);
}

int engine(const uint8_t *microprogram, size_t microsz)
{
  // 0.53 us / execution
  size_t ip = 0;
  int ret = 0;
  int64_t loc;
  int64_t val, val2, condition, jmp;
  const size_t stackbound = 131072;
  std::vector<memblock> stack;

  ip = 0;
  ret = -EAGAIN;
  while (ret == -EAGAIN && ip < microsz)
  {
    uint8_t instr = microprogram[ip++];
    printf("ip %zu instr %d\n", ip-1, (int)instr);
    switch (instr)
    {
      case STIRBCE_OPCODE_JMP:
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        jmp = get_i64(stack);
        if (unlikely(ip + jmp > microsz))
        {
          printf("microprogram overflow\n");
          ret = -EFAULT;
          break;
        }
        ip += jmp;
        break;
      case STIRBCE_OPCODE_CALL:
      {
        if (unlikely(stack.size() < 1))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        jmp = get_i64(stack);
        if (jmp > microsz || jmp < 0)
        {
          printf("microprogram overflow\n");
          ret = -EFAULT;
          break;
        }
        printf("call, stack size %zu jmp %zu usz %zu\n", stack.size(), jmp, microsz);
        printf("instr %d\n", microprogram[jmp]);
        stack.push_back(ip);
        ip = jmp;
        break;
      }
      case STIRBCE_OPCODE_EXIT:
      {
        printf("exit, stack size %zu\n", stack.size());
        ip = microsz;
        break;
      }
      case STIRBCE_OPCODE_RET:
      {
        printf("ret1, stack size %zu\n", stack.size());
        if (unlikely(stack.size() < 2))
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack.back();
        stack.pop_back();
        jmp = get_i64(stack);
        if (unlikely(jmp > microsz || jmp < 0))
        {
          printf("microprogram overflow\n");
          ret = -EFAULT;
          break;
        }
        printf("ret, stack size %zu\n", stack.size());
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
        if (unlikely(ip + jmp > microsz))
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
        if (unlikely(stack.size() < val))
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
        if (val < 0 || val >= stack.size())
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        memblock mb = stack[stack.size() - val - 1];
        stack.push_back(mb);
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
        if (val < 0 || val >= stack.size())
        {
          printf("stack underflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack[stack.size() - val - 1] = mb;
      }
      case STIRBCE_OPCODE_PUSH_NEW_ARRAY:
      {
        if (unlikely(stack.size() >= stackbound))
        {
          printf("stack overflow\n");
          ret = -EOVERFLOW;
          break;
        }
        stack.push_back(new std::vector<memblock>());
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
        stack.push_back(new std::map<std::string, memblock>());
        break;
      }
      case STIRBCE_OPCODE_PUSH_DBL:
      {
        if (unlikely(ip+8 >= microsz))
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
                      ((((uint64_t)microprogram[ip+0])<<56) |
                      (((uint64_t)microprogram[ip+1])<<48) |
                      (((uint64_t)microprogram[ip+2])<<40) |
                      (((uint64_t)microprogram[ip+3])<<32) |
                      (((uint64_t)microprogram[ip+4])<<24) |
                      (((uint64_t)microprogram[ip+5])<<16) |
                      (((uint64_t)microprogram[ip+6])<<8) |
                                  microprogram[ip+7]);
        double dbl;
        memcpy(&dbl, &u64, 8);
        stack.push_back(dbl);
        ip += 8;
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
      default:
        printf("invalid opcode\n");
        ret = -EILSEQ;
        break;
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

int main(int argc, char **argv)
{
  std::vector<uint8_t> microprogram;
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 1);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 2);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 3);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 10 + 4*8);
  microprogram.push_back(STIRBCE_OPCODE_CALL);
  microprogram.push_back(STIRBCE_OPCODE_POP); // retval
  microprogram.push_back(STIRBCE_OPCODE_POP); // arg
  microprogram.push_back(STIRBCE_OPCODE_POP); // arg
  microprogram.push_back(STIRBCE_OPCODE_POP); // arg
  microprogram.push_back(STIRBCE_OPCODE_EXIT);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 42);
  microprogram.push_back(STIRBCE_OPCODE_RET);
  engine(&microprogram[0], microprogram.size());
}
