#ifndef _BYTGT_H_
#define _BYTGT_H_

#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "stiryy.h"
#include "const.h"

struct ruleid_by_tgt_entry {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  int ruleid;
  //char *tgt;
  mysize_t tgtidx;
};

extern mysize_t ruleid_by_tgt_entry_cnt;

void ins_ruleid_by_tgt(mysize_t tgtidx, int ruleid, const char *prefix, int lineno);
int get_ruleid_by_tgt(mysize_t tgt);

#endif
