#include "mymalloc.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>

char *my_arena;
char *my_arena_ptr;
size_t sizeof_my_arena;

void errxit(const char *fmt, ...);
void my_abort(void);

void *my_malloc(size_t sz)
{
  void *result = my_arena_ptr;
  if (sz > sizeof_my_arena)
  {
#if 0
    if (debug)
    {
      print_indent();
      printf("allocating outside of arena, big allocation, %zu bytes\n", sz);
    }
#endif
    result = stir_do_mmap_madvise(sz);
    if (result == NULL)
    {
      fprintf(stderr, "too large alloc, out of memory: %zu bytes\n", sz);
      my_abort();
    }
    return result;
  }
  my_arena_ptr += (sz+7)/8*8;
  if (my_arena_ptr > my_arena + sizeof_my_arena)
  {
#if 0
    if (debug)
    {
      print_indent();
      printf("allocating new arena\n");
    }
#endif
    my_arena = stir_do_mmap_madvise(sizeof_my_arena);
    if (my_arena == NULL)
    {
      errxit("Can't mmap new arena");
      exit(2);
    }
    my_arena_ptr = my_arena;
    result = my_arena_ptr;
    my_arena_ptr += (sz+7)/8*8;
    if (my_arena_ptr > my_arena + sizeof_my_arena)
    {
      //fprintf(stderr, "out of memory\n"); // nondescriptive
      my_abort();
    }
  }
  return result;
}
void my_free(void *ptr)
{
  // nop
}
void *my_strdup_len(const char *str, size_t sz)
{
  char *result = my_malloc(sz + 1);
  memcpy(result, str, sz);
  result[sz] = '\0';
  return result;
}
void *my_strdup(const char *str)
{
  size_t sz = strlen(str);
  void *result = my_malloc(sz + 1);
  memcpy(result, str, sz + 1);
  return result;
}

int my_malloc_init(void)
{
  sizeof_my_arena = 128*1024;
  my_arena = stir_do_mmap_madvise(sizeof_my_arena);
  if (my_arena == NULL)
  {
    return -1;
  }
  my_arena_ptr = my_arena;
  return 0;
}

void *stir_do_mmap_madvise(size_t bytes)
{
  void *ptr;
  bytes = stir_topages(bytes);
  // Ugh. I wish all systems had simple and compatible interface.
#ifdef MAP_ANON
  #ifdef MAP_NORESERVE
  ptr = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON|MAP_NORESERVE, -1, 0);
  #else
  ptr = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);
  #endif
#else
  #ifdef MAP_ANONYMOUS
    #ifdef MAP_NORESERVE
  ptr = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
    #else
  ptr = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    #endif
  #else
  {
    int fd;
    fd = open("/dev/zero", O_RDWR);
    if (fd < 0)
    {
      abort();
    }
    #ifdef MAP_FILE
    ptr = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FILE, fd, 0);
    #else
    ptr = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    #endif
    close(fd);
  }
  #endif
#endif
#ifdef MADV_DONTNEED
  #ifdef __linux__
  if (ptr && ptr != MAP_FAILED)
  {
    madvise(ptr, bytes, MADV_DONTNEED); // Linux-ism
  }
  #endif
#endif
#ifdef MADV_FREE
  #if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
  if (ptr && ptr != MAP_FAILED)
  {
    madvise(ptr, bytes, MADV_FREE); // *BSD-ism
    // on Linux, MADV_FREE works only on private anonymous pages
    // TODO: not sure about FreeBSD
  }
  #endif
#endif
  if (ptr == MAP_FAILED)
  {
    return NULL;
  }
  return ptr;
}

void stir_do_munmap(void *ptr, size_t bytes)
{
  munmap(ptr, stir_topages(bytes));
}
