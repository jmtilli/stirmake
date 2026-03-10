#include <stdio.h>
#include <stdlib.h>
#include "yyutils.h"
#include "stiryy.h"

int yy_stored_lineno = -1;
const char *yy_stored_prefix = NULL;

void *my_strdup(const char *str)
{
  abort();
}
void my_abort(void)
{
  abort();
}
void *my_malloc(size_t sz)
{
  abort();
}

mysize_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen)
{
  abort();
}
#if 0
size_t stiryy_add_fun_sym(struct stiryy *stiryy, const char *symbol, int maybe, size_t loc)
{
  abort();
}
#endif
int add_dep_after_parsing_stage(char **tgts, size_t tgtsz,
                                char **deps, size_t depsz,
                                char *prefix,
                                int rec, int orderonly, int wait)
{
  abort();
}
void *my_memrchr(const void *s, int c, size_t n)
{
  unsigned const char *ptr = s + n;
  while (n > 0)
  {
    ptr--;
    if (*ptr == c)
    {
      return (void*)ptr;
    }
    n--;
  }
  return NULL;
}

int main(int argc, char **argv)
{
  FILE *f = fopen("Stirfile", "r");
  struct abce abce;
  struct stiryy_main stirmain = {.abce = &abce};
  struct stiryy stiryy = {};
  abce_init(&abce);
  stiryy_init(&stiryy, &stirmain, ".", ".", abce.dynscope, NULL, "Stirfile", 1);
  if (!f)
  {
    abort();
  }
  stiryydoparse(f, &stiryy);
  fclose(f);
  return 0;
}
