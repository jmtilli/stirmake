#ifndef _RULE_H_
#define _RULE_H_

#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "stiryy.h"
#include "db.h"
#include "const.h"
#include "syncbuf.h"
// Just in case something could define st_mtim into something different,
// we include these 3 headers in all files that access the token st_mtim
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct stirdep {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  struct linked_list_node dupellnode;
  mysize_t nameidx;
  mysize_t nameidxnodir;
  unsigned is_recursive:1;
  unsigned is_orderonly:1;
  unsigned is_wait:1;
  unsigned is_primary:1;
  unsigned is_dupe:1;
};

struct dep_remain {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  int ruleid;
  int waitcnt;
};

struct stirtgt {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  mysize_t tgtidx;
  mysize_t tgtidxnodir;
  unsigned is_dist:1;
};


/*
 * First, is_executing is set to 1. This means the dependencies of the rule
 * are being executed.
 *
 * Then, is_queued is set to 1. This means the rule is in the queue of processes
 * to fork&exec.
 *
 * Last, is_executed is set to 1 if the sub-process was successful.
 *
 * We shouldn't add any dependencies to a rule whenever is_executing flag is on.
 * XXX or should we? Hard to support dynamic deps without.
 */
struct rule {
  struct linked_list_node remainllnode;
  struct linked_list_node cleanllnode;
  unsigned is_phony:1;
  unsigned is_maybe:1;
  unsigned is_rectgt:1;
  unsigned is_detouch:1;
  unsigned is_executed:1;
  unsigned is_actually_executed:1; // command actually invoked
  unsigned is_executing:1;
  unsigned is_queued:1;
  unsigned is_cleanqueued:1;
  unsigned remain:1;
  unsigned st_mtim_valid:1;
  unsigned is_inc:1; // whether this is included from dependency file
  unsigned is_dist:1;
  unsigned is_cleanhook:1;
  unsigned is_distcleanhook:1;
  unsigned is_bothcleanhook:1;
  unsigned is_forked:1;
  unsigned is_traversed:1;
  unsigned is_under_consideration:1;
  mysize_t diridx;
  mysize_t meatidx;
  struct cmdsrc cmdsrc;
  struct cmd cmd; // calculated from cmdsrc
  struct timespec st_mtim;
  int ruleid;
  struct abce_rb_tree_nocmp tgts[TGTS_SIZE];
  struct linked_list_head tgtlist;
  struct abce_rb_tree_nocmp deps[DEPS_SIZE];
  struct linked_list_head deplist;
  struct linked_list_head depremainlist;
  struct linked_list_head dupedeplist;
  struct abce_rb_tree_nocmp deps_remain[DEPS_REMAIN_SIZE];
  mysize_t deps_remain_cnt;
  mysize_t wait_remain_cnt;
  mysize_t cmdidx;
  mysize_t scopeidx; // abce scope index, but let's use mysize_t here too
  struct stirtgt *curtgt_touch;
  struct syncbuf output;
  struct stirdep *waitloc;
};
extern struct rule **rules; // Needs doubly indirect, otherwise pointers messed up

void zero_rule(struct rule *rule);
void calc_deps_remain(struct rule *rule);
void deps_remain_insert(struct rule *rule, int ruleid);
void deps_remain_erase(struct rule *rule, int ruleid);
void deps_remain_forwait(struct rule *rule, int ruleid);
int deps_remain_has(struct rule *rule, int ruleid);
struct stirtgt *rule_get_tgt(struct rule *rule, mysize_t tgtidx);
void ins_tgt(struct rule *rule, mysize_t tgtidx, mysize_t tgtidxnodir, int is_dist, const char *prefix, int lineno);
int ins_dep(struct rule *rule,
            mysize_t depidx, mysize_t diridx, mysize_t depidxnodir,
            int is_recursive, int orderonly, int wait, int primary);

extern struct linked_list_head rules_remain_list;
extern mysize_t tgt_cnt;
extern mysize_t dep_remain_cnt;
extern mysize_t stirdep_cnt;

static inline void ruleremain_add(struct rule *rule)
{
  if (rule->remain)
  {
    return;
  }
  linked_list_add_tail(&rule->remainllnode, &rules_remain_list);
  rule->remain = 1;
}
static inline void ruleremain_rm(struct rule *rule)
{
  if (!rule->remain)
  {
    return;
  }
  linked_list_delete(&rule->remainllnode);
  rule->remain = 0;
}

#endif
