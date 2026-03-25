#ifndef _MY_MALLOC_H_
#define _MY_MALLOC_H_

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#if STIR_NO_MMAP
#else
static inline size_t stir_topages(size_t limit)
{
  long pagesz = sysconf(_SC_PAGE_SIZE);
  size_t pages, actlimit;
  if (pagesz <= 0)
  {
    abort();
  }
  pages = (limit + (pagesz-1)) / pagesz;
  actlimit = pages * pagesz;
  return actlimit;
}
#endif

void *my_malloc(size_t sz);
void my_free(void *ptr);
void *my_strdup_len(const char *str, size_t sz);
void *my_strdup(const char *str);
int my_malloc_init(void);
void *stir_do_mmap_madvise(size_t bytes);
void stir_do_munmap(void *ptr, size_t bytes);

#endif
