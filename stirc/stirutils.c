#include "stirutils.h"
#include "canon.h"
#include "mymalloc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/sysctl.h>
#if __FreeBSD_version >= 1003000
#define HAS_UTIMENSAT 1
#endif
#endif
#ifdef __DragonFly__
#include <sys/param.h>
#include <sys/sysctl.h>
#if __DragonFly_version >= 400100
#define HAS_UTIMENSAT 1
#endif
#endif
#ifdef __linux__
#include <sys/sysinfo.h>
#define HAS_UTIMENSAT 1
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
// Don't know how to detect MacOS version, so HAS_UTIMENSAT not set
#endif
#ifdef __NetBSD__
#include <sys/param.h>
#include <sys/sysctl.h>
#if __NetBSD_Version__ >= 600000000
#define HAS_UTIMENSAT 1
#endif
#endif
#ifdef __OpenBSD__
#include <sys/param.h>
#include <sys/sysctl.h>
// Don't know how to detect OpenBSD version, so HAS_UTIMENSAT not set
#endif

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

int my_get_nprocs_impl(void)
{
#ifdef _SC_NPROCESSORS_ONLN
  return sysconf(_SC_NPROCESSORS_ONLN);
#else
  #ifdef __linux__
  return get_nprocs();
  #else
    #ifdef __APPLE__
  int count = 1;
  size_t count_len = sizeof(count);
  if (sysctlbyname("hw.logicalcpu", &count, &count_len, NULL, 0) != 0 ||
      count < 1)
  {
    fprintf(stderr, "stirmake: can't detect CPU count, assuming 1.\n");
    return 1;
  }
  return count;
    #else
      #ifdef __FreeBSD__
  int count = 1;
  size_t count_len = sizeof(count);
  if (sysctlbyname("hw.ncpu", &count, &count_len, NULL, 0) != 0 ||
      count < 1)
  {
    fprintf(stderr, "stirmake: can't detect CPU count, assuming 1.\n");
    return 1;
  }
  return count;
      #else
        #ifdef __NetBSD__
  int count = 1;
  size_t count_len = sizeof(count);
  if (sysctlbyname("hw.ncpuonline", &count, &count_len, NULL, 0) != 0 ||
      count < 1)
  {
    fprintf(stderr, "stirmake: can't detect CPU count, assuming 1.\n");
    return 1;
  }
  return count;
        #else
          #ifdef __DragonFly__
  int count = 1;
  size_t count_len = sizeof(count);
  if (sysctlbyname("hw.ncpu", &count, &count_len, NULL, 0) != 0 ||
      count < 1)
  {
    fprintf(stderr, "stirmake: can't detect CPU count, assuming 1.\n");
    return 1;
  }
  return count;
	  #else
  fprintf(stderr, "stirmake: can't detect CPU count, assuming 1.\n");
  return 1;
	  #endif
        #endif
      #endif
    #endif
  #endif
#endif
}

int my_get_nprocs(void)
{
  int ret = my_get_nprocs_impl();
  if (ret < 1)
  {
    fprintf(stderr, "stirmake: can't detect CPU count, assuming 1.\n");
    return 1;
  }
  if (ret > 384)
  {
    fprintf(stderr, "stirmake: capping CPU count at 384.\n");
    return 384; // To avoid hitting select() file descriptor count
  }
  return ret;
}

int utimensat_both_emul(const char *pathname, struct timespec time, int l,
                        int forwards)
{
#ifdef HAS_UTIMENSAT
  struct timespec timespecs[2];
  timespecs[0] = time;
  timespecs[1] = time;
#else
  struct timeval times[2];
  times[0].tv_sec = time.tv_sec;
  times[0].tv_usec = (time.tv_nsec+999*(!!forwards))/1000;
  times[1].tv_sec = time.tv_sec;
  times[1].tv_usec = (time.tv_nsec+999*(!!forwards))/1000;
#endif

#ifdef HAS_UTIMENSAT
  return utimensat(AT_FDCWD, pathname, timespecs, l ? AT_SYMLINK_NOFOLLOW : 0);
#else
  {
    struct timespec req;
    struct timespec rem;
    int utimeret;
    utimeret = utimes(pathname, times); // Ugh. Can't change symlink time!
    if (forwards)
    {
      req.tv_sec = 0;
      req.tv_nsec = 2000; // let's sleep for 2 us to be rather safe than sorry
      for (;;)
      {
        int ret = nanosleep(&req, &rem);
        if (ret == 0 || (ret != 0 && errno != EINTR))
        {
          break;
        }
        req = rem;
      }
    }
    return utimeret;
  }
#endif
}
