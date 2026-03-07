#ifndef _ADD_DEP_H_
#define _ADD_DEP_H_

#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "stiryy.h"
#include "const.h"

struct add_dep {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  mysize_t depidx;
  mysize_t depidxnodir;
  unsigned auto_phony:1;
};

struct add_deps {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  mysize_t tgtidx;
  struct abce_rb_tree_nocmp add_deps[ADD_DEP_SIZE];
  struct linked_list_head add_deplist;
  unsigned phony:1;
};

extern mysize_t add_deps_cnt;
extern mysize_t add_dep_cnt;

extern struct linked_list_head add_deplist;

struct add_deps *add_deps_ensure(mysize_t tgtidx);
struct add_dep *add_dep_ensure(struct add_deps *entry, mysize_t depidx, mysize_t depidxnodir);

#endif
