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
  size_t i;
  incyy.prefix = ".";
  incyy.prefixlen = 1;
  if (!f)
  {
    abort();
  }
  for (i = 0; i < 1000*1000; i++)
  {
    struct incyy incyy = INCYY_EMPTY;
    incyy.prefix = ".";
    incyy.prefixlen = 1;
    rewind(f);
    incyymineparse(f, &incyy, NULL, NULL, NULL, NULL);
    incyy_free(&incyy);
  }
  //incyydoparse(f, &incyy);
  printf("Parsing result: %d\n", incyymineparse(f, &incyy, NULL, NULL, NULL, NULL));
  fclose(f);
  return 0;
}
