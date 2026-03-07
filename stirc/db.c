#include "db.h"
#include "stircommon.h"
#include "mymalloc.h"
#include "stringtab.h"
#include <stdio.h>

int usetsdb = 1;

static inline int tsdbe_cmp_asym(const void *strv, struct abce_rb_tree_node *n2, void *ud)
{
  const mysize_t *str = strv;
  struct tsdbe *e = ABCE_CONTAINER_OF(n2, struct tsdbe, node);
  int ret;
  size_t str2;
  str2 = e->stringtabidx;
  ret = sizecmp(*str, str2);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}
static inline int tsdbe_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct tsdbe *e1 = ABCE_CONTAINER_OF(n1, struct tsdbe, node);
  struct tsdbe *e2 = ABCE_CONTAINER_OF(n2, struct tsdbe, node);
  int ret;
  ret = sizecmp(e1->stringtabidx, e2->stringtabidx);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}

static inline int dbe_cmp_asym(const void *strv, struct abce_rb_tree_node *n2, void *ud)
{
  const mysize_t *str = strv;
  struct dbe *e = ABCE_CONTAINER_OF(n2, struct dbe, node);
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
static inline int dbe_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct dbe *e1 = ABCE_CONTAINER_OF(n1, struct dbe, node);
  struct dbe *e2 = ABCE_CONTAINER_OF(n2, struct dbe, node);
  int ret;
  ret = sizecmp(e1->tgtidx, e2->tgtidx);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}

struct tsdb tsdb = {};
struct db db = {};

void maybe_del_tsdbe(struct tsdb *tsdb, mysize_t tgtidx)
{
  uint32_t hash = abce_murmur32(HASH_SEED, tgtidx);
  struct abce_rb_tree_node *n;
  struct abce_rb_tree_nocmp *head;
  head = &tsdb->byname[hash % (sizeof(tsdb->byname)/sizeof(*tsdb->byname))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, tsdbe_cmp_asym, NULL, &tgtidx);
  if (n == NULL)
  {
    return;
  }
  abce_rb_tree_nocmp_delete(head, n);
  linked_list_delete(&ABCE_CONTAINER_OF(n, struct tsdbe, node)->llnode);
}

void ins_tsdbe(struct tsdb *tsdb, struct tsdbe *tsdbe)
{
  uint32_t hash = abce_murmur32(HASH_SEED, tsdbe->stringtabidx);
  struct abce_rb_tree_nocmp *head;
  int ret;
  head = &tsdb->byname[hash % (sizeof(tsdb->byname)/sizeof(*tsdb->byname))];
  ret = abce_rb_tree_nocmp_insert_nonexist(head, tsdbe_cmp_sym, NULL, &tsdbe->node);
  if (ret != 0)
  {
    struct abce_rb_tree_node *n;
    n = ABCE_RB_TREE_NOCMP_FIND(head, tsdbe_cmp_asym, NULL, &tsdbe->stringtabidx);
    if (n == NULL)
    {
      my_abort();
    }
    abce_rb_tree_nocmp_delete(head, n);
    linked_list_delete(&ABCE_CONTAINER_OF(n, struct tsdbe, node)->llnode);
    ret = abce_rb_tree_nocmp_insert_nonexist(head, tsdbe_cmp_sym, NULL, &tsdbe->node);
    if (ret != 0)
    {
      my_abort();
    }
  }
  linked_list_add_tail(&tsdbe->llnode, &tsdb->ll);
}

void maybe_del_dbe(struct db *db, mysize_t tgtidx)
{
  uint32_t hash = abce_murmur32(HASH_SEED, tgtidx);
  struct abce_rb_tree_node *n;
  struct abce_rb_tree_nocmp *head;
  head = &db->byname[hash % (sizeof(db->byname)/sizeof(*db->byname))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, dbe_cmp_asym, NULL, &tgtidx);
  if (n == NULL)
  {
    return;
  }
  abce_rb_tree_nocmp_delete(head, n);
  linked_list_delete(&ABCE_CONTAINER_OF(n, struct dbe, node)->llnode);
}

void ins_dbe(struct db *db, struct dbe *dbe)
{
  uint32_t hash = abce_murmur32(HASH_SEED, dbe->tgtidx);
  struct abce_rb_tree_nocmp *head;
  int ret;
  head = &db->byname[hash % (sizeof(db->byname)/sizeof(*db->byname))];
  ret = abce_rb_tree_nocmp_insert_nonexist(head, dbe_cmp_sym, NULL, &dbe->node);
  if (ret != 0)
  {
    struct abce_rb_tree_node *n;
    n = ABCE_RB_TREE_NOCMP_FIND(head, dbe_cmp_asym, NULL, &dbe->tgtidx);
    if (n == NULL)
    {
      my_abort();
    }
    abce_rb_tree_nocmp_delete(head, n);
    linked_list_delete(&ABCE_CONTAINER_OF(n, struct dbe, node)->llnode);
    ret = abce_rb_tree_nocmp_insert_nonexist(head, dbe_cmp_sym, NULL, &dbe->node);
    if (ret != 0)
    {
      my_abort();
    }
  }
  linked_list_add_tail(&dbe->llnode, &db->ll);
}

int tsszstoresource(struct tsdb *tsdb, mysize_t stringtabidx, struct timespec ts, off_t sz)
{
  uint32_t hash = abce_murmur32(HASH_SEED, stringtabidx);
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  struct tsdbe *tsdbe;
  //const char *tgtsrc = "source";
  head = &tsdb->byname[hash % (sizeof(tsdb->byname)/sizeof(*tsdb->byname))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, tsdbe_cmp_asym, NULL, &stringtabidx);
#if 0
  if (get_ruleid_by_tgt(stringtabidx) >= 0)
  {
    //return 0; // it's target too
  }
#endif
  if (n == NULL)
  {
    tsdbe = my_malloc(sizeof(struct tsdbe));
    tsdbe->stringtabidx = stringtabidx;
    tsdbe->tsnew = ts;
    tsdbe->ts = ts;
    tsdbe->sznew = sz;
    tsdbe->sz = sz;
    tsdbe->seen = 1;
    ins_tsdbe(tsdb, tsdbe);
    return 0;
  }
  tsdbe = ABCE_CONTAINER_OF(n, struct tsdbe, node);
  tsdbe->tsnew = ts;
  tsdbe->ts = ts;
  tsdbe->sznew = sz;
  tsdbe->sz = sz;
  tsdbe->seen = 1;
  return 1;
}

int tsszstoretarget(struct tsdb *tsdb, mysize_t stringtabidx, struct timespec ts, off_t sz)
{
  uint32_t hash = abce_murmur32(HASH_SEED, stringtabidx);
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  struct tsdbe *tsdbe;
  //const char *tgtsrc = "target";
  head = &tsdb->byname[hash % (sizeof(tsdb->byname)/sizeof(*tsdb->byname))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, tsdbe_cmp_asym, NULL, &stringtabidx);
  if (n == NULL)
  {
    tsdbe = my_malloc(sizeof(struct tsdbe));
    tsdbe->stringtabidx = stringtabidx;
    tsdbe->tsnew = ts;
    tsdbe->ts = ts;
    tsdbe->sznew = sz;
    tsdbe->sz = sz;
    tsdbe->seen = 1;
    ins_tsdbe(tsdb, tsdbe);
    return 0;
  }
  tsdbe = ABCE_CONTAINER_OF(n, struct tsdbe, node);
  tsdbe->tsnew = ts;
  tsdbe->ts = ts;
  tsdbe->sznew = sz;
  tsdbe->sz = sz;
  tsdbe->seen = 1;
  return 1;
}

struct tsdbe *get_tsdbe(mysize_t stringtabidx)
{
  uint32_t hash = abce_murmur32(HASH_SEED, stringtabidx);
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  struct tsdbe *tsdbe;
  head = &tsdb.byname[hash % (sizeof(tsdb.byname)/sizeof(*tsdb.byname))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, tsdbe_cmp_asym, NULL, &stringtabidx);
  if (n == NULL)
  {
    return NULL;
  }
  tsdbe = ABCE_CONTAINER_OF(n, struct tsdbe, node);
  return tsdbe;
}

struct dbe *get_dbe(mysize_t stringtabidx)
{
  uint32_t hash = abce_murmur32(HASH_SEED, stringtabidx);
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  struct dbe *dbe;
  head = &db.byname[hash % (sizeof(db.byname)/sizeof(*db.byname))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, dbe_cmp_asym, NULL, &stringtabidx);
  if (n == NULL)
  {
    return NULL;
  }
  dbe = ABCE_CONTAINER_OF(n, struct dbe, node);
  return dbe;
}
