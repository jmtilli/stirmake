#include <stdio.h>
#include <stdlib.h>
#include "yyutils.h"
#include "stiryy.h"

size_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen)
{
  abort();
}
void stiryy_add_fun_sym(struct stiryy *stiryy, const char *symbol, size_t funloc)
{
  abort();
}

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
