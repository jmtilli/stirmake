#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <locale.h>
#include <libgen.h>
#include "yyutils.h"
#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#if 0
#include "opcodes.h"
#include "engine.h"
#endif
#include "incyyutils.h"

enum mode {
  MODE_NONE = 0,
  MODE_THIS = 1,
  MODE_PROJECT = 2,
  MODE_ALL = 3,
};

enum mode mode = MODE_NONE;
int isspecprog = 0;

#define STIR_LINKED_LIST_HEAD_INITER(x) { \
  .node = { \
    .prev = &(x).node, \
    .next = &(x).node, \
  }, \
}

enum {
  RULEID_BY_TGT_SIZE = 8192,
  RULEIDS_BY_DEP_SIZE = 8192,
  TGTS_SIZE = 64,
  DEPS_SIZE = 64,
  DEPS_REMAIN_SIZE = 64,
  ONE_RULEID_BY_DEP_SIZE = 64,
  ADD_DEP_SIZE = 64,
  ADD_DEPS_SIZE = 8192,
  RULEID_BY_PID_SIZE = 64,
  STRINGTAB_SIZE = 8192,
};


int debug = 1;

int self_pipe_fd[2];

int jobserver_fd[2];

struct stringtabentry {
  struct abce_rb_tree_node node;
  char *string;
  size_t idx;
};

void update_recursive_pid(int parent)
{
  pid_t pid = parent ? getppid() : getpid();
  char buf[64] = {};
  snprintf(buf, sizeof(buf), "%d", (int)pid);
  setenv("STIRMAKEPID", buf, 1);
}

static inline int stringtabentry_cmp_asym(const char *str, struct abce_rb_tree_node *n2, void *ud)
{
  struct stringtabentry *e = ABCE_CONTAINER_OF(n2, struct stringtabentry, node);
  int ret;
  char *str2;
  str2 = e->string;
  ret = strcmp(str, str2);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}
static inline int stringtabentry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct stringtabentry *e1 = ABCE_CONTAINER_OF(n1, struct stringtabentry, node);
  struct stringtabentry *e2 = ABCE_CONTAINER_OF(n2, struct stringtabentry, node);
  int ret;
  ret = strcmp(e1->string, e2->string);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}

char my_arena[1536*1024*1024];
char *my_arena_ptr = my_arena;

void *my_malloc(size_t sz)
{
  void *result = my_arena_ptr;
  my_arena_ptr += (sz+7)/8*8;
  if (my_arena_ptr >= my_arena + sizeof(my_arena))
  {
    printf("OOM\n");
    abort();
  }
  return result;
}
void my_free(void *ptr)
{
  // nop
}
void *my_strdup(const char *str)
{
  size_t sz = strlen(str);
  void *result = my_malloc(sz + 1);
  memcpy(result, str, sz + 1);
  return result;
}


struct abce_rb_tree_nocmp st[STRINGTAB_SIZE];
char *sttable[1048576]; // RFE make variable sized
size_t st_cnt;

size_t stringtab_cnt = 0;

size_t stringtab_add(const char *symbol)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur_buf(0x12345678U, symbol, strlen(symbol));
  hashloc = hashval % (sizeof(st)/sizeof(*st));
  n = ABCE_RB_TREE_NOCMP_FIND(&st[hashloc], stringtabentry_cmp_asym, 
NULL, symbol);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct stringtabentry, node)->idx;
  }
  stringtab_cnt++;
  struct stringtabentry *stringtabentry = my_malloc(sizeof(struct stringtabentry));
  stringtabentry->string = my_strdup(symbol);
  sttable[st_cnt] = stringtabentry->string;
  stringtabentry->idx = st_cnt++;
  if (abce_rb_tree_nocmp_insert_nonexist(&st[hashloc], stringtabentry_cmp_sym, NULL, &stringtabentry->node) != 0)
  {
    printf("23\n");
    abort();
  }
  return stringtabentry->idx;
}

size_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen)
{
  if (strlen(symbol) != symlen)
  {
    printf("22\n");
    abort(); // RFE what to do?
  }
  return stringtab_add(symbol);
}


struct ruleid_by_tgt_entry {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  int ruleid;
  //char *tgt;
  size_t tgtidx;
};

int sizecmp(size_t size1, size_t size2)
{
  if (size1 > size2)
  {
    return 1;
  }
  if (size1 < size2)
  {
    return -1;
  }
  return 0;
}

struct abce_rb_tree_nocmp ruleid_by_tgt[RULEID_BY_TGT_SIZE];
struct linked_list_head ruleid_by_tgt_list =
  STIR_LINKED_LIST_HEAD_INITER(ruleid_by_tgt_list);

static inline int ruleid_by_tgt_entry_cmp_asym(size_t str, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_tgt_entry *e = ABCE_CONTAINER_OF(n2, struct ruleid_by_tgt_entry, node);
  int ret;
  size_t str2;
  str2 = e->tgtidx;
  ret = sizecmp(str, str2);
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

size_t ruleid_by_tgt_entry_cnt;

void ins_ruleid_by_tgt(size_t tgtidx, int ruleid)
{
  uint32_t hash = abce_murmur32(0x12345678U, tgtidx);
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
    printf("1\n");
    abort();
  }
  linked_list_add_tail(&e->llnode, &ruleid_by_tgt_list);
}

int get_ruleid_by_tgt(size_t tgt)
{
  uint32_t hash = abce_murmur32(0x12345678U, tgt);
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  head = &ruleid_by_tgt[hash % (sizeof(ruleid_by_tgt)/sizeof(*ruleid_by_tgt))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, ruleid_by_tgt_entry_cmp_asym, NULL, tgt);
  if (n == NULL)
  {
    return -ENOENT;
  }
  return ABCE_CONTAINER_OF(n, struct ruleid_by_tgt_entry, node)->ruleid;
}

struct cmd {
  char ***args;
};

int ts_cmp(struct timespec ta, struct timespec tb)
{
  if (ta.tv_sec > tb.tv_sec)
  {
    return 1;
  }
  if (ta.tv_sec < tb.tv_sec)
  {
    return -1;
  }
  if (ta.tv_nsec > tb.tv_nsec)
  {
    return 1;
  }
  if (ta.tv_nsec < tb.tv_nsec)
  {
    return -1;
  }
  return 0;
}

struct stirdep {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  size_t nameidx;
  unsigned is_recursive:1;
};

struct dep_remain {
  struct abce_rb_tree_node node;
  //struct linked_list_node llnode;
  int ruleid;
};

static inline int dep_remain_cmp_asym(int ruleid, struct abce_rb_tree_node *n2, void *ud)
{
  struct dep_remain *e = ABCE_CONTAINER_OF(n2, struct dep_remain, node);
  if (ruleid > e->ruleid)
  {
    return 1;
  }
  if (ruleid < e->ruleid)
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

struct tgt {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  size_t tgtidx;
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
  unsigned is_phony:1;
  unsigned is_executed:1;
  unsigned is_executing:1;
  unsigned is_queued:1;
  struct cmd cmd;
  int ruleid;
  struct abce_rb_tree_nocmp tgts[TGTS_SIZE];
  struct linked_list_head tgtlist;
  struct abce_rb_tree_nocmp deps[DEPS_SIZE];
  struct linked_list_head deplist;
  struct abce_rb_tree_nocmp deps_remain[DEPS_REMAIN_SIZE];
  //struct linked_list_head depremainlist;
  size_t deps_remain_cnt; // XXX return this for less memory use?
};

static inline int tgt_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct tgt *e1 = ABCE_CONTAINER_OF(n1, struct tgt, node);
  struct tgt *e2 = ABCE_CONTAINER_OF(n2, struct tgt, node);
  int ret;
  ret = sizecmp(e1->tgtidx, e2->tgtidx);
  if (ret != 0)
  {
    return ret;
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

size_t tgt_cnt;


void ins_tgt(struct rule *rule, size_t tgtidx)
{
  uint32_t hash = abce_murmur32(0x12345678U, tgtidx);
  struct tgt *e;
  struct abce_rb_tree_nocmp *head;
  int ret;
  tgt_cnt++;
  e = my_malloc(sizeof(*e));
  e->tgtidx = tgtidx;
  head = &rule->tgts[hash % (sizeof(rule->tgts)/sizeof(*rule->tgts))];
  ret = abce_rb_tree_nocmp_insert_nonexist(head, tgt_cmp_sym, NULL, &e->node);
  if (ret != 0)
  {
    printf("2\n");
    abort();
  }
  linked_list_add_tail(&e->llnode, &rule->tgtlist);
}

size_t stirdep_cnt;

void ins_dep(struct rule *rule, size_t depidx, int is_recursive)
{
  uint32_t hash = abce_murmur32(0x12345678U, depidx);
  struct stirdep *e;
  struct abce_rb_tree_nocmp *head;
  int ret;
  stirdep_cnt++;
  e = my_malloc(sizeof(*e));
  e->nameidx = depidx;
  e->is_recursive = !!is_recursive;
  head = &rule->deps[hash % (sizeof(rule->deps)/sizeof(*rule->deps))];
  ret = abce_rb_tree_nocmp_insert_nonexist(head, dep_cmp_sym, NULL, &e->node);
  if (ret != 0)
  {
    printf("3\n");
    abort();
  }
  linked_list_add_tail(&e->llnode, &rule->deplist);
}

int deps_remain_has(struct rule *rule, int ruleid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur32(0x12345678U, ruleid);
  hashloc = hashval % (sizeof(rule->deps_remain)/sizeof(*rule->deps_remain));
  n = ABCE_RB_TREE_NOCMP_FIND(&rule->deps_remain[hashloc], dep_remain_cmp_asym, NULL, ruleid);
  return n != NULL;
}

void deps_remain_erase(struct rule *rule, int ruleid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur32(0x12345678U, ruleid);
  hashloc = hashval % (sizeof(rule->deps_remain)/sizeof(*rule->deps_remain));
  n = ABCE_RB_TREE_NOCMP_FIND(&rule->deps_remain[hashloc], dep_remain_cmp_asym, NULL, ruleid);
  if (n == NULL)
  {
    return;
  }
  struct dep_remain *dep_remain = ABCE_CONTAINER_OF(n, struct dep_remain, node);
  abce_rb_tree_nocmp_delete(&rule->deps_remain[hashloc], &dep_remain->node);
  //linked_list_delete(&dep_remain->llnode);
  rule->deps_remain_cnt--;
  my_free(dep_remain);
}

size_t dep_remain_cnt;

void deps_remain_insert(struct rule *rule, int ruleid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur32(0x12345678U, ruleid);
  hashloc = hashval % (sizeof(rule->deps_remain)/sizeof(*rule->deps_remain));
  n = ABCE_RB_TREE_NOCMP_FIND(&rule->deps_remain[hashloc], dep_remain_cmp_asym, NULL, ruleid);
  if (n != NULL)
  {
    return;
  }
  dep_remain_cnt++;
  struct dep_remain *dep_remain = my_malloc(sizeof(struct dep_remain));
  dep_remain->ruleid = ruleid;
  if (abce_rb_tree_nocmp_insert_nonexist(&rule->deps_remain[hashloc], dep_remain_cmp_sym, NULL, &dep_remain->node) != 0)
  {
    printf("4\n");
    abort();
  }
  //linked_list_add_tail(&dep_remain->llnode, &rule->depremainlist);
  rule->deps_remain_cnt++;
}

void calc_deps_remain(struct rule *rule)
{
  size_t i;
  struct linked_list_node *node;
  LINKED_LIST_FOR_EACH(node, &rule->deplist)
  {
    struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
    size_t depnameidx = e->nameidx;
    int ruleid = get_ruleid_by_tgt(depnameidx);
    if (ruleid >= 0)
    {
      deps_remain_insert(rule, ruleid);
    }
  }
}

int children = 0;
const int limit = 2;

struct rule **rules; // Needs doubly indirect, otherwise pointers messed up
size_t rules_capacity;
size_t rules_size;

struct one_ruleid_by_dep_entry {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  int ruleid;
};

static inline int one_ruleid_by_dep_entry_cmp_asym(int ruleid, struct abce_rb_tree_node *n2, void *ud)
{
  struct one_ruleid_by_dep_entry *e = ABCE_CONTAINER_OF(n2, struct one_ruleid_by_dep_entry, node);
  if (ruleid > e->ruleid)
  {
    return 1;
  }
  if (ruleid < e->ruleid)
  {
    return -1;
  }
  return 0;
}

static inline int one_ruleid_by_dep_entry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct one_ruleid_by_dep_entry *e1 = ABCE_CONTAINER_OF(n1, struct one_ruleid_by_dep_entry, node);
  struct one_ruleid_by_dep_entry *e2 = ABCE_CONTAINER_OF(n2, struct one_ruleid_by_dep_entry, node);
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

struct ruleid_by_dep_entry {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  size_t depidx;
  struct abce_rb_tree_nocmp one_ruleid_by_dep[ONE_RULEID_BY_DEP_SIZE];
  struct linked_list_head one_ruleid_by_deplist;
};

static inline int ruleid_by_dep_entry_cmp_asym(size_t str, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_dep_entry *e = ABCE_CONTAINER_OF(n2, struct ruleid_by_dep_entry, node);
  int ret;
  size_t str2;
  str2 = e->depidx;
  ret = sizecmp(str, str2);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}
static inline int ruleid_by_dep_entry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_dep_entry *e1 = ABCE_CONTAINER_OF(n1, struct ruleid_by_dep_entry, node);
  struct ruleid_by_dep_entry *e2 = ABCE_CONTAINER_OF(n2, struct ruleid_by_dep_entry, node);
  int ret;
  
  ret = sizecmp(e1->depidx, e2->depidx);
  if (ret != 0)
  {
    return ret;
  }
  return 0;
}

struct abce_rb_tree_nocmp ruleids_by_dep[RULEIDS_BY_DEP_SIZE];
struct linked_list_head ruleids_by_dep_list =
  STIR_LINKED_LIST_HEAD_INITER(ruleids_by_dep_list);

struct ruleid_by_dep_entry *find_ruleids_by_dep(size_t depidx)
{
  uint32_t hash = abce_murmur32(0x12345678U, depidx);
  struct ruleid_by_dep_entry *e;
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  int ret;
  size_t i;

  head = &ruleids_by_dep[hash % (sizeof(ruleids_by_dep)/sizeof(*ruleids_by_dep))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, ruleid_by_dep_entry_cmp_asym, NULL, depidx);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct ruleid_by_dep_entry, node);
  }
  return NULL;
}

size_t ruleid_by_dep_entry_cnt;

struct ruleid_by_dep_entry *ensure_ruleid_by_dep(size_t depidx)
{
  uint32_t hash = abce_murmur32(0x12345678U, depidx);
  struct ruleid_by_dep_entry *e;
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  int ret;
  size_t i;

  head = &ruleids_by_dep[hash % (sizeof(ruleids_by_dep)/sizeof(*ruleids_by_dep))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, ruleid_by_dep_entry_cmp_asym, NULL, depidx);
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
    abort();
  }
  linked_list_add_tail(&e->llnode, &ruleids_by_dep_list);
  return e;
}

size_t one_ruleid_by_dep_entry_cnt;

void ins_ruleid_by_dep(size_t depidx, int ruleid)
{
  struct ruleid_by_dep_entry *e = ensure_ruleid_by_dep(depidx);
  uint32_t hash = abce_murmur32(0x12345678U, ruleid);
  struct one_ruleid_by_dep_entry *one;
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  int ret;
  head = &e->one_ruleid_by_dep[hash % (sizeof(e->one_ruleid_by_dep)/sizeof(*e->one_ruleid_by_dep))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, one_ruleid_by_dep_entry_cmp_asym, NULL, ruleid);
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
    abort();
  }
  return;
}

void better_cycle_detect_impl(int cur, unsigned char *no_cycles, unsigned char *parents)
{
  size_t i;
  struct linked_list_node *node;
  if (no_cycles[cur])
  {
    return;
  }
  if (parents[cur])
  {
    fprintf(stderr, "cycle found\n");
    for (size_t i = 0; i < rules_size; i++)
    {
      if (parents[i])
      {
        // FIXME print full rule info
        fprintf(stderr, " rule in cycle: %d\n", rules[i]->ruleid);
      }
    }
    exit(1);
  }
  parents[cur] = 1;
  LINKED_LIST_FOR_EACH(node, &rules[cur]->deplist)
  {
    struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
    int ruleid = get_ruleid_by_tgt(e->nameidx);
    if (ruleid >= 0)
    {
      better_cycle_detect_impl(ruleid, no_cycles, parents);
    }
  }
  parents[cur] = 0;
  no_cycles[cur] = 1;
}

unsigned char *better_cycle_detect(int cur)
{
  unsigned char *no_cycles, *parents;
  no_cycles = malloc(rules_size);
  parents = malloc(rules_size);

  memset(no_cycles, 0, rules_size);
  memset(parents, 0, rules_size);

  better_cycle_detect_impl(cur, no_cycles, parents);
  free(parents);
  return no_cycles;
}

struct add_dep {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  size_t depidx;
};

struct add_deps {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  size_t tgtidx;
  struct abce_rb_tree_nocmp add_deps[ADD_DEP_SIZE];
  struct linked_list_head add_deplist;
  unsigned phony:1;
};

struct abce_rb_tree_nocmp add_deps[ADD_DEPS_SIZE];

struct linked_list_head add_deplist = STIR_LINKED_LIST_HEAD_INITER(add_deplist);

static inline int add_dep_cmp_asym(size_t str, struct abce_rb_tree_node *n2, void *ud)
{
  struct add_dep *e = ABCE_CONTAINER_OF(n2, struct add_dep, node);
  int ret;
  size_t str2;
  str2 = e->depidx;
  ret = sizecmp(str, str2);
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

static inline int add_deps_cmp_asym(size_t str, struct abce_rb_tree_node *n2, void *ud)
{
  struct add_deps *e = ABCE_CONTAINER_OF(n2, struct add_deps, node);
  int ret;
  size_t str2;
  str2 = e->tgtidx;
  ret = sizecmp(str, str2);
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

size_t add_dep_cnt;

struct add_dep *add_dep_ensure(struct add_deps *entry, size_t depidx)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur32(0x12345678U, depidx);
  hashloc = hashval % (sizeof(entry->add_deps)/sizeof(entry->add_deps));
  n = ABCE_RB_TREE_NOCMP_FIND(&entry->add_deps[hashloc], add_dep_cmp_asym, NULL, depidx);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct add_dep, node);
  }
  add_dep_cnt++;
  struct add_dep *entry2 = my_malloc(sizeof(struct add_dep));
  entry2->depidx = depidx;
  if (abce_rb_tree_nocmp_insert_nonexist(&entry->add_deps[hashloc], add_dep_cmp_sym, NULL, &entry2->node) != 0)
  {
    printf("7\n");
    abort();
  }
  linked_list_add_tail(&entry2->llnode, &entry->add_deplist);
  return entry2;
}

size_t add_deps_cnt;

struct add_deps *add_deps_ensure(size_t tgtidx)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  size_t i;
  hashval = abce_murmur32(0x12345678U, tgtidx);
  hashloc = hashval % (sizeof(add_deps)/sizeof(add_deps));
  n = ABCE_RB_TREE_NOCMP_FIND(&add_deps[hashloc], add_deps_cmp_asym, NULL, tgtidx);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct add_deps, node);
  }
  add_deps_cnt++;
  struct add_deps *entry = my_malloc(sizeof(struct add_deps));
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
    abort();
  }
  linked_list_add_tail(&entry->llnode, &add_deplist);
  return entry;
}

void add_dep(char **tgts, size_t tgts_sz,
             char **deps, size_t deps_sz,
             int phony)
{
  size_t i, j;
  for (i = 0; i < tgts_sz; i++)
  {
    struct add_deps *entry = add_deps_ensure(stringtab_add(tgts[i]));
    if (phony)
    {
      entry->phony = 1;
    }
    for (j = 0; j < deps_sz; j++)
    {
      add_dep_ensure(entry, stringtab_add(deps[j]));
    }
  }
}

void zero_rule(struct rule *rule)
{
  memset(rule, 0, sizeof(*rule));
  linked_list_head_init(&rule->deplist);
  linked_list_head_init(&rule->tgtlist);
  rule->deps_remain_cnt = 0;
  //linked_list_head_init(&rule->depremainlist);
}

char **null_cmds[] = {NULL};

char **argdup(char **cmdargs)
{
  size_t cnt = 0;
  size_t i;
  char **result;
  while (cmdargs[cnt] != NULL)
  {
    cnt++;
  }
  result = my_malloc((cnt+1) * sizeof(*result));
  for (i = 0; i < cnt; i++)
  {
    result[i] = my_strdup(cmdargs[i]);
  }
  result[cnt] = NULL;
  return result;
}

char ***argsdupcnt(char ***cmdargs, size_t cnt)
{
  size_t i;
  char ***result;
  result = my_malloc((cnt+1) * sizeof(*result));
  for (i = 0; i < cnt; i++)
  {
    result[i] = cmdargs[i] ? argdup(cmdargs[i]) : NULL;
  }
  result[cnt] = NULL;
  return result;
}

size_t rule_cnt;

void process_additional_deps(void)
{
  struct linked_list_node *node, *node2;
  LINKED_LIST_FOR_EACH(node, &add_deplist)
  {
    struct add_deps *entry = ABCE_CONTAINER_OF(node, struct add_deps, llnode);
    int ruleid = get_ruleid_by_tgt(entry->tgtidx);
    struct rule *rule;
    if (ruleid < 0)
    {
      if (rules_size >= rules_capacity)
      {
        size_t new_capacity = 2*rules_capacity + 16;
        rules = realloc(rules, new_capacity * sizeof(*rules));
        rules_capacity = new_capacity;
      }
      rule_cnt++;
      rule = my_malloc(sizeof(*rule));
      rules[rules_size] = rule;
      //rule = &rules[rules_size];
      //printf("adding tgt: %s\n", entry->tgt);
      zero_rule(rule);
      rule->cmd.args = argsdupcnt(null_cmds, 1);

      rule->ruleid = rules_size++;
      ins_ruleid_by_tgt(entry->tgtidx, rule->ruleid);
      ins_tgt(rule, entry->tgtidx);
      LINKED_LIST_FOR_EACH(node2, &entry->add_deplist)
      {
        struct add_dep *dep = ABCE_CONTAINER_OF(node2, struct add_dep, llnode);
        ins_dep(rule, dep->depidx, 0);
      }
      rule->is_phony = !!entry->phony;
      LINKED_LIST_FOR_EACH(node2, &rule->deplist)
      {
        struct stirdep *dep = ABCE_CONTAINER_OF(node2, struct stirdep, llnode);
        ins_ruleid_by_dep(dep->nameidx, rule->ruleid);
        //printf(" dep: %s\n", dep->name);
      }
      continue;
    }
    rule = rules[ruleid];
    if (entry->phony)
    {
      rule->is_phony = 1;
    }
    LINKED_LIST_FOR_EACH(node2, &entry->add_deplist)
    {
      struct add_dep *dep = ABCE_CONTAINER_OF(node2, struct add_dep, llnode);
      ins_dep(rule, dep->depidx, 0);
    }
    LINKED_LIST_FOR_EACH(node2, &rule->deplist)
    {
      struct stirdep *dep = ABCE_CONTAINER_OF(node2, struct stirdep, llnode);
      ins_ruleid_by_dep(dep->nameidx, rule->ruleid);
      //printf(" dep: %s\n", dep->name);
    }
  }
}

void add_rule(char **tgts, size_t tgtsz,
              struct dep *deps, size_t depsz,
              char ***cmdargs, size_t cmdargsz, int phony)
{
  struct rule *rule;
  struct cmd cmd;
  size_t i;

  if (tgtsz <= 0)
  {
    printf("9\n");
    abort();
  }
  if (phony && tgtsz != 1)
  {
    printf("10\n");
    abort();
  }
  cmd.args = argsdupcnt(cmdargs, cmdargsz);
  if (rules_size >= rules_capacity)
  {
    size_t new_capacity = 2*rules_capacity + 16;
    rules = realloc(rules, new_capacity * sizeof(*rules));
    rules_capacity = new_capacity;
  }
  rule_cnt++;
  rule = my_malloc(sizeof(*rule));
  rules[rules_size] = rule;

  zero_rule(rule);
  rule->ruleid = rules_size++;
  rule->cmd = cmd;
  rule->is_phony = !!phony;

  for (i = 0; i < tgtsz; i++)
  {
    size_t tgtidx = stringtab_add(tgts[i]);
    ins_tgt(rule, tgtidx);
    ins_ruleid_by_tgt(tgtidx, rule->ruleid);
  }
  for (i = 0; i < depsz; i++)
  {
    size_t nameidx = stringtab_add(deps[i].name);
    ins_dep(rule, nameidx, !!deps[i].rec);
    ins_ruleid_by_dep(nameidx, rule->ruleid);
  }
}

int *ruleids_to_run;
size_t ruleids_to_run_size;
size_t ruleids_to_run_capacity;

struct ruleid_by_pid {
  struct abce_rb_tree_node node;
  pid_t pid;
  int ruleid;
};

static inline int ruleid_by_pid_cmp_asym(pid_t pid, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_pid *e = ABCE_CONTAINER_OF(n2, struct ruleid_by_pid, node);
  if (pid > e->pid)
  {
    return 1;
  }
  if (pid < e->pid)
  {
    return -1;
  }
  return 0;
}

static inline int ruleid_by_pid_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_pid *e1 = ABCE_CONTAINER_OF(n1, struct ruleid_by_pid, node);
  struct ruleid_by_pid *e2 = ABCE_CONTAINER_OF(n2, struct ruleid_by_pid, node);
  if (e1->pid > e2->pid)
  {
    return 1;
  }
  if (e1->pid < e2->pid)
  {
    return -1;
  }
  return 0;
}

struct abce_rb_tree_nocmp ruleid_by_pid[RULEID_BY_PID_SIZE];

int ruleid_by_pid_erase(pid_t pid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  int ruleid;
  hashval = abce_murmur32(0x12345678U, pid);
  hashloc = hashval % (sizeof(ruleid_by_pid)/sizeof(*ruleid_by_pid));
  n = ABCE_RB_TREE_NOCMP_FIND(&ruleid_by_pid[hashloc], ruleid_by_pid_cmp_asym, NULL, pid);
  if (n == NULL)
  {
    return -ENOENT;
  }
  struct ruleid_by_pid *bypid = ABCE_CONTAINER_OF(n, struct ruleid_by_pid, node);
  abce_rb_tree_nocmp_delete(&ruleid_by_pid[hashloc], &bypid->node);
  ruleid = bypid->ruleid;
  my_free(bypid);
  return ruleid;
}

//std::unordered_map<pid_t, int> ruleid_by_pid;

size_t ruleid_by_pid_cnt;

void print_cmd(char **argiter_orig)
{
  size_t argcnt = 0;
  struct iovec *iovs;
  char **argiter = argiter_orig;
  size_t i;
  while (*argiter != NULL)
  {
    argiter++;
    argcnt++;
  }
  iovs = malloc(sizeof(*iovs)*argcnt*2);
  for (i = 0; i < argcnt; i++)
  {
    iovs[2*i+0].iov_base = argiter_orig[i];
    iovs[2*i+0].iov_len = strlen(argiter_orig[i]);
    iovs[2*i+1].iov_base = " ";
    iovs[2*i+1].iov_len = 1;
  }
  iovs[2*argcnt-1].iov_base = "\n";
  iovs[2*argcnt-1].iov_len = 1;
  writev(1, iovs, 2*argcnt);
}

void child_execvp_wait(const char *cmd, char **args)
{
  pid_t pid = fork();
  if (pid < 0)
  {
    //printf("11\n");
    _exit(1);
  }
  else if (pid == 0)
  {
#if 0 // Already closed
    close(self_pipe_fd[0]);
    close(self_pipe_fd[1]);
#endif
    // FIXME check for make
    close(jobserver_fd[0]);
    close(jobserver_fd[1]);
    print_cmd(args);
    execvp(cmd, args);
    //write(1, "Err\n", 4);
    _exit(1);
  }
  else
  {
    int wstatus;
    pid_t ret;
    do {
      ret = waitpid(pid, &wstatus, 0);
    } while (ret == -1 && errno == -EINTR);
    if (ret == -1)
    {
      _exit(1);
    }
    if (!WIFEXITED(wstatus))
    {
      _exit(1);
    }
    if (WEXITSTATUS(wstatus) != 0)
    {
      _exit(1);
    }
    return;
  }
}

pid_t fork_child(int ruleid)
{
  char ***args;
  pid_t pid;
  struct cmd cmd = rules[ruleid]->cmd;
  char ***argiter;
  char **oneargiter;
  size_t argcnt = 0;

  args = cmd.args;
  argiter = args;

  printf("start args:\n");
  while (*argiter)
  {
    oneargiter = *argiter++;
    printf(" ");
    while (*oneargiter)
    {
      printf(" %s", *oneargiter++);
    }
    printf("\n");
    argcnt++;
  }
  printf("end args\n");

  argiter = args;

  if (argcnt == 0)
  {
    printf("no arguments\n");
    abort();
  }

  pid = fork();
  if (pid < 0)
  {
    printf("11\n");
    abort();
  }
  else if (pid == 0)
  {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL; // SIG_IGN does not allow waitpid()
    sigaction(SIGCHLD, &sa, NULL);
    close(self_pipe_fd[0]);
    close(self_pipe_fd[1]);
    update_recursive_pid(0);
    while (argcnt > 1)
    {
      child_execvp_wait((*argiter)[0], &(*argiter)[0]);
      argiter++;
      argcnt--;
    }
    update_recursive_pid(1);
    // FIXME check for make
    close(jobserver_fd[0]);
    close(jobserver_fd[1]);
    print_cmd(&(*argiter)[0]);
    execvp((*argiter)[0], &(*argiter)[0]);
    //write(1, "Err\n", 4);
    _exit(1);
  }
  else
  {
    ruleid_by_pid_cnt++;
    struct ruleid_by_pid *bypid = my_malloc(sizeof(*bypid));
    uint32_t hashval;
    size_t hashloc;
    bypid->pid = pid;
    bypid->ruleid = ruleid;
    children++;
    hashval = abce_murmur32(0x12345678U, pid);
    hashloc = hashval % (sizeof(ruleid_by_pid)/sizeof(*ruleid_by_pid));
    if (abce_rb_tree_nocmp_insert_nonexist(&ruleid_by_pid[hashloc], ruleid_by_pid_cmp_sym, NULL, &bypid->node) != 0)
    {
      printf("12\n");
      abort();
    }
    return pid;
  }
}

void mark_executed(int ruleid);

struct timespec rec_mtim(const char *name)
{
  struct timespec max;
  struct stat statbuf;
  DIR *dir = opendir(name);
  //printf("Statting %s\n", name);
  if (stat(name, &statbuf) != 0)
  {
    printf("Can't open file %s\n", name);
    exit(1);
  }
  max = statbuf.st_mtim;
  if (lstat(name, &statbuf) != 0)
  {
    printf("Can't open file %s\n", name);
    exit(1);
  }
  if (ts_cmp(statbuf.st_mtim, max) > 0)
  {
    max = statbuf.st_mtim;
  }
  if (dir == NULL)
  {
    printf("Can't open dir %s\n", name);
    exit(1);
  }
  for (;;)
  {
    struct dirent *de = readdir(dir);
    struct timespec cur;
    char nam2[PATH_MAX + 1] = {0}; // RFE avoid large static recursive allocs?
    //std::string nam2(name);
    if (snprintf(nam2, sizeof(nam2), "%s", name) >= sizeof(nam2))
    {
      printf("13\n");
      abort();
    }
    if (de == NULL)
    {
      break;
    }
    if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
    {
      continue;
    }
    size_t oldlen = strlen(nam2);
    if (snprintf(nam2+oldlen, sizeof(nam2)-oldlen,
                 "/%s", de->d_name) >= sizeof(nam2)-oldlen)
    {
      printf("14\n");
      abort();
    }
    //if (de->d_type == DT_DIR)
    if (0)
    {
      cur = rec_mtim(nam2);
    }
    else
    {
      if (stat(nam2, &statbuf) != 0)
      {
        printf("Can't open file %s\n", nam2);
        exit(1);
      }
      cur = statbuf.st_mtim;
      if (lstat(nam2, &statbuf) != 0)
      {
        printf("Can't open file %s\n", nam2);
        exit(1);
      }
      if (ts_cmp(statbuf.st_mtim, cur) > 0)
      {
        cur = statbuf.st_mtim;
      }
    }
    if (ts_cmp(cur, max) > 0)
    {
      //printf("nam2 file new %s\n", nam2);
      max = cur;
    }
    if ((statbuf.st_mode & S_IFMT) == S_IFDIR)
    {
      cur = rec_mtim(nam2);
      if (ts_cmp(cur, max) > 0)
      {
        //printf("nam2 dir new %s\n", nam2);
        max = cur;
      }
    }
  }
  closedir(dir);
  return max;
}

void do_exec(int ruleid)
{
  struct rule *r = rules[ruleid];
  //Rule &r = rules.at(ruleid);
  if (debug)
  {
    printf("do_exec %d\n", ruleid);
  }
  if (!r->is_queued)
  {
    int has_to_exec = 0;
    if (!r->is_phony && !linked_list_is_empty(&r->deplist))
    {
      int seen_nonphony = 0;
      int seen_tgt = 0;
      struct timespec st_mtim = {}, st_mtimtgt = {};
      struct linked_list_node *node;
      LINKED_LIST_FOR_EACH(node, &r->deplist)
      {
        struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
        struct stat statbuf;
        int depid = get_ruleid_by_tgt(e->nameidx);
        if (depid >= 0)
        {
          if (rules[depid]->is_phony)
          {
            has_to_exec = 1;
            continue;
          }
        }
        if (e->is_recursive)
        {
          struct timespec st_rectim = rec_mtim(sttable[e->nameidx]);
          if (!seen_nonphony || ts_cmp(st_rectim, st_mtim) > 0)
          {
            st_mtim = st_rectim;
          }
          seen_nonphony = 1;
          continue;
        }
        if (stat(sttable[e->nameidx], &statbuf) != 0)
        {
          has_to_exec = 1;
          break;
          //perror("can't stat");
          //fprintf(stderr, "file was: %s\n", it->c_str());
          //abort();
        }
        if (!seen_nonphony || ts_cmp(statbuf.st_mtim, st_mtim) > 0)
        {
          st_mtim = statbuf.st_mtim;
        }
        seen_nonphony = 1;
        if (lstat(sttable[e->nameidx], &statbuf) != 0)
        {
          has_to_exec = 1;
          break;
          //perror("can't lstat");
          //fprintf(stderr, "file was: %s\n", it->c_str());
          //abort();
        }
        if (!seen_nonphony || ts_cmp(statbuf.st_mtim, st_mtim) > 0)
        {
          st_mtim = statbuf.st_mtim;
        }
        seen_nonphony = 1;
      }
      LINKED_LIST_FOR_EACH(node, &r->tgtlist)
      {
        struct tgt *e = ABCE_CONTAINER_OF(node, struct tgt, llnode);
        struct stat statbuf;
        if (stat(sttable[e->tgtidx], &statbuf) != 0)
        {
          has_to_exec = 1;
          break;
        }
        if (!seen_tgt || ts_cmp(statbuf.st_mtim, st_mtimtgt) < 0)
        {
          st_mtimtgt = statbuf.st_mtim;
        }
        seen_tgt = 1;
      }
      if (!has_to_exec)
      {
        if (!seen_tgt)
        {
          printf("15\n");
          abort();
        }
        if (seen_nonphony && ts_cmp(st_mtimtgt, st_mtim) < 0)
        {
          has_to_exec = 1;
        }
      }
    }
    else if (r->is_phony)
    {
      has_to_exec = 1;
    }
    if (has_to_exec && r->cmd.args[0] != NULL)
    {
      if (debug)
      {
        printf("do_exec: has_to_exec %d\n", ruleid);
      }
      if (ruleids_to_run_size >= ruleids_to_run_capacity)
      {
        size_t new_capacity = 2*ruleids_to_run_capacity + 16;
        ruleids_to_run = realloc(ruleids_to_run, new_capacity*sizeof(*ruleids_to_run));
        ruleids_to_run_capacity = new_capacity;
      }
      ruleids_to_run[ruleids_to_run_size++] = ruleid;
      r->is_queued = 1;
    }
    else
    {
      if (debug)
      {
        printf("do_exec: mark_executed %d\n", ruleid);
      }
      r->is_queued = 1;
      mark_executed(ruleid);
    }
  }
}

void consider(int ruleid)
{
  struct rule *r = rules[ruleid];
  struct linked_list_node *node;
  int toexecute = 0;
  if (debug)
  {
    printf("considering %d\n", r->ruleid); // FIXME better output
  }
  if (r->is_executed)
  {
    if (debug)
    {
      printf("already execed %d\n", r->ruleid); // FIXME better output
    }
    return;
  }
  if (r->is_executing)
  {
    if (debug)
    {
      printf("already execing %d\n", r->ruleid); // FIXME better output
    }
    return;
  }
  r->is_executing = 1;
  LINKED_LIST_FOR_EACH(node, &r->deplist)
  {
    struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
    int idbytgt = get_ruleid_by_tgt(e->nameidx);
    if (idbytgt >= 0)
    {
      consider(idbytgt);
      if (!rules[idbytgt]->is_executed)
      {
        if (debug)
        {
          printf("rule %d not executed, executing rule %d\n", idbytgt, ruleid);
          //std::cout << "rule " << ruleid_by_tgt[it->name] << " not executed, executing rule " << ruleid << std::endl;
        }
        toexecute = 1;
      }
    }
    else
    {
      if (debug)
      {
        printf("ruleid by target %s not found\n", sttable[e->nameidx]);
      }
    }
  }
/*
  if (r.phony)
  {
    r.executed = true;
    break;
  }
*/
  if (!toexecute && !r->is_queued)
  {
    do_exec(ruleid);
    //ruleids_to_run.push_back(ruleid);
    //r.queued = true;
  }
/*
  ruleids_to_run.push_back(ruleid);
  r.executed = true;
*/
}

void reconsider(int ruleid, int ruleid_executed)
{
  struct rule *r = rules[ruleid];
  int toexecute = 0;
  if (debug)
  {
    printf("reconsidering %d\n", r->ruleid); // FIXME better output
  }
  if (r->is_executed)
  {
    if (debug)
    {
      printf("already execed %d\n", r->ruleid); // FIXME better output
    }
    return;
  }
  if (!r->is_executing)
  {
    if (debug)
    {
      printf("rule not executing %d\n", r->ruleid); // FIXME better output
    }
    return;
  }
  deps_remain_erase(r, ruleid_executed);
  //if (!linked_list_is_empty(&r->depremainlist))
  if (r->deps_remain_cnt > 0)
  {
    toexecute = 1;
  }
  if (!toexecute && !r->is_queued)
  {
    do_exec(ruleid);
    //ruleids_to_run.push_back(ruleid);
    //r.queued = true;
  }
}

void mark_executed(int ruleid)
{
  struct rule *r = rules[ruleid];
  struct linked_list_node *node, *node2;
  if (r->is_executed)
  {
    printf("16\n");
    abort();
  }
  if (!r->is_executing)
  {
    printf("17\n");
    abort();
  }
  r->is_executed = 1;
  if (ruleid == 0)
  {
    return;
  }
  LINKED_LIST_FOR_EACH(node, &r->tgtlist)
  {
    struct tgt *e = ABCE_CONTAINER_OF(node, struct tgt, llnode);
    struct ruleid_by_dep_entry *e2 = find_ruleids_by_dep(e->tgtidx);
    LINKED_LIST_FOR_EACH(node2, &e2->one_ruleid_by_deplist)
    {
      struct one_ruleid_by_dep_entry *one =
        ABCE_CONTAINER_OF(node2, struct one_ruleid_by_dep_entry, llnode);
      reconsider(one->ruleid, ruleid);
    }
  }
}

/*
.PHONY: all

all: l1g.txt

l1g.txt: l2a.txt l2b.txt
	./touchs1 l1g.txt

l2a.txt l2b.txt: l3c.txt l3d.txt l3e.txt
	./touchs1 l2a.txt l2b.txt

l3c.txt: l4f.txt
	./touchs1 l3c.txt

l3d.txt: l4f.txt
	./touchs1 l3d.txt

l3e.txt: l4f.txt
	./touchs1 l3e.txt
 */

void set_nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0)
  {
    printf("18\n");
    abort();
  }
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0)
  {
    printf("19\n");
    abort();
  }
}

void sigchld_handler(int x)
{
  write(self_pipe_fd[1], ".", 1);
}

char *myitoa(int i)
{
  char *res = my_malloc(16);
  snprintf(res, 16, "%d", i);
  return res;
}

void pathological_test(void)
{
  int rule;
  char *v_rules[3000];
  size_t v_rules_sz = 0;
  struct timeval tv1, tv2;
  char *rulestr;
  for (rule = 0; rule < 3000; rule++)
  {
    rulestr = myitoa(rule);
    add_dep(&rulestr, 1, v_rules, v_rules_sz, 0);
    v_rules[v_rules_sz++] = rulestr;
  }
  process_additional_deps();
  printf("starting DFS2\n");
  gettimeofday(&tv1, NULL);
  free(better_cycle_detect(get_ruleid_by_tgt(stringtab_add(rulestr))));
  gettimeofday(&tv2, NULL);
  double ms = (tv2.tv_usec - tv1.tv_usec + 1e6*(tv2.tv_sec - tv1.tv_sec)) / 1e3;
  printf("ending DFS2 in %g ms\n", ms);
  exit(0);
}

void stack_conf(void)
{
  const rlim_t stackSize = 16 * 1024 * 1024;
  struct rlimit rl;
  int result;
  result = getrlimit(RLIMIT_STACK, &rl);
  if (result != 0)
  {
    printf("20\n");
    abort();
  }
  if (rl.rlim_cur < stackSize)
  {
    rl.rlim_cur = stackSize;
    result = setrlimit(RLIMIT_STACK, &rl);
    if (result != 0)
    {
      printf("21\n");
      abort();
    }
  }
}


#if 0
size_t stiryy_add_fun_sym(struct stiryy *stiryy, const char *symbol, int maybe, size_t loc)
{
  // FIXME implement in C!
#if 0
  size_t old = (size_t)-1;
  memblock &mb = scope_stack.back();
  if (mb.type != memblock::T_SC)
  {
    abort();
  }
  scope *sc = mb.u.sc;
  std::string str(symbol);
  if (sc->vars.find(str) != sc->vars.end()
      //&& sc->vars[str].type == memblock::T_F
     )
  {
    old = st.addNonString(sc->vars[str]);
    if (maybe)
    {
      return old;
    }
    //old = sc->vars[str].u.d;
  }
  sc->vars[str] = memblock(loc, true);
  return old;
#endif
}
#endif

void version(char *argv0)
{
  fprintf(stderr, "Stirmake 0.1, Copyright (C) 2017-19 Aalto University, 2018 Juha-Matti Tilli\n");
  fprintf(stderr, "Logo (C) 2019 Juha-Matti Tilli, All right reserved, license not applicable\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Authors:\n");
  fprintf(stderr, "- Juha-Matti Tilli (copyright transfer to Aalto on 19.4.2018 for some code)\n");
  fprintf(stderr, "\n");
  fprintf(stderr,
"Permission is hereby granted, free of charge, to any person obtaining a copy\n"
"of this software and associated documentation files (the \"Software\"), to\n"
"deal in the Software without restriction, including without limitation the\n"
"rights to use, copy, modify, merge, publish, distribute, sublicense, and/or\n"
"sell copies of the Software, and to permit persons to whom the Software is\n"
"furnished to do so, subject to the following conditions:\n"
"\n"
"The above copyright notice and this permission notice shall be included in\n"
"all copies or substantial portions of the Software.\n"
"\n"
"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING\n"
"FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS\n"
"IN THE SOFTWARE.\n");
  exit(0);
}

void usage(char *argv0)
{
  fprintf(stderr, "Usage:\n");
  if (isspecprog)
  {
    fprintf(stderr, "%s [-v] [-f Stirfile]\n", argv0);
    fprintf(stderr, "  You can start %s as smka, smkt or smkp or use main command stirmake\n", argv0);
    fprintf(stderr, "  smka, smkt and smkp do not take -t | -p | -a whereas stirmake takes\n");
  }
  else
  {
    fprintf(stderr, "%s [-v] [-f Stirfile] -t | -p | -a\n", argv0);
    fprintf(stderr, "  You can start %s as smka, smkt or smkp or use main command %s\n", argv0, argv0);
    fprintf(stderr, "  smka, smkt and smkp do not take -t | -p | -a whereas %s takes\n", argv0);
  }
  exit(1);
}

void do_narration(void)
{
  wchar_t aumlautch = L'\xe4';
  wchar_t oumlautch = L'\xf6';
  char *aumlaut = malloc(MB_CUR_MAX+1);
  char *oumlaut = malloc(MB_CUR_MAX+1);
  int ret;
  ret = wctomb(aumlaut, aumlautch);
  if (ret < 0)
  {
    aumlaut[0] = 'a';
    aumlaut[1] = '\0';
  }
  else
  {
    aumlaut[ret] = '\0';
  }
  ret = wctomb(oumlaut, oumlautch);
  if (ret < 0)
  {
    oumlaut[0] = 'o';
    oumlaut[1] = '\0';
  }
  else
  {
    oumlaut[ret] = '\0';
  }
  printf("Hellurei, hellurei, k%s%snt%s on hurjaa!\n", aumlaut, aumlaut, oumlaut);
  free(aumlaut);
  free(oumlaut);
}

void recursion_misuse_prevention(void)
{
  char *pidstr = getenv("STIRMAKEPID");
  if (pidstr != NULL)
  {
    pid_t pid = (int)atoi(pidstr);
    if (getppid() == pid)
    {
      fprintf(stderr, "Recursion misuse detected. Stirmake is designed to be used non-recursively.\n");
      exit(1);
    }
  }
  update_recursive_pid(0);
}

void set_mode(enum mode newmode, int newspecprog, char *argv0)
{
  if (mode != MODE_NONE && mode != newmode)
  {
    fprintf(stderr, "Ambiguous mode\n");
    usage(argv0);
  }
  mode = newmode;
  if (newspecprog)
  {
    isspecprog = 1;
  }
}

int main(int argc, char **argv)
{
#if 0
  pathological_test();
#endif
  FILE *f;
  struct abce abce = {};
  struct stiryy_main main = {.abce = &abce};
  struct stiryy stiryy = {};
  size_t i;
  int opt;
  const char *filename = "Stirfile";
  uint32_t forkedchildcnt = 0;
  int narration = 0;

  char *dupargv0 = strdup(argv[0]);
  char *basenm = basename(dupargv0);

  if (strcmp(basenm, "smka") == 0)
  {
    set_mode(MODE_ALL, 1, argv[0]);
  }
  else if (strcmp(basenm, "smkt") == 0)
  {
    set_mode(MODE_THIS, 1, argv[0]);
  }
  else if (strcmp(basenm, "smkp") == 0)
  {
    set_mode(MODE_PROJECT, 1, argv[0]);
  }

  while ((opt = getopt(argc, argv, "vf:Htpa")) != -1)
  {
    switch (opt)
    {
    case 'v':
      version(argv[0]);
    case 'H':
      narration = 1;
      setlocale(LC_CTYPE, "");
      break;
    case 'f':
      filename = optarg;
      break;
    case 't':
      set_mode(MODE_THIS, 0, argv[0]);
      break;
    case 'p':
      set_mode(MODE_PROJECT, 0, argv[0]);
      break;
    case 'a':
      set_mode(MODE_ALL, 0, argv[0]);
      break;
    default:
    case '?':
      usage(argv[0]);
    }
  }

  if (mode == MODE_NONE)
  {
    usage(argv[0]);
  }

  recursion_misuse_prevention();

  abce_init(&abce);
  stiryy_init(&stiryy, &main, ".", abce.dynscope);

  f = fopen(filename, "r");
  if (!f)
  {
    printf("24\n");
    abort();
  }
  stiryydoparse(f, &stiryy);
  fclose(f);

  stack_conf();

  for (i = 0; i < main.rulesz; i++)
  {
    if (main.rules[i].targetsz > 0) // FIXME chg to if (1)
    {
      if (debug)
      {
        printf("ADDING RULE\n");
      }
      add_rule(main.rules[i].targets, main.rules[i].targetsz,
               main.rules[i].deps, main.rules[i].depsz,
               main.rules[i].shells, main.rules[i].shellsz, 0);
    }
  }

  for (i = 0; i < stiryy.cdepincludesz; i++)
  {
    struct incyy incyy = {};
    size_t j;
    f = fopen(stiryy.cdepincludes[i], "r");
    if (!f)
    {
      printf("25\n");
      abort();
    }
    incyydoparse(f, &incyy);
    //for (auto it = incyy.rules; it != incyy.rules + incyy.rulesz; it++)
    for (j = 0; j < incyy.rulesz; j++)
    {
      //std::vector<std::string> tgt;
      //std::vector<std::string> dep;
      //std::copy(it->deps, it->deps+it->depsz, std::back_inserter(dep));
      //std::copy(it->targets, it->targets+it->targetsz, std::back_inserter(tgt));
      add_dep(incyy.rules[j].targets, incyy.rules[j].targetsz,
              incyy.rules[j].deps, incyy.rules[j].depsz,
              0);
      //add_dep(tgt, dep, 0);
    }
    fclose(f);
    incyy_free(&incyy);
  }
  stiryy_free(&stiryy);

  //add_dep(v_l3e, v_l1g, 0); // offending rule

  process_additional_deps();

  unsigned char *no_cycles = better_cycle_detect(0);
  for (i = 0; i < rules_size; i++)
  {
    calc_deps_remain(rules[i]);
  }

  // Delete unreachable rules from ruleids_by_dep
#if 1
  struct linked_list_node *node, *tmp;
  LINKED_LIST_FOR_EACH_SAFE(node, tmp, &ruleids_by_dep_list)
  {
    struct ruleid_by_dep_entry *entry =
      ABCE_CONTAINER_OF(node, struct ruleid_by_dep_entry, llnode);
    int bytgt = get_ruleid_by_tgt(entry->depidx);
    if (bytgt < 0)
    {
      continue;
      printf("26\n");
      abort();
    }
    if (no_cycles[bytgt])
    {
      struct linked_list_node *node2, *tmp2;
      LINKED_LIST_FOR_EACH_SAFE(node2, tmp2, &entry->one_ruleid_by_deplist)
      {
        struct one_ruleid_by_dep_entry *one =
          ABCE_CONTAINER_OF(node2, struct one_ruleid_by_dep_entry, llnode);
        if (no_cycles[one->ruleid])
        {
          continue;
        }
        uint32_t hashval;
        size_t hashloc;
        hashval = abce_murmur32(0x12345678U, one->ruleid);
        hashloc = hashval % (sizeof(entry->one_ruleid_by_dep)/sizeof(*entry->one_ruleid_by_dep));
        abce_rb_tree_nocmp_delete(&entry->one_ruleid_by_dep[hashloc], &one->node);
        linked_list_delete(&one->llnode);
        my_free(one);
      }
    }
    else
    {
      uint32_t hashval;
      size_t hashloc;
      hashval = abce_murmur32(0x12345678U, entry->depidx);
      hashloc = hashval % (sizeof(ruleids_by_dep)/sizeof(*ruleids_by_dep));
      abce_rb_tree_nocmp_delete(&ruleids_by_dep[hashloc], &entry->node);
      linked_list_delete(&entry->llnode);
      my_free(entry);
    }
  }
  LINKED_LIST_FOR_EACH_SAFE(node, tmp, &ruleid_by_tgt_list)
  {
    struct ruleid_by_tgt_entry *entry =
      ABCE_CONTAINER_OF(node, struct ruleid_by_tgt_entry, llnode);
    if (no_cycles[entry->ruleid])
    {
      continue;
    }
    uint32_t hashval;
    size_t hashloc;
    hashval = abce_murmur32(0x12345678U, entry->tgtidx);
    hashloc = hashval % (sizeof(ruleid_by_tgt)/sizeof(*ruleid_by_tgt));
    abce_rb_tree_nocmp_delete(&ruleid_by_tgt[hashloc], &entry->node);
    linked_list_delete(&entry->llnode);
    my_free(entry);
  }
#endif

  if (pipe(self_pipe_fd) != 0)
  {
    printf("27\n");
    abort();
  }
  set_nonblock(self_pipe_fd[0]);
  set_nonblock(self_pipe_fd[1]);
  if (pipe(jobserver_fd) != 0)
  {
    printf("28\n");
    abort();
  }
  set_nonblock(jobserver_fd[0]);
  set_nonblock(jobserver_fd[1]);

  for (int i = 0; i < limit - 1; i++)
  {
    write(jobserver_fd[1], ".", 1);
  }

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = sigchld_handler;
  sigaction(SIGCHLD, &sa, NULL);

  consider(0);

  while (ruleids_to_run_size > 0)
  {
    if (children)
    {
      char ch;
      if (read(jobserver_fd[0], &ch, 1) != 1)
      {
        break;
      }
    }
    printf("forking1 child\n");
    fork_child(ruleids_to_run[ruleids_to_run_size-1]);
    ruleids_to_run_size--;
  }

/*
  while (children < limit && !ruleids_to_run.empty())
  {
    std::cout << "forking1 child" << std::endl;
    fork_child(ruleids_to_run.back());
    ruleids_to_run.pop_back();
  }
*/

  int maxfd = 0;
  if (self_pipe_fd[0] > maxfd)
  {
    maxfd = self_pipe_fd[0];
  }
  if (jobserver_fd[0] > maxfd)
  {
    maxfd = jobserver_fd[0];
  }
  char chbuf[100];
  while (children > 0)
  {
    int wstatus = 0;
    fd_set readfds;
    FD_SET(self_pipe_fd[0], &readfds);
    if (ruleids_to_run_size > 0)
    {
      FD_SET(jobserver_fd[0], &readfds);
    }
    select(maxfd+1, &readfds, NULL, NULL, NULL);
    printf("SELECT RETURNED\n");
    if (read(self_pipe_fd[0], chbuf, 100))
    {
      for (;;)
      {
        pid_t pid;
        pid = waitpid(-1, &wstatus, WNOHANG);
        if (pid == 0)
        {
          break;
        }
        if (wstatus != 0 && pid > 0)
        {
          if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
          {
            printf("error exit status\n");
            printf("%d\n", (int)WIFEXITED(wstatus));
            printf("%d\n", (int)WIFSIGNALED(wstatus));
            printf("%d\n", (int)WEXITSTATUS(wstatus));
            return 1;
          }
        }
        if (children <= 0 && ruleids_to_run_size == 0)
        {
          if (pid < 0 && errno == ECHILD)
          {
            if (rules[0]->is_executed)
            {
              return 0;
            }
            else
            {
              fprintf(stderr, "can't execute rule 0\n");
              abort();
            }
          }
          printf("29\n");
          abort();
        }
        if (pid < 0)
        {
          if (errno == ECHILD)
          {
            break;
          }
          printf("30\n");
          abort();
        }
        int ruleid = ruleid_by_pid_erase(pid);
        if (ruleid < 0)
        {
          printf("31\n");
          abort();
        }
        mark_executed(ruleid);
        children--;
        if (children != 0)
        {
          write(jobserver_fd[1], ".", 1);
        }
        forkedchildcnt++;
        if (((forkedchildcnt % 10) == 0) && narration)
        {
          do_narration();
        }
      }
    }
    while (ruleids_to_run_size > 0)
    {
      if (children)
      {
        char ch;
        if (read(jobserver_fd[0], &ch, 1) != 1)
        {
          break;
        }
      }
      printf("forking child\n");
      //std::cout << "forking child" << std::endl;
      fork_child(ruleids_to_run[ruleids_to_run_size-1]);
      ruleids_to_run_size--;
      //ruleids_to_run.pop_back();
    }
  }
  if (debug)
  {
    printf("\n");
    printf("Memory use statistics:\n");
    printf("  stringtab: %zu\n", stringtab_cnt);
    printf("  ruleid_by_tgt_entry: %zu\n", ruleid_by_tgt_entry_cnt);
    printf("  tgt: %zu\n", tgt_cnt);
    printf("  stirdep: %zu\n", stirdep_cnt);
    printf("  dep_remain: %zu\n", dep_remain_cnt);
    printf("  ruleid_by_dep_entry: %zu\n", ruleid_by_dep_entry_cnt);
    printf("  one_ruleid_by_dep_entry: %zu\n", one_ruleid_by_dep_entry_cnt);
    printf("  add_dep: %zu\n", add_dep_cnt);
    printf("  add_deps: %zu\n", add_deps_cnt);
    printf("  rule: %zu\n", rule_cnt);
    printf("  ruleid_by_pid: %zu\n", ruleid_by_pid_cnt);
  }
  return 0;
}
