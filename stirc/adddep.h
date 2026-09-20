#ifndef _ADD_DEP_H_
#define _ADD_DEP_H_

#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "stiryy.h"
#include "const.h"

struct one_add_dep_entry_block {
  mysize_t tgtidx;
  mysize_t depidx;
  //mysize_t depidxnodir;
  unsigned auto_phony:1;
  unsigned tgt_phony:1;
};

struct add_dep_entry_block {
  uint32_t cnt;
  struct one_add_dep_entry_block e[ADD_DEP_ENTRY_BLOCK_SIZE];
  struct add_dep_entry_block *next;
};

extern struct add_dep_entry_block *add_dep_entry_block_first;

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
extern mysize_t add_dep_entry_block_cnt;

extern struct linked_list_head add_deplist;

struct add_deps *add_deps_ensure(mysize_t tgtidx);
struct add_dep *add_dep_ensure(struct add_deps *entry, mysize_t depidx, mysize_t depidxnodir);
void ins_add_dep(mysize_t tgtidx, mysize_t depidx, mysize_t depidxnodir,
                 int auto_phony, int tgt_phony);

#endif
