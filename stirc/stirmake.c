#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
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


#define STIR_LINKED_LIST_HEAD_INITER(x) { \
  .node = { \
    .prev = &(x).node, \
    .next = &(x).node, \
  }, \
}


int debug = 1;

int self_pipe_fd[2];

int jobserver_fd[2];

struct ruleid_by_tgt_entry {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  int ruleid;
  char *tgt;
};

struct abce_rb_tree_nocmp ruleid_by_tgt[8192];
struct linked_list_head ruleid_by_tgt_list =
  STIR_LINKED_LIST_HEAD_INITER(ruleid_by_tgt_list);

static inline int ruleid_by_tgt_entry_cmp_asym(char *str, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_tgt_entry *e = ABCE_CONTAINER_OF(n2, struct ruleid_by_tgt_entry, node);
  size_t len1 = strlen(str);
  size_t len2, lenmin;
  int ret;
  char *str2;
  len2 = strlen(e->tgt);
  str2 = e->tgt;
  lenmin = (len1 < len2) ? len1 : len2;
  ret = memcmp(str, str2, lenmin);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 > len2)
  {
    return 1;
  }
  if (len1 < len2)
  {
    return -1;
  }
  return 0;
}
static inline int ruleid_by_tgt_entry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_tgt_entry *e1 = ABCE_CONTAINER_OF(n1, struct ruleid_by_tgt_entry, node);
  struct ruleid_by_tgt_entry *e2 = ABCE_CONTAINER_OF(n2, struct ruleid_by_tgt_entry, node);
  size_t len1 = strlen(e1->tgt);
  size_t len2, lenmin;
  int ret;
  len2 = strlen(e2->tgt);
  lenmin = (len1 < len2) ? len1 : len2;
  ret = memcmp(e1->tgt, e2->tgt, lenmin);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 > len2)
  {
    return 1;
  }
  if (len1 < len2)
  {
    return -1;
  }
  return 0;
}

void ins_ruleid_by_tgt(char *tgt, int ruleid)
{
  uint32_t hash = abce_murmur_buf(0x12345678U, tgt, strlen(tgt));
  struct ruleid_by_tgt_entry *e;
  struct abce_rb_tree_nocmp *head;
  int ret;
  e = malloc(sizeof(*e));
  e->tgt = tgt;
  e->ruleid = ruleid;
  head = &ruleid_by_tgt[hash % (sizeof(ruleid_by_tgt)/sizeof(*ruleid_by_tgt))];
  ret = abce_rb_tree_nocmp_insert_nonexist(head, ruleid_by_tgt_entry_cmp_sym, NULL, &e->node);
  if (ret != 0)
  {
    abort();
  }
  linked_list_add_tail(&e->llnode, &ruleid_by_tgt_list);
}

int get_ruleid_by_tgt(char *tgt)
{
  uint32_t hash = abce_murmur_buf(0x12345678U, tgt, strlen(tgt));
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
  char **args;
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
  char *name;
  unsigned is_recursive:1;
};

struct dep_remain {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
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
  char *tgt;
};

struct rule {
  unsigned is_phony:1;
  unsigned is_executed:1;
  unsigned is_executing:1;
  unsigned is_queued:1;
  struct cmd cmd;
  int ruleid;
  struct abce_rb_tree_nocmp tgts[64];
  struct linked_list_head tgtlist;
  struct abce_rb_tree_nocmp deps[64];
  struct linked_list_head deplist;
  struct abce_rb_tree_nocmp deps_remain[64];
  struct linked_list_head depremainlist;
  //size_t deps_remain_cnt; // XXX return this for less memory use?
};

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
  linked_list_delete(&dep_remain->llnode);
  free(dep_remain);
}

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
  struct dep_remain *dep_remain = malloc(sizeof(struct dep_remain));
  dep_remain->ruleid = ruleid;
  if (abce_rb_tree_nocmp_insert_nonexist(&rule->deps_remain[hashloc], dep_remain_cmp_sym, NULL, &dep_remain->node) != 0)
  {
    abort();
  }
  linked_list_add_tail(&dep_remain->llnode, &rule->depremainlist);
  //rule->deps_remain_cnt++;
}

void calc_deps_remain(struct rule *rule)
{
  size_t i;
  struct linked_list_node *node;
  LINKED_LIST_FOR_EACH(node, &rule->deplist)
  {
    struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
    char *depname = e->name;
    int ruleid = get_ruleid_by_tgt(depname);
    if (ruleid >= 0)
    {
      deps_remain_insert(rule, ruleid);
    }
  }
#if 0
  for (i = 0; i < rule->deps_size; i++) // FIXME linked list
  {
    char *depname = rule->deps[i].name;
    int ruleid = get_ruleid_by_tgt(depname);
    if (ruleid >= 0)
    {
      deps_remain_insert(rule, ruleid);
    }
  }
#endif
}

#if 0
class Rule {
  public:
    bool phony;
    bool executed;
    bool executing;
    bool queued;
    std::unordered_set<std::string> tgts;
    std::unordered_set<Dep> deps;
    std::unordered_set<int> deps_remain;
    Cmd cmd;
    int ruleid;

    Rule(): phony(false), executed(false), executing(false), queued(false) {}

    void calc_deps_remain(void)
    {
      for (auto it = deps.begin(); it != deps.end(); it++)
      {
        if (ruleid_by_tgt.find(it->name) != ruleid_by_tgt.end())
        {
          deps_remain.insert(ruleid_by_tgt[it->name]);
        }
      }
    }
};
#endif

#if 0
std::ostream &operator<<(std::ostream &o, const Rule &r)
{
  bool first = true;
  o << "Rule(";
  o << r.ruleid;
  o << ",[";
  for (auto it = r.tgts.begin(); it != r.tgts.end(); it++)
  {
    if (first)
    {
      first = false;
    }
    else
    {
      o << ",";
    }
    o << *it;
  }
  first = true;
  o << "],[";
  for (auto it = r.deps.begin(); it != r.deps.end(); it++)
  {
    if (first)
    {
      first = false;
    }
    else
    {
      o << ",";
    }
    o << it->name;
  }
  o << "])";
  return o;
}
#endif

int children = 0;
const int limit = 2;

struct rule *rules;
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
  char *dep;
  struct abce_rb_tree_nocmp one_ruleid_by_dep[64];
  struct linked_list_head one_ruleid_by_deplist;
#if 0
  int *ruleids;
  size_t ruleids_capacity;
  size_t ruleids_size;
#endif
};

static inline int ruleid_by_dep_entry_cmp_asym(char *str, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_dep_entry *e = ABCE_CONTAINER_OF(n2, struct ruleid_by_dep_entry, node);
  size_t len1 = strlen(str);
  size_t len2, lenmin;
  int ret;
  char *str2;
  len2 = strlen(e->dep);
  str2 = e->dep;
  lenmin = (len1 < len2) ? len1 : len2;
  ret = memcmp(str, str2, lenmin);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 > len2)
  {
    return 1;
  }
  if (len1 < len2)
  {
    return -1;
  }
  return 0;
}
static inline int ruleid_by_dep_entry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_dep_entry *e1 = ABCE_CONTAINER_OF(n1, struct ruleid_by_dep_entry, node);
  struct ruleid_by_dep_entry *e2 = ABCE_CONTAINER_OF(n2, struct ruleid_by_dep_entry, node);
  size_t len2, lenmin;
  int ret;
  size_t len1;
  
  len1 = strlen(e1->dep);
  len2 = strlen(e2->dep);
  lenmin = (len1 < len2) ? len1 : len2;
  ret = memcmp(e1->dep, e2->dep, lenmin);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 > len2)
  {
    return 1;
  }
  if (len1 < len2)
  {
    return -1;
  }
  return 0;
}

struct abce_rb_tree_nocmp ruleids_by_dep[8192];
struct linked_list_head ruleids_by_dep_list =
  STIR_LINKED_LIST_HEAD_INITER(ruleids_by_dep_list);

struct ruleid_by_dep_entry *find_ruleids_by_dep(char *dep)
{
  uint32_t hash = abce_murmur_buf(0x12345678U, dep, strlen(dep));
  struct ruleid_by_dep_entry *e;
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  int ret;
  size_t i;

  head = &ruleids_by_dep[hash % (sizeof(ruleids_by_dep)/sizeof(*ruleids_by_dep))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, ruleid_by_dep_entry_cmp_asym, NULL, dep);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct ruleid_by_dep_entry, node);
  }
  return NULL;
}

struct ruleid_by_dep_entry *ensure_ruleid_by_dep(char *dep)
{
  uint32_t hash = abce_murmur_buf(0x12345678U, dep, strlen(dep));
  struct ruleid_by_dep_entry *e;
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  int ret;
  size_t i;

  head = &ruleids_by_dep[hash % (sizeof(ruleids_by_dep)/sizeof(*ruleids_by_dep))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, ruleid_by_dep_entry_cmp_asym, NULL, dep);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct ruleid_by_dep_entry, node);
  }
  
  e = malloc(sizeof(*e));
  e->dep = dep;
  for (i = 0; i < sizeof(e->one_ruleid_by_dep)/sizeof(*e->one_ruleid_by_dep); i++)
  {
    abce_rb_tree_nocmp_init(&e->one_ruleid_by_dep[i]);
  }
  linked_list_head_init(&e->one_ruleid_by_deplist);

  ret = abce_rb_tree_nocmp_insert_nonexist(head, ruleid_by_dep_entry_cmp_sym, NULL, &e->node);
  if (ret != 0)
  {
    abort();
  }
  linked_list_add_tail(&e->llnode, &ruleids_by_dep_list);
  return e;
}

void ins_ruleid_by_dep(char *dep, int ruleid)
{
  struct ruleid_by_dep_entry *e = ensure_ruleid_by_dep(dep);
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
  
  one = malloc(sizeof(*one));
  one->ruleid = ruleid;
  linked_list_add_tail(&one->llnode, &e->one_ruleid_by_deplist);

  ret = abce_rb_tree_nocmp_insert_nonexist(head, one_ruleid_by_dep_entry_cmp_sym, NULL, &one->node);
  if (ret != 0)
  {
    abort();
  }
  return;
}

#if 0
std::vector<Rule> rules;

std::unordered_map<std::string, std::unordered_set<int>> ruleids_by_dep;
#endif


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
        fprintf(stderr, " rule in cycle: %d\n", rules[i].ruleid);
      }
    }
    exit(1);
  }
  parents[cur] = 1;
#if 0
  for (i = 0; i < rules[cur].deps_size; i++)
  {
    int ruleid = get_ruleid_by_tgt(rules[cur].deps[i].name);
    if (ruleid >= 0)
    {
      better_cycle_detect_impl(ruleid, parents, no_cycles);
    }
  }
#endif
  LINKED_LIST_FOR_EACH(node, &rules[cur].deplist)
  {
    struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
    char *depname = e->name;
    int ruleid = get_ruleid_by_tgt(depname);
    if (ruleid >= 0)
    {
      better_cycle_detect_impl(ruleid, parents, no_cycles);
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
  better_cycle_detect_impl(cur, parents, no_cycles);
  free(parents);
  return no_cycles;
}

struct add_dep {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  char *dep;
};

struct add_deps {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  char *tgt;
  struct abce_rb_tree_nocmp add_deps[64];
  struct linked_list_head add_deplist;
  unsigned phony:1;
};

struct abce_rb_tree_nocmp add_deps[8192];

struct linked_list_head add_deplist = STIR_LINKED_LIST_HEAD_INITER(add_deplist);

static inline int add_dep_cmp_asym(const char *str, struct abce_rb_tree_node *n2, void *ud)
{
  struct add_dep *e = ABCE_CONTAINER_OF(n2, struct add_dep, node);
  size_t len1 = strlen(str);
  size_t len2, lenmin;
  int ret;
  char *str2;
  len2 = strlen(e->dep);
  str2 = e->dep;
  lenmin = (len1 < len2) ? len1 : len2;
  ret = memcmp(str, str2, lenmin);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 > len2)
  {
    return 1;
  }
  if (len1 < len2)
  {
    return -1;
  }
  return 0;
}
static inline int add_dep_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct add_dep *e1 = ABCE_CONTAINER_OF(n1, struct add_dep, node);
  struct add_dep *e2 = ABCE_CONTAINER_OF(n2, struct add_dep, node);
  size_t len2, lenmin;
  int ret;
  size_t len1;
  
  len1 = strlen(e1->dep);
  len2 = strlen(e2->dep);
  lenmin = (len1 < len2) ? len1 : len2;
  ret = memcmp(e1->dep, e2->dep, lenmin);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 > len2)
  {
    return 1;
  }
  if (len1 < len2)
  {
    return -1;
  }
  return 0;
}

static inline int add_deps_cmp_asym(const char *str, struct abce_rb_tree_node *n2, void *ud)
{
  struct add_deps *e = ABCE_CONTAINER_OF(n2, struct add_deps, node);
  size_t len1 = strlen(str);
  size_t len2, lenmin;
  int ret;
  char *str2;
  len2 = strlen(e->tgt);
  str2 = e->tgt;
  lenmin = (len1 < len2) ? len1 : len2;
  ret = memcmp(str, str2, lenmin);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 > len2)
  {
    return 1;
  }
  if (len1 < len2)
  {
    return -1;
  }
  return 0;
}
static inline int add_deps_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct add_deps *e1 = ABCE_CONTAINER_OF(n1, struct add_deps, node);
  struct add_deps *e2 = ABCE_CONTAINER_OF(n2, struct add_deps, node);
  size_t len2, lenmin;
  int ret;
  size_t len1;
  
  len1 = strlen(e1->tgt);
  len2 = strlen(e2->tgt);
  lenmin = (len1 < len2) ? len1 : len2;
  ret = memcmp(e1->tgt, e2->tgt, lenmin);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 > len2)
  {
    return 1;
  }
  if (len1 < len2)
  {
    return -1;
  }
  return 0;
}

struct add_dep *add_dep_ensure(struct add_deps *entry, const char *dep)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur_buf(0x12345678U, dep, strlen(dep));
  hashloc = hashval % (sizeof(entry->add_deps)/sizeof(entry->add_deps));
  n = ABCE_RB_TREE_NOCMP_FIND(&entry->add_deps[hashloc], add_dep_cmp_asym, NULL, dep);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct add_dep, node);
  }
  struct add_dep *entry2 = malloc(sizeof(struct add_dep));
  entry2->dep = strdup(dep);
  if (abce_rb_tree_nocmp_insert_nonexist(&entry->add_deps[hashloc], add_dep_cmp_sym, NULL, &entry2->node) != 0)
  {
    abort();
  }
  linked_list_add_tail(&entry2->llnode, &entry->add_deplist);
  return entry2;
}

struct add_deps *add_deps_ensure(const char *tgt)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  size_t i;
  hashval = abce_murmur_buf(0x12345678U, tgt, strlen(tgt));
  hashloc = hashval % (sizeof(add_deps)/sizeof(add_deps));
  n = ABCE_RB_TREE_NOCMP_FIND(&add_deps[hashloc], add_deps_cmp_asym, NULL, tgt);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct add_deps, node);
  }
  struct add_deps *entry = malloc(sizeof(struct add_deps));
  entry->tgt = strdup(tgt);
  entry->phony = 0;
  for (i = 0; i < sizeof(entry->add_deps)/sizeof(*entry->add_deps); i++)
  {
    abce_rb_tree_nocmp_init(&entry->add_deps[i]);
  }
  linked_list_head_init(&entry->add_deplist);
  if (abce_rb_tree_nocmp_insert_nonexist(&add_deps[hashloc], add_deps_cmp_sym, NULL, &entry->node) != 0)
  {
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
    struct add_deps *entry = add_deps_ensure(tgts[i]);
    if (phony)
    {
      entry->phony = 1;
    }
    for (j = 0; j < deps_sz; j++)
    {
      add_dep_ensure(entry, deps[j]);
    }
  }
#if 0
  for (auto tgt = tgts.begin(); tgt != tgts.end(); tgt++)
  {
    if (add_deps.find(*tgt) == add_deps.end())
    {
      add_deps[*tgt] = std::make_pair(false, std::unordered_set<std::string>());
    }
    if (phony)
    {
      add_deps[*tgt].first = true;
    }
    add_deps[*tgt].second.rehash(deps.size());
    for (auto dep = deps.begin(); dep != deps.end(); dep++)
    {
      add_deps[*tgt].second.insert(*dep);
    }
  }
#endif
}

void process_additional_deps(void)
{
  struct linked_list_node *node, *node2;
  LINKED_LIST_FOR_EACH(node, &add_deplist)
  {
    struct add_deps *entry = ABCE_CONTAINER_OF(node, struct add_deps, llnode);
    int ruleid = get_ruleid_by_tgt(entry->tgt);
    struct rule *rule;
    if (ruleid < 0)
    {
      if (rules_size >= rules_capacity)
      {
        size_t new_capacity = 2*rules_capacity + 16;
        rules = realloc(rules, new_capacity * sizeof(*rules));
        rules_capacity = new_capacity;
      }
      rule = &rules[rules_size];
      //printf("adding tgt: %s\n", entry->tgt);
      memset(rule, 0, sizeof(*rule));
      rule->ruleid = rules_size++;
      ins_ruleid_by_tgt(entry->tgt, rule->ruleid);
      // FIXME ins_tgt
      // FIXME ins_deps
      rule->is_phony = entry->phony;
      LINKED_LIST_FOR_EACH(node2, &rule->deplist)
      {
        struct stirdep *dep = ABCE_CONTAINER_OF(node2, struct stirdep, llnode);
        ins_ruleid_by_dep(dep->name, rule->ruleid);
        //printf(" dep: %s\n", dep->name);
      }
      continue;
    }
    rule = &rules[ruleid];
    if (entry->phony)
    {
      rule->is_phony = 1;
    }
    // FIXME ins_deps
    LINKED_LIST_FOR_EACH(node2, &rule->deplist)
    {
      struct stirdep *dep = ABCE_CONTAINER_OF(node2, struct stirdep, llnode);
      ins_ruleid_by_dep(dep->name, rule->ruleid);
      //printf(" dep: %s\n", dep->name);
    }
  }
#if 0
  for (auto it = add_deps.begin(); it != add_deps.end(); it++)
  {
    if (ruleid_by_tgt.find(it->first) == ruleid_by_tgt.end())
    {
      Rule r;
      r.ruleid = rules.size();
      //std::cout << "adding tgt " << it->first << std::endl;
      ruleid_by_tgt[it->first] = r.ruleid;
      r.tgts.insert(it->first);
      r.deps.rehash(r.deps.size() + it->second.second.size());
      std::copy(it->second.second.begin(), it->second.second.end(),
                std::inserter(r.deps, r.deps.begin()));
      r.phony = it->second.first;
      for (auto it2 = r.deps.begin(); it2 != r.deps.end(); it2++)
      {
        if (ruleids_by_dep.find(it2->name) == ruleids_by_dep.end())
        {
          ruleids_by_dep[it2->name] = std::unordered_set<int>();
        }
        ruleids_by_dep[it2->name].insert(r.ruleid);
        //std::cout << " dep: " << *it2 << std::endl;
      }
      rules.push_back(r);
      //std::cout << "added Rule: " << r << std::endl;
      continue;
    }
    Rule &r = rules[ruleid_by_tgt[it->first]];
    if (it->second.first)
    {
      r.phony = true;
    }
    //std::cout << "modifying rule " << r << std::endl;
    std::copy(it->second.second.begin(), it->second.second.end(),
              std::inserter(r.deps, r.deps.begin()));
    //std::cout << "modified rule " << r << std::endl;
    for (auto it2 = r.deps.begin(); it2 != r.deps.end(); it2++)
    {
      if (ruleids_by_dep.find(it2->name) == ruleids_by_dep.end())
      {
        ruleids_by_dep[it2->name] = std::unordered_set<int>();
      }
      ruleids_by_dep[it2->name].insert(r.ruleid);
    }
  }
#endif
}

void add_rule(char **tgts, size_t tgtsz,
              struct dep *deps, size_t depsz,
              char **cmdargs, int phony)
{
  struct rule *rule;
  struct cmd cmd;
  if (tgtsz <= 0)
  {
    abort();
  }
  if (phony && tgtsz != 1)
  {
    abort();
  }
  cmd.args = cmdargs;
  if (rules_size >= rules_capacity)
  {
    size_t new_capacity = 2*rules_capacity + 16;
    rules = realloc(rules, new_capacity * sizeof(*rules));
    rules_capacity = new_capacity;
  }
  rule = &rules[rules_size];
  memset(rule, 0, sizeof(*rule));
  rule->ruleid = rules_size++;
  rule->cmd = cmd;
  rule->is_phony = !!phony;
  // FIXME add tgts, deps
}

#if 0
void add_rule(const std::vector<std::string> &tgts,
              const std::vector<Dep> &deps,
              const std::vector<std::string> &cmdargs,
              bool phony)
{
  Rule r;
  Cmd c(cmdargs);
  r.phony = phony;
  if (tgts.size() <= 0)
  {
    abort();
  }
  if (phony && tgts.size() != 1)
  {
    abort();
  }
  r.tgts.rehash(tgts.size());
  r.deps.rehash(deps.size());
  std::copy(tgts.begin(), tgts.end(), std::inserter(r.tgts, r.tgts.begin()));
  std::copy(deps.begin(), deps.end(), std::inserter(r.deps, r.deps.begin()));
  //r.tgts = tgts;
  //r.deps = deps;
  r.cmd = c;
  r.ruleid = rules.size();
  rules.push_back(r);
  for (auto it = tgts.begin(); it != tgts.end(); it++)
  {
    if (ruleid_by_tgt.find(*it) != ruleid_by_tgt.end())
    {
      std::cerr << "duplicate rule" << std::endl;
      exit(1);
    }
    ruleid_by_tgt[*it] = r.ruleid;
  }
  for (auto it = deps.begin(); it != deps.end(); it++)
  {
    if (ruleids_by_dep.find(it->name) == ruleids_by_dep.end())
    {
      ruleids_by_dep[it->name] = std::unordered_set<int>();
    }
    ruleids_by_dep[it->name].insert(r.ruleid);
  }
}
#endif

//std::vector<int> ruleids_to_run;
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

struct abce_rb_tree_nocmp ruleid_by_pid[64];

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
  free(bypid);
  return ruleid;
}

//std::unordered_map<pid_t, int> ruleid_by_pid;

pid_t fork_child(int ruleid)
{
  char **args;
  pid_t pid;
  struct cmd cmd = rules[ruleid].cmd;
  args = cmd.args;

  pid = fork();
  if (pid < 0)
  {
    abort();
  }
  else if (pid == 0)
  {
    close(self_pipe_fd[0]);
    close(self_pipe_fd[1]);
    // FIXME check for make
    close(jobserver_fd[0]);
    close(jobserver_fd[1]);
    execvp(args[0], &args[0]);
    //write(1, "Err\n", 4);
    _exit(1);
  }
  else
  {
    struct ruleid_by_pid *bypid = malloc(sizeof(*bypid));
    uint32_t hashval;
    size_t hashloc;
    bypid->pid = pid;
    bypid->ruleid = ruleid;
    children++;
    hashval = abce_murmur32(0x12345678U, pid);
    hashloc = hashval % (sizeof(ruleid_by_pid)/sizeof(*ruleid_by_pid));
    if (abce_rb_tree_nocmp_insert_nonexist(&ruleid_by_pid[hashloc], ruleid_by_pid_cmp_sym, NULL, &bypid->node) != 0)
    {
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
  struct rule *r = &rules[ruleid];
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
        int depid = get_ruleid_by_tgt(e->name);
        if (depid >= 0)
        {
          if (rules[depid].is_phony)
          {
            has_to_exec = 1;
            continue;
          }
        }
        if (e->is_recursive)
        {
          struct timespec st_rectim = rec_mtim(e->name);
          if (!seen_nonphony || ts_cmp(st_rectim, st_mtim) > 0)
          {
            st_mtim = st_rectim;
          }
          seen_nonphony = 1;
          continue;
        }
        if (stat(e->name, &statbuf) != 0)
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
        if (lstat(e->name, &statbuf) != 0)
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
        if (stat(e->tgt, &statbuf) != 0)
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
#if 0
      for (auto it = r.deps.begin(); it != r.deps.end(); it++)
      {
        struct stat statbuf;
        //std::cout << "it " << *it << std::endl;
        if (ruleid_by_tgt.find(it->name) != ruleid_by_tgt.end())
        {
          //std::cout << "ruleid by tgt: " << *it << std::endl;
          //std::cout << "ruleid by tgt- " << ruleid_by_tgt[*it] << std::endl;
          if (rules.at(ruleid_by_tgt[it->name]).phony)
          {
            has_to_exec = 1;
            //std::cout << "phony" << std::endl;
            continue;
          }
          //std::cout << "nonphony" << std::endl;
        }
        if (it->recursive)
        {
          struct timespec st_rectim = rec_mtim(it->name.c_str());
          if (!seen_nonphony || ts_cmp(st_rectim, st_mtim) > 0)
          {
            st_mtim = st_rectim;
          }
          seen_nonphony = 1;
          continue;
        }
        if (stat(it->name.c_str(), &statbuf) != 0)
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
        if (lstat(it->name.c_str(), &statbuf) != 0)
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
      for (auto it = r.tgts.begin(); it != r.tgts.end(); it++)
      {
        struct stat statbuf;
        if (stat(it->c_str(), &statbuf) != 0)
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
#endif
      if (!has_to_exec)
      {
        if (!seen_tgt)
        {
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
  struct rule *r = &rules[ruleid];
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
    int idbytgt = get_ruleid_by_tgt(e->name);
    if (idbytgt >= 0)
    {
      consider(idbytgt);
      if (!rules[idbytgt].is_executed)
      {
        if (debug)
        {
          printf("rule %d not executed, executing rule %d\n", idbytgt, ruleid);
          //std::cout << "rule " << ruleid_by_tgt[it->name] << " not executed, executing rule " << ruleid << std::endl;
        }
        toexecute = 1;
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
  struct rule *r = &rules[ruleid];
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
  if (!linked_list_is_empty(&r->depremainlist))
  {
    toexecute = 1;
  }
#if 0
  for (auto it = r.deps.begin(); it != r.deps.end(); it++)
  {
    int dep = ruleid_by_tgt[*it];
    if (!rules.at(dep).executed)
    {
      if (debug)
      {
        std::cout << "rule " << ruleid_by_tgt[*it] << " not executed, executing rule " << ruleid << std::endl;
      }
      toexecute = 1;
      break;
    }
  }
#endif
  if (!toexecute && !r->is_queued)
  {
    do_exec(ruleid);
    //ruleids_to_run.push_back(ruleid);
    //r.queued = true;
  }
}

void mark_executed(int ruleid)
{
  struct rule *r = &rules[ruleid];
  struct linked_list_node *node, *node2;
  if (r->is_executed)
  {
    abort();
  }
  if (!r->is_executing)
  {
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
    struct ruleid_by_dep_entry *e2 = find_ruleids_by_dep(e->tgt);
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
    abort();
  }
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0)
  {
    abort();
  }
}

void sigchld_handler(int x)
{
  write(self_pipe_fd[1], ".", 1);
}

char *myitoa(int i)
{
  char *res = malloc(16);
  snprintf(res, 16, "%d", i);
  return res;
}

void pathological_test(void)
{
  int rule;
  //std::vector<std::string> v_rules;
  char *v_rules[3000];
  size_t v_rules_sz = 0;
  struct timeval tv1, tv2;
  char *rulestr;
  //std::string rulestr;
  //std::ofstream mf("Makefile");
  //mf << "all: d2999" << std::endl;
  for (rule = 0; rule < 3000; rule++)
  {
    rulestr = myitoa(rule);
    add_dep(&rulestr, 1, v_rules, v_rules_sz, 0);
#if 0
    mf << "d" << rulestr << ": ";
    for (auto it = v_rules.begin(); it != v_rules.end(); it++)
      mf << "d" << *it << " ";
    mf << std::endl;
#endif
    v_rules[v_rules_sz++] = rulestr;
  }
  process_additional_deps();
  printf("starting DFS2\n");
  gettimeofday(&tv1, NULL);
  free(better_cycle_detect(get_ruleid_by_tgt(rulestr)));
  gettimeofday(&tv2, NULL);
  double ms = (tv2.tv_usec - tv1.tv_usec + 1e6*(tv2.tv_sec - tv1.tv_sec)) / 1e3;
  printf("ending DFS2 in %g ms\n", ms);
  //mf.close();
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
    abort();
  }
  if (rl.rlim_cur < stackSize)
  {
    rl.rlim_cur = stackSize;
    result = setrlimit(RLIMIT_STACK, &rl);
    if (result != 0)
    {
      abort();
    }
  }
}

struct stringtabentry {
  struct abce_rb_tree_node node;
  char *string;
  size_t idx;
};

static inline int stringtabentry_cmp_asym(const char *str, struct abce_rb_tree_node *n2, void *ud)
{
  struct stringtabentry *e = ABCE_CONTAINER_OF(n2, struct stringtabentry, node);
  size_t len1 = strlen(str);
  size_t len2, lenmin;
  int ret;
  char *str2;
  len2 = strlen(e->string);
  str2 = e->string;
  lenmin = (len1 < len2) ? len1 : len2;
  ret = memcmp(str, str2, lenmin);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 > len2)
  {
    return 1;
  }
  if (len1 < len2)
  {
    return -1;
  }
  return 0;
}
static inline int stringtabentry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct stringtabentry *e1 = ABCE_CONTAINER_OF(n1, struct stringtabentry, node);
  struct stringtabentry *e2 = ABCE_CONTAINER_OF(n2, struct stringtabentry, node);
  size_t len1 = strlen(e1->string);
  size_t len2, lenmin;
  int ret;
  len2 = strlen(e2->string);
  lenmin = (len1 < len2) ? len1 : len2;
  ret = memcmp(e1->string, e2->string, lenmin);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 > len2)
  {
    return 1;
  }
  if (len1 < len2)
  {
    return -1;
  }
  return 0;
}

struct abce_rb_tree_nocmp st[8192];
size_t st_cnt;

#if 0
std::vector<memblock> all_scopes; // FIXME needed?
std::vector<memblock> scope_stack;
#endif

#if 0
size_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen)
{
  std::string str(symbol, symlen);
  return st.add(str);
}
#endif

size_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  if (strlen(symbol) != symlen)
  {
    abort(); // RFE what to do?
  }
  hashval = abce_murmur_buf(0x12345678U, symbol, symlen);
  hashloc = hashval % (sizeof(st)/sizeof(*st));
  n = ABCE_RB_TREE_NOCMP_FIND(&st[hashloc], stringtabentry_cmp_asym, 
NULL, symbol);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct stringtabentry, node)->idx;
  }
  struct stringtabentry *stringtabentry = malloc(sizeof(struct stringtabentry));
  stringtabentry->string = strdup(symbol);
  stringtabentry->idx = st_cnt++;
  if (abce_rb_tree_nocmp_insert_nonexist(&st[hashloc], stringtabentry_cmp_sym, NULL, &stringtabentry->node) != 0)
  {
    abort();
  }
  return stringtabentry->idx;
}


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

char **null_cmds = {NULL};

int main(int argc, char **argv)
{
#if 0
  all_scopes.push_back(memblock(new scope()));
  scope_stack.push_back(all_scopes.back());
#endif

#if 0
  std::vector<std::string> v_all{"all"};
  std::vector<std::string> v_l1g{"l1g.txt"};
  std::vector<std::string> v_l2ab{"l2a.txt", "l2b.txt"};
  std::vector<std::string> v_l2a{"l2a.txt"};
  std::vector<std::string> v_l3cde{"l3c.txt", "l3d.txt", "l3e.txt"};
  std::vector<std::string> v_l3cd{"l3c.txt", "l3d.txt"};
  std::vector<std::string> v_l3c{"l3c.txt"};
  std::vector<std::string> v_l3d{"l3d.txt"};
  std::vector<std::string> v_l3e{"l3e.txt"};
  std::vector<std::string> v_l4f{"l4f.txt"};

  std::vector<std::string> argt_l2ab{"./touchs1", "l2a.txt", "l2b.txt"};
  std::vector<std::string> argt_l3c{"./touchs1", "l3c.txt"};
  std::vector<std::string> argt_l3d{"./touchs1", "l3d.txt"};
  std::vector<std::string> argt_l3e{"./touchs1", "l3e.txt"};
  std::vector<std::string> argt_l1g{"./touchs1", "l1g.txt"};
  std::vector<std::string> arge_all{"echo", "all"};

  pathological_test();

  add_rule(v_all, v_l1g, arge_all, 1);
  add_rule(v_l1g, v_l2ab, argt_l1g, 0);
  add_rule(v_l2ab, v_l3cd, argt_l2ab, 0);
  add_rule(v_l3c, v_l4f, argt_l3c, 0);
  add_rule(v_l3d, v_l4f, argt_l3d, 0);
  add_rule(v_l3e, v_l4f, argt_l3e, 0);
  add_dep(v_l2a, v_l3e, 0);
#endif
  FILE *f = fopen("Stirfile", "r");
  struct stiryy stiryy = {};
  size_t i;

  if (!f)
  {
    abort();
  }
  stiryydoparse(f, &stiryy);
  fclose(f);

#if 0
  std::vector<memblock> stack;

  stack.push_back(memblock(-1, false, true));
  stack.push_back(memblock(-1, false, true));
  size_t ip = scope_stack.back().u.sc->vars["MYFLAGS"].u.d + 9;
  std::cout << "to become ip: " << ip << std::endl;

  microprogram_global = stiryy.bytecode;
  microsz_global = stiryy.bytesz;
  st_global = &st;
  engine(scope_stack.back().u.sc->lua, scope_stack.back(),
         stack, ip);

  std::cout << "STACK SIZE: " << stack.size() << std::endl;
  std::cout << "DUMP: ";
  stack.back().dump();
  std::cout << std::endl;
  //exit(0);
#endif

  stack_conf();

  //ruleid_by_tgt.rehash(stiryy.rulesz);
  //for (auto it = stiryy.rules; it != stiryy.rules + stiryy.rulesz; it++)
  for (i = 0; i < stiryy.rulesz; i++)
  {
#if 0
    std::vector<std::string> tgt;
    std::vector<Dep> dep;
    std::vector<std::string> cmd;
    //std::copy(it->deps, it->deps+it->depsz, std::back_inserter(dep));
    for (auto it2 = it->deps; it2 != it->deps+it->depsz; it2++)
    {
      dep.push_back(Dep(it2->name, it2->rec));
    }
    std::copy(it->targets, it->targets+it->targetsz, std::back_inserter(tgt));
#endif
    if (stiryy.rules[i].targetsz > 0) // FIXME chg to if (1)
    {
      if (debug)
      {
        printf("ADDING RULE\n");
      }
      add_rule(stiryy.rules[i].targets, stiryy.rules[i].targetsz,
               stiryy.rules[i].deps, stiryy.rules[i].depsz,
               null_cmds, 0);
    }
  }

  for (i = 0; i < stiryy.cdepincludesz; i++)
  {
    struct incyy incyy = {};
    size_t j;
    f = fopen(stiryy.cdepincludes[i], "r");
    if (!f)
    {
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
    calc_deps_remain(&rules[i]);
  }

  // Delete unreachable rules from ruleids_by_dep
#if 1
  struct linked_list_node *node, *tmp;
  LINKED_LIST_FOR_EACH_SAFE(node, tmp, &ruleids_by_dep_list)
  {
    struct ruleid_by_dep_entry *entry =
      ABCE_CONTAINER_OF(node, struct ruleid_by_dep_entry, llnode);
    int bytgt = get_ruleid_by_tgt(entry->dep);
    if (bytgt < 0)
    {
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
        free(one);
      }
    }
    else
    {
      uint32_t hashval;
      size_t hashloc;
      hashval = abce_murmur_buf(0x12345678U, entry->dep, strlen(entry->dep));
      hashloc = hashval % (sizeof(ruleids_by_dep)/sizeof(*ruleids_by_dep));
      abce_rb_tree_nocmp_delete(&ruleids_by_dep[hashloc], &entry->node);
      linked_list_delete(&entry->llnode);
      free(entry);
    }
  }
#if 0
  for (auto it = ruleids_by_dep.begin(); it != ruleids_by_dep.end(); )
  {
    if (no_cycles[ruleid_by_tgt[it->first]])
    {
      for (auto it2 = it->second.begin(); it2 != it->second.end(); )
      {
        if (no_cycles[*it2])
        {
          it2++;
        }
        else
        {
          it2 = it->second.erase(it2);
        }
      }
      it->second.rehash(it->second.size());
      it++;
    }
    else
    {
      it = ruleids_by_dep.erase(it);
    }
  }
#endif
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
    hashval = abce_murmur_buf(0x12345678U, entry->tgt, strlen(entry->tgt));
    hashloc = hashval % (sizeof(ruleid_by_tgt)/sizeof(*ruleid_by_tgt));
    abce_rb_tree_nocmp_delete(&ruleid_by_tgt[hashloc], &entry->node);
    linked_list_delete(&entry->llnode);
    free(entry);
  }
#if 0
  // Delete unreachable rules from ruleid_by_tgt
  for (auto it = ruleid_by_tgt.begin(); it != ruleid_by_tgt.end(); )
  {
    if (no_cycles[it->second])
    {
      it++;
    }
    else
    {
      it = ruleid_by_tgt.erase(it);
    }
  }
  ruleid_by_tgt.rehash(ruleid_by_tgt.size());
#endif
#endif

  if (pipe(self_pipe_fd) != 0)
  {
    abort();
  }
  set_nonblock(self_pipe_fd[0]);
  set_nonblock(self_pipe_fd[1]);
  if (pipe(jobserver_fd) != 0)
  {
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
            if (rules[0].is_executed)
            {
              return 0;
            }
            else
            {
              fprintf(stderr, "can't execute rule 0\n");
              abort();
            }
          }
          abort();
        }
        if (pid < 0)
        {
          if (errno == ECHILD)
          {
            break;
          }
          abort();
        }
        int ruleid = ruleid_by_pid_erase(pid);
        if (ruleid < 0)
        {
          abort();
        }
#if 0
        int ruleid = ruleid_by_pid[pid];
        if (ruleid_by_pid.erase(pid) != 1)
        {
          abort();
        }
#endif
        mark_executed(ruleid);
        children--;
        if (children != 0)
        {
          write(jobserver_fd[1], ".", 1);
        }
      }
    }
#if 0
    while (children < limit && !ruleids_to_run.empty())
    {
      std::cout << "forking child" << std::endl;
      fork_child(ruleids_to_run.back());
      ruleids_to_run.pop_back();
    }
#endif
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
  return 0;
}
