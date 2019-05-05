#include <stdio.h>
#include "incyyutils.h"
#include "incyy.h"

int main(int argc, char **argv)
{
  FILE *f = fopen("depfile.d", "r");
  struct incyy incyy = {};
  if (!f)
  {
    abort();
  }
  incyydoparse(f, &incyy);
  fclose(f);
}