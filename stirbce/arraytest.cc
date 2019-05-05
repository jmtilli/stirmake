#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <exception>
#include <iostream>
#include "opcodes.h"
#include "engine.h"
#include "errno.h"
#include "stirbce.h"

int main(int argc, char **argv)
{
  stringtab st;
  size_t a = st.add("a");
  size_t b = st.add("b");
  std::vector<uint8_t> microprogram;

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 5 + 2*8); // jmp offset
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0); // arg cnt
  microprogram.push_back(STIRBCE_OPCODE_CALL);
  microprogram.push_back(STIRBCE_OPCODE_POP);
  microprogram.push_back(STIRBCE_OPCODE_EXIT);

  microprogram.push_back(STIRBCE_OPCODE_FUN_HEADER);
  store_d(microprogram, 0);

  microprogram.push_back(STIRBCE_OPCODE_PUSH_NEW_ARRAY); // lists

  microprogram.push_back(STIRBCE_OPCODE_PUSH_NEW_ARRAY); // list

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, a);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB);
  microprogram.push_back(STIRBCE_OPCODE_APPEND);

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 1);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 1);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK);
  microprogram.push_back(STIRBCE_OPCODE_APPEND);
  microprogram.push_back(STIRBCE_OPCODE_POP);

  microprogram.push_back(STIRBCE_OPCODE_PUSH_NEW_ARRAY); // list

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, b);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB);
  microprogram.push_back(STIRBCE_OPCODE_APPEND);

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 1);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 1);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK);
  microprogram.push_back(STIRBCE_OPCODE_APPEND);
  microprogram.push_back(STIRBCE_OPCODE_POP);

  microprogram.push_back(STIRBCE_OPCODE_RET);

  memblock sc(new scope());

  engine(&microprogram[0], microprogram.size(), st, NULL, sc);
}
