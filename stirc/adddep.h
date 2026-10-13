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
  unsigned is_inc:1;
};

struct add_dep_entry_block {
  uint32_t cnt;
  struct one_add_dep_entry_block e[ADD_DEP_ENTRY_BLOCK_SIZE];
  struct add_dep_entry_block *next;
};

extern struct add_dep_entry_block *add_dep_entry_block_first;

extern mysize_t add_dep_entry_block_cnt;

void ins_add_dep(mysize_t tgtidx, mysize_t depidx, mysize_t depidxnodir,
                 int auto_phony, int tgt_phony, int is_inc);

#endif
