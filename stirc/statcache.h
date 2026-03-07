#ifndef _STATCACHE_H_
#define _STATCACHE_H_

#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "stiryy.h"
#include "const.h"

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
