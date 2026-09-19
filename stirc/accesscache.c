#include "accesscache.h"
#include "mymalloc.h"
#include "stircommon.h"
#include "stirutils.h"
#include "stringtab.h"
#include <unistd.h>

struct abce_rb_tree_nocmp accesshash[ACCESSHASH_SIZE_RB];
struct accesshashentry *accesshashentries = NULL;
mysize_t accesshashentriescnt;
struct linked_list_head accesslrulist =
  STIR_LINKED_LIST_HEAD_INITER(accesslrulist);
struct linked_list_head accessfreelist =
  STIR_LINKED_LIST_HEAD_INITER(accessfreelist);

void accesscache_init(void)
{
  size_t i;
  if (accesshashentriescnt == 0)
  {
    accesshashentriescnt = ACCESSHASH_SIZE_INIT;
    if (accesshashentriescnt > ACCESSHASH_SIZE)
    {
      accesshashentriescnt = ACCESSHASH_SIZE;
    }
  }
  accesshashentries = stir_do_mmap_madvise(accesshashentriescnt*sizeof(*accesshashentries));
  for (i = 0; i < accesshashentriescnt; i++)
  {
    linked_list_add_tail(&accesshashentries[i].llnode, &accessfreelist);
  }
}

void accesscache_grow(void)
{
  size_t oldaccesshashentriescnt = accesshashentriescnt;
  size_t i;
  accesshashentriescnt = 2*accesshashentriescnt;
  if (accesshashentriescnt > ACCESSHASH_SIZE)
  {
    accesshashentriescnt = ACCESSHASH_SIZE;
  }
  if (accesshashentriescnt == oldaccesshashentriescnt)
  {
    return;
  }
  accesshashentries = stir_do_mmap_madvise((accesshashentriescnt-oldaccesshashentriescnt)*sizeof(*accesshashentries)); 
  for (i = 0; i < accesshashentriescnt - oldaccesshashentriescnt; i++)
  {
    linked_list_add_tail(&accesshashentries[i].llnode, &accessfreelist);
  }
}

static inline void accesshashentry_evict(void)
{
  struct accesshashentry *e;
  uint32_t hash;
  struct abce_rb_tree_nocmp *head;
  e = ABCE_CONTAINER_OF(accesslrulist.node.prev, struct accesshashentry, llnode);
  hash = abce_murmur32(HASH_SEED, e->nameidx);
  head = &accesshash[hash % (sizeof(accesshash)/sizeof(*accesshash))];
  linked_list_delete(&e->llnode); 
  abce_rb_tree_nocmp_delete(head, &e->node);
  linked_list_add_head(&e->llnode, &accessfreelist);
}

static inline void accesshashentry_ensure_evict(void)
{
  if (!linked_list_is_empty(&accessfreelist))
  {
    return;
  }
  accesscache_grow();
  if (!linked_list_is_empty(&accessfreelist))
  {
    return;
  }
  accesshashentry_evict();
  if (linked_list_is_empty(&accessfreelist))
  {
    abort();
  }
}

static inline int accesshashentry_cmp_asym(const void *strv, struct abce_rb_tree_node *n2, void *ud)
{
  const mysize_t *str = strv;
  struct accesshashentry *e = ABCE_CONTAINER_OF(n2, struct accesshashentry, node);
  int ret; 
  mysize_t str2;
  str2 = e->nameidx;
  ret = sizecmp(*str, str2);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}
static inline int accesshashentry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct accesshashentry *e1 = ABCE_CONTAINER_OF(n1, struct accesshashentry, node);
  struct accesshashentry *e2 = ABCE_CONTAINER_OF(n2, struct accesshashentry, node);
  int ret;
  ret = sizecmp(e1->nameidx, e2->nameidx);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}

void accesshashentry_evict_all(void)
{
  size_t i;
  linked_list_head_init(&accesslrulist);
  linked_list_head_init(&accessfreelist);
  for (i = 0; i < sizeof(accesshash)/sizeof(*accesshash); i++)
  {
    abce_rb_tree_nocmp_init(&accesshash[i]);
  }
  accesscache_init();
}

void accesshash_evict_named(mysize_t nameidx)
{
  struct accesshashentry *e;
  uint32_t hash;
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;

  hash = abce_murmur32(HASH_SEED, nameidx);
  head = &accesshash[hash % (sizeof(accesshash)/sizeof(*accesshash))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, accesshashentry_cmp_asym, NULL, &nameidx);
  if (n == NULL)
  {
    return;
  } 
  e = ABCE_CONTAINER_OF(n, struct accesshashentry, node);
  linked_list_delete(&e->llnode); 
  abce_rb_tree_nocmp_delete(head, &e->node);
  linked_list_add_head(&e->llnode, &accessfreelist);
}


int access_cached(mysize_t nameidx)
{
  struct abce_rb_tree_node *n;
  struct accesshashentry *e;
  uint32_t hash;
  struct abce_rb_tree_nocmp *head;
  int ret;
  hash = abce_murmur32(HASH_SEED, nameidx);
  head = &accesshash[hash % (sizeof(accesshash)/sizeof(*accesshash))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, accesshashentry_cmp_asym, NULL, &nameidx);
  if (n != NULL)
  {
    e = ABCE_CONTAINER_OF(n, struct accesshashentry, node);
    if (accesslrulist.node.next != &e->llnode)
    {
      linked_list_delete(&e->llnode);
      linked_list_add_head(&e->llnode, &accesslrulist);
    }
    if (accesslrulist.node.next != &e->llnode)
    {
      abort();
    }
    return ABCE_CONTAINER_OF(n, struct accesshashentry, node)->ret;
  }
  accesshashentry_ensure_evict();
  e = ABCE_CONTAINER_OF(accessfreelist.node.next, struct accesshashentry, llnode);
  linked_list_delete(&e->llnode);
  ret = access(sttable[nameidx].s, F_OK);
  e->ret = ret;
  e->nameidx = nameidx;
  linked_list_add_head(&e->llnode, &accesslrulist);
  ret = abce_rb_tree_nocmp_insert_nonexist(head, accesshashentry_cmp_sym, NULL, &e->node);
  if (ret != 0)
  {
    abort();
  }
  return e->ret;
}
