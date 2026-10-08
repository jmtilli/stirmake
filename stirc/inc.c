#include <stdio.h>
#include "incyyutils.h"
#include "incyy.h"

void my_abort(void)
{
  abort();
}

void errxit(const char *fmt, ...)
{
}

int main(int argc, char **argv)
{
  FILE *f = fopen("depfile.dep", "r");
  struct incyy incyy = INCYY_EMPTY;
  incyy.prefix = ".";
  incyy.prefixlen = 1;
  if (!f)
  {
    abort();
  }
  //incyydoparse(f, &incyy);
  printf("Parsing result: %d\n", incyymineparse(f, &incyy));
  fclose(f);
  return 0;
}
