#include <stdio.h>
#include "incyyutils.h"
#include "incyy.h"

void my_abort(void)
{
  abort();
}

int main(int argc, char **argv)
{
  FILE *f = fopen("depfile.dep", "r");
  struct incyy incyy = {};
  if (!f)
  {
    abort();
  }
  incyydoparse(f, &incyy);
  fclose(f);
}
