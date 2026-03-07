#include "statcache.h"
#include "mymalloc.h"
#include "stircommon.h"
#include "stringtab.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct abce_rb_tree_nocmp stathash[STATHASH_SIZE_RB];
//struct stathashentry stathashentries[STATHASH_SIZE];
struct stathashentry *stathashentries = NULL;
mysize_t stathashentriescnt;
struct linked_list_head statlrulist =
  STIR_LINKED_LIST_HEAD_INITER(statlrulist);
struct linked_list_head statfreelist =
  STIR_LINKED_LIST_HEAD_INITER(statfreelist);

void statcache_init(void)
{
  size_t i;
  if (stathashentriescnt == 0)
  {
    stathashentriescnt = STATHASH_SIZE_INIT;
    if (stathashentriescnt > STATHASH_SIZE)
    {
      stathashentriescnt = STATHASH_SIZE;
    }
  }
  stathashentries = stir_do_mmap_madvise(stathashentriescnt*sizeof(*stathashentries));
  for (i = 0; i < stathashentriescnt; i++)
  {
    linked_list_add_tail(&stathashentries[i].llnode, &statfreelist);
  }
}

void statcache_grow(void)
{
  size_t oldstathashentriescnt = stathashentriescnt;
  size_t i;
  stathashentriescnt = 2*stathashentriescnt;
  if (stathashentriescnt > STATHASH_SIZE)
  {
    stathashentriescnt = STATHASH_SIZE;
  }
  if (stathashentriescnt == oldstathashentriescnt)
  {
    return;
  }
  stathashentries = stir_do_mmap_madvise((stathashentriescnt-oldstathashentriescnt)*sizeof(*stathashentries)); 
  for (i = 0; i < stathashentriescnt - oldstathashentriescnt; i++)
  {
    linked_list_add_tail(&stathashentries[i].llnode, &statfreelist);
  }
}

static inline void stathashentry_evict(void)
{
  struct stathashentry *e;
  uint32_t hash;
  struct abce_rb_tree_nocmp *head;
  e = ABCE_CONTAINER_OF(statlrulist.node.prev, struct stathashentry, llnode);
  hash = abce_murmur32(HASH_SEED, e->nameidx);
  head = &stathash[hash % (sizeof(stathash)/sizeof(*stathash))];
  linked_list_delete(&e->llnode); 
  abce_rb_tree_nocmp_delete(head, &e->node);
  linked_list_add_head(&e->llnode, &statfreelist);
}

static inline void stathashentry_ensure_evict(void)
{
  if (!linked_list_is_empty(&statfreelist))
  {
    return;
  }
  statcache_grow();
  if (!linked_list_is_empty(&statfreelist))
  {
    return;
  }
  stathashentry_evict();
  if (linked_list_is_empty(&statfreelist))
  {
    abort();
  }
}

static inline int stathashentry_cmp_asym(const void *strv, struct abce_rb_tree_node *n2, void *ud)
{
  const mysize_t *str = strv;
  struct stathashentry *e = ABCE_CONTAINER_OF(n2, struct stathashentry, node);
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
static inline int stathashentry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct stathashentry *e1 = ABCE_CONTAINER_OF(n1, struct stathashentry, node);
  struct stathashentry *e2 = ABCE_CONTAINER_OF(n2, struct stathashentry, node);
  int ret;
  ret = sizecmp(e1->nameidx, e2->nameidx);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}

void stathashentry_evict_all(void)
{
  size_t i;
  linked_list_head_init(&statlrulist);
  linked_list_head_init(&statfreelist);
  for (i = 0; i < sizeof(stathash)/sizeof(*stathash); i++)
  {
    abce_rb_tree_nocmp_init(&stathash[i]);
  }
  statcache_init();
}

void lstat_evict_named(mysize_t nameidx)
{
  struct stathashentry *e;
  uint32_t hash;
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;

  hash = abce_murmur32(HASH_SEED, nameidx);
  head = &stathash[hash % (sizeof(stathash)/sizeof(*stathash))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, stathashentry_cmp_asym, NULL, &nameidx);
  if (n == NULL)
  {
    return;
  } 
  e = ABCE_CONTAINER_OF(n, struct stathashentry, node);
  linked_list_delete(&e->llnode); 
  abce_rb_tree_nocmp_delete(head, &e->node);
  linked_list_add_head(&e->llnode, &statfreelist);
}


struct stathashentry *lstat_cached(mysize_t nameidx)
{
  struct abce_rb_tree_node *n;
  struct stathashentry *e;
  uint32_t hash;
  struct abce_rb_tree_nocmp *head;
  int ret;
  struct stat statbuf;
  hash = abce_murmur32(HASH_SEED, nameidx);
  head = &stathash[hash % (sizeof(stathash)/sizeof(*stathash))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, stathashentry_cmp_asym, NULL, &nameidx);
  if (n != NULL)
  {
    e = ABCE_CONTAINER_OF(n, struct stathashentry, node);
    if (statlrulist.node.next != &e->llnode)
    {
      linked_list_delete(&e->llnode);
      linked_list_add_head(&e->llnode, &statlrulist);
    }
    if (statlrulist.node.next != &e->llnode)
    {
      abort();
    }
    return ABCE_CONTAINER_OF(n, struct stathashentry, node);
  }
  stathashentry_ensure_evict();
  e = ABCE_CONTAINER_OF(statfreelist.node.next, struct stathashentry, llnode);
  linked_list_delete(&e->llnode);
  ret = lstat(sttable[nameidx].s, &statbuf);
  if (ret == 0)
  {
    e->st_mode = statbuf.st_mode;
    e->st_mtim = statbuf.st_mtim;
    e->st_size = statbuf.st_size;
  }
  else
  {
    e->st_mode = 0;
    e->st_mtim.tv_sec = 0;
    e->st_mtim.tv_nsec = 0;
    e->st_size = 0;
  }
  e->ret = ret;
  e->nameidx = nameidx;
  linked_list_add_head(&e->llnode, &statlrulist);
  ret = abce_rb_tree_nocmp_insert_nonexist(head, stathashentry_cmp_sym, NULL, &e->node);
  if (ret != 0)
  {
    abort();
  }
  return e;
}
