#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <exception>
#include <iostream>
#include "opcodes.h"
#include "engine.h"
#include "math.h"
#include "errno.h"
#include "stirbce.h"

int main(int argc, char **argv)
{
  stringtab st;
  size_t a = st.add("*.c");
  std::vector<uint8_t> microprogram;

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, a);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB); // file names
  microprogram.push_back(STIRBCE_OPCODE_GLOB);
  microprogram.push_back(STIRBCE_OPCODE_DUMP);
  microprogram.push_back(STIRBCE_OPCODE_EXIT);

  memblock sc(new scope());

  microprogram_global = &microprogram[0];
  microsz_global = microprogram.size();
  st_global = &st;
  engine(NULL, sc);
}
