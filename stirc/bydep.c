#include "bydep.h"
#include "mymalloc.h"
#include "stircommon.h"
#include "stringtab.h"
#include "rule.h"
#include "bytgt.h"

struct ruleid_by_dep_entry_block_later *ruleid_by_dep_entry_block_later_first;
struct ruleid_by_dep_entry_block_later *ruleid_by_dep_entry_block_later_last;
mysize_t ruleid_by_dep_entry_block_later_cnt;
mysize_t ruleid_by_dep_entry_block_cnt;

struct abce_rb_tree_nocmp ruleids_by_dep[RULEIDS_BY_DEP_SIZE];
struct linked_list_head ruleids_by_dep_list =
  STIR_LINKED_LIST_HEAD_INITER(ruleids_by_dep_list);

mysize_t ruleid_by_dep_entry_cnt;

mysize_t one_ruleid_by_dep_entry_cnt;

void ins_ruleid_by_dep_later(void)
{
  struct ruleid_by_dep_entry_block_later *later;
  unsigned i;
  later = ruleid_by_dep_entry_block_later_first;
  while (later)
  {
    for (i = 0; i < later->cnt; i++)
    {
      ins_ruleid_by_dep2(later->e[i].depid, later->e[i].tgt_ruleid, 1);
    }
    later = later->next;
  }
  ruleid_by_dep_entry_block_later_first = NULL; // let it leak
}

int ins_ruleid_by_dep2(mysize_t depidx, int ruleid, int enforce)
{
  int depruleid;
  struct ruleid_by_dep_entry_block *blk;
  depruleid = get_ruleid_by_tgt(depidx);
  if (depruleid < 0)
  {
    struct ruleid_by_dep_entry_block_later *later;
    if (enforce)
    {
      //mysize_t tgtidx = ABCE_CONTAINER_OF(rules[ruleid]->tgtlist.node.next, struct stirtgt, llnode)->tgtidx;
      //printf("ins_ruleid_by_dep2 enforce dep %s rule %s\n", sttable[depidx].s, sttable[tgtidx].s); // FIXME better message
      return -1;
      //abort();
    }
    if (!ruleid_by_dep_entry_block_later_first)
    {
      ruleid_by_dep_entry_block_later_first = my_malloc(sizeof(*later));
      ruleid_by_dep_entry_block_later_cnt++;
      later = ruleid_by_dep_entry_block_later_first;
      later->cnt = 0;
      later->next = NULL;
      ruleid_by_dep_entry_block_later_last = later;
    }
    later = ruleid_by_dep_entry_block_later_last;
    while (later->cnt >= sizeof(later->e)/sizeof(*later->e))
    {
      if (later->next == NULL)
      {
        later->next = my_malloc(sizeof(*later->next));
        ruleid_by_dep_entry_block_later_cnt++;
	later->next->cnt = 0;
	later->next->next = NULL;
	ruleid_by_dep_entry_block_later_last = later->next;
      }
      later = later->next;
    }
    later->e[later->cnt].depid = depidx;
    later->e[later->cnt].tgt_ruleid = ruleid;
    later->cnt++;
    //mysize_t tgtidx = ABCE_CONTAINER_OF(rules[ruleid]->tgtlist.node.next, struct stirtgt, llnode)->tgtidx;
    //printf("ins_ruleid_by_dep2 dep %s rule %s\n", sttable[depidx].s, sttable[tgtidx].s);
    return 0;
  }
  if (rules[depruleid]->firstdepblock == NULL)
  {
    rules[depruleid]->firstdepblock = my_malloc(sizeof(*rules[depruleid]->firstdepblock));
    ruleid_by_dep_entry_block_cnt++;
    rules[depruleid]->firstdepblock->cnt = 0;
    rules[depruleid]->firstdepblock->next = NULL;
    rules[depruleid]->lastdepblock = rules[depruleid]->firstdepblock;
  }
  blk = rules[depruleid]->lastdepblock;
  while (blk->cnt >= sizeof(blk->ruleid)/sizeof(*blk->ruleid))
  {
    if (blk->next == NULL)
    {
      blk->next = my_malloc(sizeof(*blk->next));
      ruleid_by_dep_entry_block_cnt++;
      blk->next->cnt = 0;
      blk->next->next = NULL;
      rules[depruleid]->lastdepblock = blk->next;
    }
    blk = blk->next;
  }
  blk->ruleid[blk->cnt++] = ruleid;
  return 0;
}
