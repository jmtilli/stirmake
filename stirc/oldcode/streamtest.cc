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
  size_t a = st.add("/proc/cpuinfo");
  size_t b = st.add("streamtest_out.tmp");
  size_t c = st.add("CC");
  std::vector<uint8_t> microprogram;

  // First, try reading /proc/cpuinfo

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, a);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB); // file name
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 1); // 1 == in
  microprogram.push_back(STIRBCE_OPCODE_FILE_OPEN); // [0]: stream

  for (int i = 0; i < 3; i++)
  {
    microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
    store_d(microprogram, 0);
    microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK); // stream
    microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
    store_d(microprogram, 1.0/0.0); // +Inf, maxcnt
    microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
    store_d(microprogram, '\n'); // delim
    microprogram.push_back(STIRBCE_OPCODE_FILE_GET);
    microprogram.push_back(STIRBCE_OPCODE_DUMP);
  }
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK); // stream
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 1.0/0.0); // +Inf, maxcnt
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, log(-1)); // NaN, delim
  microprogram.push_back(STIRBCE_OPCODE_FILE_GET);
  microprogram.push_back(STIRBCE_OPCODE_DUMP);

  microprogram.push_back(STIRBCE_OPCODE_POP); // remove [0]: stream

  // Second, try file output

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, b);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB); // file name
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 2); // 2 == out
  microprogram.push_back(STIRBCE_OPCODE_FILE_OPEN); // [0]: stream

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK); // stream
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, b);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB); // file name
  microprogram.push_back(STIRBCE_OPCODE_FILE_WRITE); // file name

  microprogram.push_back(STIRBCE_OPCODE_POP); // remove [0]: stream

  // Third, try file I/O

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, b);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB); // file name
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 3); // 3 == inout
  microprogram.push_back(STIRBCE_OPCODE_FILE_OPEN); // [0]: stream

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK); // stream
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, c);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STRINGTAB); // file name
  microprogram.push_back(STIRBCE_OPCODE_FILE_WRITE); // file name

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK); // stream
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0); // amt
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 1); // whence: 1 == beg
  microprogram.push_back(STIRBCE_OPCODE_FILE_SEEK_TELL);
  microprogram.push_back(STIRBCE_OPCODE_DUMP);

  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 0);
  microprogram.push_back(STIRBCE_OPCODE_PUSH_STACK); // stream
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, 1.0/0.0); // +Inf, maxcnt
  microprogram.push_back(STIRBCE_OPCODE_PUSH_DBL);
  store_d(microprogram, log(-1)); // NaN, delim
  microprogram.push_back(STIRBCE_OPCODE_FILE_GET);
  microprogram.push_back(STIRBCE_OPCODE_DUMP);

  microprogram.push_back(STIRBCE_OPCODE_POP); // remove [0]: stream

  memblock sc(new scope());

  microprogram_global = &microprogram[0];
  microsz_global = microprogram.size();
  st_global = &st;
  engine(NULL, sc);
}
