#include "bytgt.h"
#include "mymalloc.h"
#include "stringtab.h"
#include "stircommon.h"

void errxit(const char *fmt, ...);

extern struct abce_rb_tree_nocmp ruleid_by_tgt[RULEID_BY_TGT_SIZE];
extern struct linked_list_head ruleid_by_tgt_list;

struct abce_rb_tree_nocmp ruleid_by_tgt[RULEID_BY_TGT_SIZE];
struct linked_list_head ruleid_by_tgt_list =
  STIR_LINKED_LIST_HEAD_INITER(ruleid_by_tgt_list);
mysize_t ruleid_by_tgt_entry_cnt;

static inline int ruleid_by_tgt_entry_cmp_asym(const void *strv, struct abce_rb_tree_node *n2, void *ud)
{
  const mysize_t *str = strv;
  struct ruleid_by_tgt_entry *e = ABCE_CONTAINER_OF(n2, struct ruleid_by_tgt_entry, node);
  int ret;
  size_t str2;
  str2 = e->tgtidx;
  ret = sizecmp(*str, str2);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}
static inline int ruleid_by_tgt_entry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_tgt_entry *e1 = ABCE_CONTAINER_OF(n1, struct ruleid_by_tgt_entry, node);
  struct ruleid_by_tgt_entry *e2 = ABCE_CONTAINER_OF(n2, struct ruleid_by_tgt_entry, node);
  int ret;
  ret = sizecmp(e1->tgtidx, e2->tgtidx);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}

void ins_ruleid_by_tgt(mysize_t tgtidx, int ruleid, const char *prefix, int lineno)
{
  uint32_t hash = abce_murmur32(HASH_SEED, tgtidx);
  struct ruleid_by_tgt_entry *e;
  struct abce_rb_tree_nocmp *head;
  int ret;
  ruleid_by_tgt_entry_cnt++;
  e = my_malloc(sizeof(*e));
  e->tgtidx = tgtidx;
  e->ruleid = ruleid;
  head = &ruleid_by_tgt[hash % (sizeof(ruleid_by_tgt)/sizeof(*ruleid_by_tgt))];
  ret = abce_rb_tree_nocmp_insert_nonexist(head, ruleid_by_tgt_entry_cmp_sym, NULL, &e->node);
  if (ret != 0)
  {
    if (prefix && lineno >= 0)
    {
      errxit("ruleid by tgt %s already exists in rule at prefix %s line %d", sttable[tgtidx].s, prefix, lineno);
    }
    else
    {
      errxit("ruleid by tgt %s already exists", sttable[tgtidx].s);
    }
    exit(2);
  }
  linked_list_add_tail(&e->llnode, &ruleid_by_tgt_list);
}

int get_ruleid_by_tgt(mysize_t tgt)
{
  uint32_t hash = abce_murmur32(HASH_SEED, tgt);
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  head = &ruleid_by_tgt[hash % (sizeof(ruleid_by_tgt)/sizeof(*ruleid_by_tgt))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, ruleid_by_tgt_entry_cmp_asym, NULL, &tgt);
  if (n == NULL)
  {
    return -ENOENT;
  }
  return ABCE_CONTAINER_OF(n, struct ruleid_by_tgt_entry, node)->ruleid;
}
