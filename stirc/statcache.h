#ifndef _STATCACHE_H_
#define _STATCACHE_H_

#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "stiryy.h"
#include "const.h"
// Just in case something could define st_mtim into something different,
// we include these 3 headers in all files that access the token st_mtim
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


struct stathashentry {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  mysize_t nameidx;
  int ret;
  mode_t st_mode;
  struct timespec st_mtim;
  off_t st_size;
};

extern mysize_t stathashentriescnt;

void statcache_init(void);

void statcache_grow(void);

void stathashentry_evict_all(void);

void lstat_evict_named(mysize_t nameidx);

struct stathashentry *lstat_cached(mysize_t nameidx);

#endif
