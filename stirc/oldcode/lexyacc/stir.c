#include <stdio.h>
#include "yyutils.h"
#include "stiryy.h"
int main(int argc, char **argv)
{
  FILE *f = fopen("Stirfile", "r");
  struct stiryy stiryy = {};
  if (!f)
  {
    abort();
  }
  stiryydoparse(f, &stiryy);
  fclose(f);
}
