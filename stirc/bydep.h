#ifndef _BYDEP_H_
#define _BYDEP_H_

#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "stiryy.h"
#include "const.h"

struct one_ruleid_by_dep_entry_block_later {
  mysize_t depid;
  int tgt_ruleid;
};

struct ruleid_by_dep_entry_block_later {
  uint32_t cnt;
  struct one_ruleid_by_dep_entry_block_later e[RULEID_BY_DEP_ENTRY_BLOCK_SIZE/2];
  struct ruleid_by_dep_entry_block_later *next;
};

extern struct ruleid_by_dep_entry_block_later *ruleid_by_dep_entry_block_later_first;

struct ruleid_by_dep_entry_block {
  uint32_t cnt;
  int ruleid[RULEID_BY_DEP_ENTRY_BLOCK_SIZE];
  struct ruleid_by_dep_entry_block *next;
};

struct one_ruleid_by_dep_entry {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  int ruleid;
};

struct ruleid_by_dep_entry {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  mysize_t depidx;
  struct abce_rb_tree_nocmp one_ruleid_by_dep[ONE_RULEID_BY_DEP_SIZE];
  struct linked_list_head one_ruleid_by_deplist;
};

extern mysize_t ruleid_by_dep_entry_cnt;
extern mysize_t one_ruleid_by_dep_entry_cnt;

struct ruleid_by_dep_entry *find_ruleids_by_dep(mysize_t depidx);
void ins_ruleid_by_dep(mysize_t depidx, int ruleid);
void ins_ruleid_by_dep2(mysize_t depidx, int ruleid, int enforce);
void ins_ruleid_by_dep_later(void);

#endif
