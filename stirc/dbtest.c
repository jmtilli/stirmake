#include <stdio.h>
#include "dbyyutils.h"
#include "dbyy.h"

void my_abort(void)
{
  abort();
}

void errxit(const char *fmt, ...)
{
}

void cmdfn(void *userdata, struct dbyycmd *cmdsbuf, size_t cmdssz,
           struct dbyyrule *rule)
{
}
void tsfn(void *userdata, struct tsdbentry *e)
{
}

int main(int argc, char **argv)
{
  FILE *f = fopen(".stir.db", "r");
  struct dbyy dbyy = DBYY_EMPTY;
  if (!f)
  {
    abort();
  }
  printf("Parsing result: %d\n", dbyymineparse(f, &dbyy, cmdfn, tsfn, NULL));
  fclose(f);
  return 0;
}
