#ifndef _ACCESSCACHE_H_
#define _ACCESSCACHE_H_

#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "stiryy.h"
#include "const.h"
#include <time.h>
#include <unistd.h>


struct accesshashentry {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  mysize_t nameidx;
  int ret;
};

extern mysize_t accesshashentriescnt;

void accesscache_init(void);

void accesscache_grow(void);

void accesshashentry_evict_all(void);

void accesshash_evict_named(mysize_t nameidx);

int access_cached(mysize_t nameidx);

#endif
