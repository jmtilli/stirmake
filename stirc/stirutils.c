#include "stirutils.h"
#include "canon.h"
#include "mymalloc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

void my_abort(void);

char *calc_forward_path(char *storcwd, size_t upcnt)
{
  char *fwd_path = NULL;
  size_t idx = strlen(storcwd);
  size_t i;
  for (i = 0; i < upcnt; i++)
  {
    char *ptr;
    ptr = my_memrchr(storcwd, '/', idx);
    if (ptr == NULL)
    {
      idx = 0;
    }
    else
    {
      idx = ptr - storcwd;
    }
  }
  if (storcwd[idx] == '/')
  {
    fwd_path = storcwd + idx + 1;
  }
  else
  {
    fwd_path = storcwd + idx;
  }
  if (*fwd_path == '\0')
  {
    fwd_path = ".";
  }
  return fwd_path;
}

char *dir_up(char *old)
{
  size_t oldlen = strlen(old);
  size_t uncanonized_capacity = oldlen + 4;
  char *uncanonized = malloc(uncanonized_capacity);
  char *canonized;
  if (snprintf(uncanonized, uncanonized_capacity, "%s/..", old) >=
      (int)uncanonized_capacity)
  {
    my_abort();
  }
  canonized = canon(uncanonized);
  free(uncanonized);
  return canonized;
}

char *myitoa(int i)
{
  char *res = my_malloc(16);
  snprintf(res, 16, "%d", i);
  return res;
}
