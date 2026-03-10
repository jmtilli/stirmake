#include "adddep.h"
#include "stircommon.h"
#include "mymalloc.h"

struct abce_rb_tree_nocmp add_deps[ADD_DEPS_SIZE];

struct linked_list_head add_deplist = STIR_LINKED_LIST_HEAD_INITER(add_deplist);

static inline int add_dep_cmp_asym(const void *strv, struct abce_rb_tree_node *n2, void *ud)
{
  const mysize_t *str = strv;
  struct add_dep *e = ABCE_CONTAINER_OF(n2, struct add_dep, node);
  int ret;
  mysize_t str2;
  str2 = e->depidx;
  ret = sizecmp(*str, str2);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}
static inline int add_dep_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct add_dep *e1 = ABCE_CONTAINER_OF(n1, struct add_dep, node);
  struct add_dep *e2 = ABCE_CONTAINER_OF(n2, struct add_dep, node);
  int ret;

  ret = sizecmp(e1->depidx, e2->depidx);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}

static inline int add_deps_cmp_asym(const void *strv, struct abce_rb_tree_node *n2, void *ud)
{
  const mysize_t *str = strv;
  struct add_deps *e = ABCE_CONTAINER_OF(n2, struct add_deps, node);
  int ret;
  mysize_t str2;
  str2 = e->tgtidx;
  ret = sizecmp(*str, str2);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}
static inline int add_deps_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct add_deps *e1 = ABCE_CONTAINER_OF(n1, struct add_deps, node);
  struct add_deps *e2 = ABCE_CONTAINER_OF(n2, struct add_deps, node);
  int ret;

  ret = sizecmp(e1->tgtidx, e2->tgtidx);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}

mysize_t add_dep_cnt;

struct add_dep *add_dep_ensure(struct add_deps *entry, mysize_t depidx, mysize_t depidxnodir)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  struct add_dep *entry2;
  hashval = abce_murmur32(HASH_SEED, depidx);
  hashloc = hashval % (sizeof(entry->add_deps)/sizeof(*entry->add_deps));
  n = ABCE_RB_TREE_NOCMP_FIND(&entry->add_deps[hashloc], add_dep_cmp_asym, NULL, &depidx);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct add_dep, node);
  }
  add_dep_cnt++;
  entry2 = my_malloc(sizeof(struct add_dep));
  entry2->depidx = depidx;
  entry2->depidxnodir = depidxnodir;
  entry2->auto_phony = 0;
  if (abce_rb_tree_nocmp_insert_nonexist(&entry->add_deps[hashloc], add_dep_cmp_sym, NULL, &entry2->node) != 0)
  {
    printf("7\n");
    my_abort();
  }
  linked_list_add_tail(&entry2->llnode, &entry->add_deplist);
  return entry2;
}

mysize_t add_deps_cnt;

struct add_deps *add_deps_ensure(mysize_t tgtidx)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  size_t i;
  struct add_deps *entry;
  hashval = abce_murmur32(HASH_SEED, tgtidx);
  hashloc = hashval % (sizeof(add_deps)/sizeof(*add_deps));
  n = ABCE_RB_TREE_NOCMP_FIND(&add_deps[hashloc], add_deps_cmp_asym, NULL, &tgtidx);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct add_deps, node);
  }
  add_deps_cnt++;
  entry = my_malloc(sizeof(struct add_deps));
  entry->tgtidx = tgtidx;
  entry->phony = 0;
  for (i = 0; i < sizeof(entry->add_deps)/sizeof(*entry->add_deps); i++)
  {
    abce_rb_tree_nocmp_init(&entry->add_deps[i]);
  }
  linked_list_head_init(&entry->add_deplist);
  if (abce_rb_tree_nocmp_insert_nonexist(&add_deps[hashloc], add_deps_cmp_sym, NULL, &entry->node) != 0)
  {
    printf("8\n");
    my_abort();
  }
  linked_list_add_tail(&entry->llnode, &add_deplist);
  return entry;
}
