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
  store_d(microprogram, 6 + 2*8); // jmp offset
  microprogram.push_back(STIRBCE_OPCODE_FUNIFY);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0); // arg cnt
  microprogram.push_back(STIRBCE_OPCODE_CALL);
  microprogram.push_back(STIRBCE_OPCODE_DUMP);
  microprogram.push_back(STIRBCE_OPCODE_EXIT);

  microprogram.push_back(STIRBCE_OPCODE_FUN_HEADER);
  store_d(microprogram, 0);

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, a);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB);
  microprogram.push_back(STIRBCE_OPCODE_ERROR);

  microprogram.push_back(STIRBCE_OPCODE_FUN_TRAILER);
  store_d(microprogram, b);

  memblock sc(new scope());

  microprogram_global = &microprogram[0];
  microsz_global = microprogram.size();
  st_global = &st;
  std::vector<std::string> bt;
  std::cout << "backtrace:" << std::endl;
  if (engine(NULL, sc, bt) != -EAGAIN)
  {
    for (auto it = bt.begin(); it != bt.end(); it++)
    {
      std::cout << *it << std::endl;
    }
  }
}
