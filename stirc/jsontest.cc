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

  microprogram.push_back(STIRBCE_OPCODE_PUSH_NEW_DICT);

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, a);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, -15);
  microprogram.push_back(STIRBCE_OPCODE_DICTSET_MAINTAIN);

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, b);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_NEW_ARRAY);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_NEW_ARRAY);
  microprogram.push_back(STIRBCE_OPCODE_APPEND_MAINTAIN);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_NEW_ARRAY);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_NEW_ARRAY);
  microprogram.push_back(STIRBCE_OPCODE_APPEND_MAINTAIN);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_NEW_ARRAY);
  microprogram.push_back(STIRBCE_OPCODE_APPEND_MAINTAIN);
  microprogram.push_back(STIRBCE_OPCODE_APPEND_MAINTAIN);
  microprogram.push_back(STIRBCE_OPCODE_DICTSET_MAINTAIN);

  microprogram.push_back(STIRBCE_OPCODE_JSON_ENCODE);
  microprogram.push_back(STIRBCE_OPCODE_DUMP);

  memblock sc(new scope());

  microprogram_global = &microprogram[0];
  microsz_global = microprogram.size();
  st_global = &st;
  engine(NULL, sc);
}
