#include "bydep.h"
#include "mymalloc.h"
#include "stircommon.h"
#include "stringtab.h"
#include "rule.h"
#include "bytgt.h"

struct ruleid_by_dep_entry_block_later *ruleid_by_dep_entry_block_later_first;
struct ruleid_by_dep_entry_block_later *ruleid_by_dep_entry_block_later_last;

static inline int one_ruleid_by_dep_entry_cmp_asym(const void *ruleidv, struct abce_rb_tree_node *n2, void *ud)
{
  const int *ruleid = ruleidv;
  struct one_ruleid_by_dep_entry *e = ABCE_CONTAINER_OF(n2, struct one_ruleid_by_dep_entry, node);
  int r1 = *ruleid;
  int r2 = e->ruleid;
  return (r1>r2) - (r1<r2);
}

static inline int one_ruleid_by_dep_entry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct one_ruleid_by_dep_entry *e1 = ABCE_CONTAINER_OF(n1, struct one_ruleid_by_dep_entry, node);
  struct one_ruleid_by_dep_entry *e2 = ABCE_CONTAINER_OF(n2, struct one_ruleid_by_dep_entry, node);
  int r1 = e1->ruleid;
  int r2 = e2->ruleid;
  return (r1>r2) - (r1<r2);
}

static inline int ruleid_by_dep_entry_cmp_asym(const void *strv, struct abce_rb_tree_node *n2, void *ud)
{
  const mysize_t *str = strv;
  struct ruleid_by_dep_entry *e = ABCE_CONTAINER_OF(n2, struct ruleid_by_dep_entry, node);
  int ret;
  mysize_t str2;
  str2 = e->depidx;
  ret = sizecmp(*str, str2);
  return ret;
}
static inline int ruleid_by_dep_entry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_dep_entry *e1 = ABCE_CONTAINER_OF(n1, struct ruleid_by_dep_entry, node);
  struct ruleid_by_dep_entry *e2 = ABCE_CONTAINER_OF(n2, struct ruleid_by_dep_entry, node);
  int ret;

  ret = sizecmp(e1->depidx, e2->depidx);
  return ret;
}

struct abce_rb_tree_nocmp ruleids_by_dep[RULEIDS_BY_DEP_SIZE];
struct linked_list_head ruleids_by_dep_list =
  STIR_LINKED_LIST_HEAD_INITER(ruleids_by_dep_list);

struct ruleid_by_dep_entry *find_ruleids_by_dep(mysize_t depidx)
{
  uint32_t hash = abce_murmur32(HASH_SEED, depidx);
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;

  abort();

  head = &ruleids_by_dep[hash % (sizeof(ruleids_by_dep)/sizeof(*ruleids_by_dep))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, ruleid_by_dep_entry_cmp_asym, NULL, &depidx);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct ruleid_by_dep_entry, node);
  }
  return NULL;
}

mysize_t ruleid_by_dep_entry_cnt;

struct ruleid_by_dep_entry *ensure_ruleid_by_dep(mysize_t depidx)
{
  uint32_t hash = abce_murmur32(HASH_SEED, depidx);
  struct ruleid_by_dep_entry *e;
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  int ret;
  size_t i;

  head = &ruleids_by_dep[hash % (sizeof(ruleids_by_dep)/sizeof(*ruleids_by_dep))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, ruleid_by_dep_entry_cmp_asym, NULL, &depidx);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct ruleid_by_dep_entry, node);
  }

  ruleid_by_dep_entry_cnt++;
  e = my_malloc(sizeof(*e));
  e->depidx = depidx;
  for (i = 0; i < sizeof(e->one_ruleid_by_dep)/sizeof(*e->one_ruleid_by_dep); i++)
  {
    abce_rb_tree_nocmp_init(&e->one_ruleid_by_dep[i]);
  }
  linked_list_head_init(&e->one_ruleid_by_deplist);

  ret = abce_rb_tree_nocmp_insert_nonexist(head, ruleid_by_dep_entry_cmp_sym, NULL, &e->node);
  if (ret != 0)
  {
    printf("5\n");
    my_abort();
  }
  linked_list_add_tail(&e->llnode, &ruleids_by_dep_list);
  return e;
}

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

void ins_ruleid_by_dep2(mysize_t depidx, int ruleid, int enforce)
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
      return;
      //abort();
    }
    if (!ruleid_by_dep_entry_block_later_first)
    {
      ruleid_by_dep_entry_block_later_first = my_malloc(sizeof(*later));
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
	later->next->cnt = 0;
	later->next->next = NULL;
	ruleid_by_dep_entry_block_later_last = later->next;
      }
      later = later->next;
    }
    later->e[later->cnt].depid = depidx;
    later->e[later->cnt].tgt_ruleid = ruleid;
    later->cnt++;
    mysize_t tgtidx = ABCE_CONTAINER_OF(rules[ruleid]->tgtlist.node.next, struct stirtgt, llnode)->tgtidx;
    //printf("ins_ruleid_by_dep2 dep %s rule %s\n", sttable[depidx].s, sttable[tgtidx].s);
    return;
  }
  if (rules[depruleid]->firstdepblock == NULL)
  {
    rules[depruleid]->firstdepblock = my_malloc(sizeof(*rules[depruleid]->firstdepblock));
    rules[depruleid]->firstdepblock->cnt = 0;
    rules[depruleid]->firstdepblock->next = NULL;
    rules[depruleid]->lastdepblock = rules[depruleid]->firstdepblock;
  }
  blk = rules[depruleid]->lastdepblock;
  while (blk->cnt >= RULEID_BY_DEP_ENTRY_BLOCK_SIZE)
  {
    if (blk->next == NULL)
    {
      blk->next = my_malloc(sizeof(*blk->next));
      blk->next->cnt = 0;
      blk->next->next = NULL;
      rules[depruleid]->lastdepblock = blk->next;
    }
    blk = blk->next;
  }
  blk->ruleid[blk->cnt++] = ruleid;
}

void ins_ruleid_by_dep(mysize_t depidx, int ruleid)
{
#if 0
  struct ruleid_by_dep_entry *e = ensure_ruleid_by_dep(depidx);
  uint32_t hash = abce_murmur32(HASH_SEED, (uint32_t)ruleid);
  struct one_ruleid_by_dep_entry *one;
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  int ret;
  head = &e->one_ruleid_by_dep[hash % (sizeof(e->one_ruleid_by_dep)/sizeof(*e->one_ruleid_by_dep))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, one_ruleid_by_dep_entry_cmp_asym, NULL, &ruleid);
  if (n != NULL)
  {
    return;
  }

  one_ruleid_by_dep_entry_cnt++;
  one = my_malloc(sizeof(*one));
  one->ruleid = ruleid;
  linked_list_add_tail(&one->llnode, &e->one_ruleid_by_deplist);

  ret = abce_rb_tree_nocmp_insert_nonexist(head, one_ruleid_by_dep_entry_cmp_sym, NULL, &one->node);
  if (ret != 0)
  {
    printf("6\n");
    my_abort();
  }
#endif
  return;
}
