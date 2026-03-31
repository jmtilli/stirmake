#include "rule.h"
#include "stircommon.h"
#include "stringtab.h"
#include "mymalloc.h"
#include "bytgt.h"

void errxit(const char *fmt, ...);

struct linked_list_head rules_remain_list =
  STIR_LINKED_LIST_HEAD_INITER(rules_remain_list);


static inline int tgt_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct stirtgt *e1 = ABCE_CONTAINER_OF(n1, struct stirtgt, node);
  struct stirtgt *e2 = ABCE_CONTAINER_OF(n2, struct stirtgt, node);
  int ret;
  ret = sizecmp(e1->tgtidx, e2->tgtidx);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}

static inline int tgt_cmp_asym(const void *tgtidxv, struct abce_rb_tree_node *n2, void *ud)
{
  const mysize_t *tgtidx = tgtidxv;
  struct stirtgt *e2 = ABCE_CONTAINER_OF(n2, struct stirtgt, node);
  if (*tgtidx > e2->tgtidx)
  {
    return 1;
  }
  if (*tgtidx < e2->tgtidx)
  {
    return -1;
  }
  return 0;
}

static inline int dep_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct stirdep *e1 = ABCE_CONTAINER_OF(n1, struct stirdep, node);
  struct stirdep *e2 = ABCE_CONTAINER_OF(n2, struct stirdep, node);
  int ret;
  ret = sizecmp(e1->nameidx, e2->nameidx);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}


static inline int dep_remain_cmp_asym(const void *ruleidv, struct abce_rb_tree_node *n2, void *ud)
{
  const int *ruleid = ruleidv;
  struct dep_remain *e = ABCE_CONTAINER_OF(n2, struct dep_remain, node);
  if (*ruleid > e->ruleid)
  {
    return 1;
  }
  if (*ruleid < e->ruleid)
  {
    return -1;
  }
  return 0;
}

static inline int dep_remain_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct dep_remain *e1 = ABCE_CONTAINER_OF(n1, struct dep_remain, node);
  struct dep_remain *e2 = ABCE_CONTAINER_OF(n2, struct dep_remain, node);
  if (e1->ruleid > e2->ruleid)
  {
    return 1;
  }
  if (e1->ruleid < e2->ruleid)
  {
    return -1;
  }
  return 0;
}

void ins_tgt(struct rule *rule, mysize_t tgtidx, mysize_t tgtidxnodir, int is_dist, const char *prefix, int lineno)
{
  uint32_t hash = abce_murmur32(HASH_SEED, tgtidx);
  struct stirtgt *e;
  struct abce_rb_tree_nocmp *head;
  int ret;
  tgt_cnt++;
  e = my_malloc(sizeof(*e));
  e->is_dist = !!is_dist;
  e->tgtidx = tgtidx;
  e->tgtidxnodir = tgtidxnodir;
  head = &rule->tgts[hash % (sizeof(rule->tgts)/sizeof(*rule->tgts))];
  ret = abce_rb_tree_nocmp_insert_nonexist(head, tgt_cmp_sym, NULL, &e->node);
  if (ret != 0)
  {
    if (prefix && lineno >= 0)
    {
      errxit("Target %s already exists in rule at prefix %s line %d", sttable[tgtidx].s, prefix, lineno);
    }
    else
    {
      errxit("Target %s already exists in rule", sttable[tgtidx].s);
    }
    exit(2);
  }
  linked_list_add_tail(&e->llnode, &rule->tgtlist);
}

struct stirtgt *rule_get_tgt(struct rule *rule, mysize_t tgtidx)
{
  struct abce_rb_tree_node *n;
  uint32_t hash = abce_murmur32(HASH_SEED, tgtidx);
  struct abce_rb_tree_nocmp *head;
  head = &rule->tgts[hash % (sizeof(rule->tgts)/sizeof(*rule->tgts))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, tgt_cmp_asym, NULL, &tgtidx);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct stirtgt, node);
  }
  return NULL;
}

mysize_t tgt_cnt;
mysize_t dep_remain_cnt;

int deps_remain_has(struct rule *rule, int ruleid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur32(HASH_SEED, (uint32_t)ruleid);
  hashloc = hashval % (sizeof(rule->deps_remain)/sizeof(*rule->deps_remain));
  n = ABCE_RB_TREE_NOCMP_FIND(&rule->deps_remain[hashloc], dep_remain_cmp_asym, NULL, &ruleid);
  return n != NULL;
}

void deps_remain_forwait(struct rule *rule, int ruleid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  struct dep_remain *dep_remain;
  hashval = abce_murmur32(HASH_SEED, (uint32_t)ruleid);
  hashloc = hashval % (sizeof(rule->deps_remain)/sizeof(*rule->deps_remain));
  n = ABCE_RB_TREE_NOCMP_FIND(&rule->deps_remain[hashloc], dep_remain_cmp_asym, NULL, &ruleid);
  if (n == NULL)
  {
    abort();
  }
  dep_remain = ABCE_CONTAINER_OF(n, struct dep_remain, node);
  dep_remain->waitcnt++;
}

void deps_remain_erase(struct rule *rule, int ruleid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  struct dep_remain *dep_remain;
  hashval = abce_murmur32(HASH_SEED, (uint32_t)ruleid);
  hashloc = hashval % (sizeof(rule->deps_remain)/sizeof(*rule->deps_remain));
  n = ABCE_RB_TREE_NOCMP_FIND(&rule->deps_remain[hashloc], dep_remain_cmp_asym, NULL, &ruleid);
  if (n == NULL)
  {
    return;
  }
  dep_remain = ABCE_CONTAINER_OF(n, struct dep_remain, node);
  abce_rb_tree_nocmp_delete(&rule->deps_remain[hashloc], &dep_remain->node);
  linked_list_delete(&dep_remain->llnode);
  rule->deps_remain_cnt--;
  if (dep_remain->waitcnt < 0 || rule->wait_remain_cnt < (mysize_t)dep_remain->waitcnt)
  {
    abort();
  }
  rule->wait_remain_cnt = rule->wait_remain_cnt - (mysize_t)dep_remain->waitcnt;
  my_free(dep_remain);
}


void deps_remain_insert(struct rule *rule, int ruleid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  struct dep_remain *dep_remain;
  hashval = abce_murmur32(HASH_SEED, (uint32_t)ruleid);
  hashloc = hashval % (sizeof(rule->deps_remain)/sizeof(*rule->deps_remain));
  n = ABCE_RB_TREE_NOCMP_FIND(&rule->deps_remain[hashloc], dep_remain_cmp_asym, NULL, &ruleid);
  if (n != NULL)
  {
    return;
  }
  dep_remain_cnt++;
  dep_remain = my_malloc(sizeof(struct dep_remain));
  dep_remain->ruleid = ruleid;
  dep_remain->waitcnt = 0;
  if (abce_rb_tree_nocmp_insert_nonexist(&rule->deps_remain[hashloc], dep_remain_cmp_sym, NULL, &dep_remain->node) != 0)
  {
    printf("4\n");
    my_abort();
  }
  linked_list_add_tail(&dep_remain->llnode, &rule->depremainlist);
  rule->deps_remain_cnt++;
}

void calc_deps_remain(struct rule *rule)
{
  struct linked_list_node *node;
  LINKED_LIST_FOR_EACH(node, &rule->deplist)
  {
    struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
    mysize_t depnameidx = e->nameidx;
    int ruleid = get_ruleid_by_tgt(depnameidx);
    if (ruleid >= 0)
    {
      deps_remain_insert(rule, ruleid);
    }
  }
}

void zero_rule(struct rule *rule)
{
  memset(rule, 0, sizeof(*rule));
  linked_list_head_init(&rule->deplist);
  linked_list_head_init(&rule->tgtlist);
  syncbuf_init(&rule->output);
  rule->deps_remain_cnt = 0;
  rule->wait_remain_cnt = 0;
  rule->cmdidx = 0;
  rule->curtgt_touch = NULL;
  linked_list_head_init(&rule->depremainlist);
}

mysize_t stirdep_cnt;

int ins_dep(struct rule *rule,
            mysize_t depidx, mysize_t diridx, mysize_t depidxnodir,
            int is_recursive, int orderonly, int wait, int primary)
{
  uint32_t hash = abce_murmur32(HASH_SEED, depidx);
  struct stirdep *e;
  struct abce_rb_tree_nocmp *head;
  int ret;
  stirdep_cnt++;
  e = my_malloc(sizeof(*e));
  e->nameidx = depidx;
  e->nameidxnodir = depidxnodir;
#if 0
  if (strcmp(sttable[diridx].s, ".") == 0 || sttable[depidx].s[0] == '/')
  {
    e->nameidxnodir = depidx;
  }
  else
  {
    char *backpath = construct_backpath(sttable[diridx].s);
    size_t backforthsz = strlen(backpath) + 1 + strlen(sttable[depidx].s) + 1;
    char *backforth = malloc(backforthsz);
    char *can = NULL;
    if (snprintf(backforth, backforthsz, "%s/%s", backpath, sttable[depidx].s)
        >= backforthsz)
    {
      abort();
    }
    free(backpath);
    can = canon(backforth);
    free(backforth);
    e->nameidxnodir = stringtab_add(can);
    free(can);
  }
#endif
  e->is_recursive = !!is_recursive;
  e->is_orderonly = !!orderonly;
  e->is_wait = !!wait;
  e->is_dupe = 0;
  head = &rule->deps[hash % (sizeof(rule->deps)/sizeof(*rule->deps))];
  ret = abce_rb_tree_nocmp_insert_nonexist(head, dep_cmp_sym, NULL, &e->node);
  if (ret == 0)
  {
    //linked_list_add_tail(&e->llnode, &rule->deplist);
  }
  else
  {
    if (debug)
    {
      size_t tgtidx = ABCE_CONTAINER_OF(rule->tgtlist.node.next, struct stirtgt, llnode)->tgtidx;
      fprintf(stderr, "stirmake: duplicate dep %s: %s detected\n",
              sttable[tgtidx].s, sttable[depidx].s);
    }
    e->is_dupe = 1;
    //my_abort();
    ret = -EEXIST;
  }
  linked_list_add_tail(&e->llnode, &rule->deplist);
  e->is_primary = !!primary;
  return ret;
}
