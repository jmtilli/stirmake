#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
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
#include <poll.h>
#include "yyutils.h"
#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "incyyutils.h"
#include "dbyy.h"
#include "dbyyutils.h"

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
  RULES_REMAIN_SIZE = 64,
  DB_SIZE = 8192,
  STRINGTAB_SIZE = 8192,
  MAX_JOBCNT = 1000,
};


int debug = 0;

int self_pipe_fd[2];

int jobserver_fd[2];

struct stringtabentry {
  struct abce_rb_tree_node node;
  char *string;
  size_t idx;
};

int children = 0;

void merge_db(void);

// FIXME do updates to DB here as well... Now they are not done.
void errxit(const char *fmt, ...)
{
  va_list args;
  const char *prefix = "stirmake: *** ";
  const char *suffix = ". Exiting.\n";
  size_t sz = strlen(prefix) + strlen(fmt) + strlen(suffix) + 1;
  char *fmtdup = malloc(sz);
  int wstatus;
  pid_t pid;
  snprintf(fmtdup, sz, "%s%s%s", prefix, fmt, suffix);
  va_start(args, fmt);
  vfprintf(stderr, fmtdup, args);
  va_end(args);
  free(fmtdup);
  pid = waitpid(-1, &wstatus, WNOHANG);
  if (pid < 0 && errno == ECHILD)
  {
    merge_db();
    exit(1);
  }
  if (pid > 0)
  {
    if (children <= 0)
    {
      printf("27.E\n");
      abort();
    }
    children--;
    if (children != 0)
    {
      write(jobserver_fd[1], ".", 1);
    }
  }
  fprintf(stderr, "stirmake: *** Waiting for child processes to die.\n");
  for (;;)
  {
    pid = waitpid(-1, &wstatus, 0);
    if (pid == 0)
    {
      printf("28.E\n");
      abort();
    }
    if (children <= 0)
    {
      if (pid < 0 && errno == ECHILD)
      {
        fprintf(stderr, "stirmake: *** No children left. Exiting.\n");
        merge_db();
        exit(1);
      }
      printf("29.E\n");
      abort();
    }
    if (pid < 0)
    {
      printf("30.E\n");
      perror("Error was");
      printf("number of children: %d\n", children);
      abort();
    }
#if 0 // Let's not do this, just in case the data is messed up.
    int ruleid = ruleid_by_pid_erase(pid);
    if (ruleid < 0)
    {
      printf("31.E\n");
      abort();
    }
#endif
    children--;
    if (children != 0)
    {
      write(jobserver_fd[1], ".", 1);
    }
  }
}

struct cmd {
  char ***args;
};

int cmd_equal(struct cmd *cmd1, struct cmd *cmd2)
{
  size_t cnt1, cnt2, i;
  for (cnt1 = 0; cmd1->args[cnt1]; cnt1++);
  for (cnt2 = 0; cmd2->args[cnt2]; cnt2++);
  if (cnt1 != cnt2)
  {
    return 0;
  }
  for (i = 0; i < cnt1; i++)
  {
    size_t argcnt1, argcnt2, j;
    for (argcnt1 = 0; cmd1->args[i][argcnt1]; argcnt1++);
    for (argcnt2 = 0; cmd2->args[i][argcnt2]; argcnt2++);
    if (argcnt1 != argcnt2)
    {
      return 0;
    }
    for (j = 0; j < argcnt1; j++)
    {
      if (strcmp(cmd1->args[i][j], cmd2->args[i][j]) != 0)
      {
        return 0;
      }
    }
  }
  return 1;
}

struct dbe {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  size_t tgtidx; // key
  size_t diridx; // non-key
  struct cmd cmds; // non-key
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

static inline int dbe_cmp_asym(size_t str, struct abce_rb_tree_node *n2, void *ud)
{
  struct dbe *e = ABCE_CONTAINER_OF(n2, struct dbe, node);
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

struct db {
  struct abce_rb_tree_nocmp byname[DB_SIZE];
  struct linked_list_head ll;
};

struct db db = {};

void maybe_del_dbe(struct db *db, size_t tgtidx)
{
  uint32_t hash = abce_murmur32(0x12345678U, tgtidx);
  struct abce_rb_tree_node *n;
  struct abce_rb_tree_nocmp *head;
  head = &db->byname[hash % (sizeof(db->byname)/sizeof(*db->byname))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, dbe_cmp_asym, NULL, tgtidx);
  if (n == NULL)
  {
    return;
  }
  abce_rb_tree_nocmp_delete(head, n);
  linked_list_delete(&ABCE_CONTAINER_OF(n, struct dbe, node)->llnode);
}

void ins_dbe(struct db *db, struct dbe *dbe)
{
  uint32_t hash = abce_murmur32(0x12345678U, dbe->tgtidx);
  struct abce_rb_tree_nocmp *head;
  int ret;
  head = &db->byname[hash % (sizeof(db->byname)/sizeof(*db->byname))];
  ret = abce_rb_tree_nocmp_insert_nonexist(head, dbe_cmp_sym, NULL, &dbe->node);
  if (ret != 0)
  {
    struct abce_rb_tree_node *n;
    n = ABCE_RB_TREE_NOCMP_FIND(head, dbe_cmp_asym, NULL, dbe->tgtidx);
    if (n == NULL)
    {
      abort();
    }
    abce_rb_tree_nocmp_delete(head, n);
    linked_list_delete(&ABCE_CONTAINER_OF(n, struct dbe, node)->llnode);
    ret = abce_rb_tree_nocmp_insert_nonexist(head, dbe_cmp_sym, NULL, &dbe->node);
    if (ret != 0)
    {
      abort();
    }
  }
  linked_list_add_tail(&dbe->llnode, &db->ll);
}

void escape_string(FILE *f, const char *str)
{
  const char *ptr;
  for (ptr = str; *ptr; ptr++)
  {
    unsigned char uch = *ptr;
    if (uch < 0x20 || uch >= 0x7F)
    {
      fprintf(f, "\\x%.2X", uch);
      continue;
    }
    if (uch == '\\')
    {
      fprintf(f, "\\\\");
      continue;
    }
    if (uch == '\'')
    {
      fprintf(f, "\\'");
      continue;
    }
    if (uch == '"')
    {
      fprintf(f, "\\\"");
      continue;
    }
    if (uch == '\t')
    {
      fprintf(f, "\\t");
      continue;
    }
    if (uch == '\r')
    {
      fprintf(f, "\\r");
      continue;
    }
    if (uch == '\n')
    {
      fprintf(f, "\\n");
      continue;
    }
    fprintf(f, "%c", (char)uch);
  }
}

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

char *my_arena;
char *my_arena_ptr;
size_t sizeof_my_arena;

void *my_malloc(size_t sz)
{
  void *result = my_arena_ptr;
  my_arena_ptr += (sz+7)/8*8;
  if (my_arena_ptr >= my_arena + sizeof_my_arena)
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

int read_jobserver(void)
{
  char ch;
  struct pollfd pfd = {};
  int ret;
  pfd.fd = jobserver_fd[0];
  pfd.events = POLLIN;
  ret = poll(&pfd, 1, 0);
  if (ret == 0)
  {
    return 0;
  }
  if (ret < 0)
  {
    abort();
  }
  if (!(pfd.revents & POLLIN))
  {
    abort();
  }

  ret = recv(jobserver_fd[0], &ch, 1, MSG_DONTWAIT);
  if (ret == -1 && errno == ENOTSOCK)
  {
    //fprintf(stderr, "UGH\n");
    // Ugh. GNU make expects the jobserver to be blocking, which doesn't
    // really suit our architecture. Set an interval timer in case of a race
    // that GNU make won.
    struct itimerval new_value = {};
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_usec = 0;
    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_usec = 10*1000;
    setitimer(ITIMER_REAL, &new_value, NULL);
    ret = read(jobserver_fd[0], &ch, 1);
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_usec = 0;
    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &new_value, NULL);

    if (ret > 1)
    {
      abort();
    }
    return (ret == 1);
  }
  return (ret == 1);
}

struct abce_rb_tree_nocmp st[STRINGTAB_SIZE];
char *sttable[1048576]; // RFE make variable sized
size_t st_cnt;

size_t stringtab_cnt = 0;

size_t stringtab_get(const char *symbol)
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
  return (size_t)-1;
}

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

int cmdequal_db(struct db *db, size_t tgtidx, struct cmd *cmd, size_t diridx)
{
  uint32_t hash = abce_murmur32(0x12345678U, tgtidx);
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  struct dbe *dbe;
  head = &db->byname[hash % (sizeof(db->byname)/sizeof(*db->byname))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, dbe_cmp_asym, NULL, tgtidx);
  if (n == NULL)
  {
    if (debug)
    {
      printf("target %s not found in cmd DB\n", sttable[tgtidx]);
    }
    return 0;
  }
  dbe = ABCE_CONTAINER_OF(n, struct dbe, node);
  if (dbe->diridx != diridx)
  {
    if (debug)
    {
      printf("target %s has different dir in cmd DB\n", sttable[tgtidx]);
    }
    return 0;
  }
  if (!cmd_equal(cmd, &dbe->cmds))
  {
    if (debug)
    {
      printf("target %s has different cmd in cmd DB\n", sttable[tgtidx]);
    }
    return 0;
  }
  return 1;
}


struct ruleid_by_tgt_entry {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  int ruleid;
  //char *tgt;
  size_t tgtidx;
};

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
    printf("stirmake: *** ruleid by tgt %s already exists\n", sttable[tgtidx]);
    exit(1); // FIXME print (filename, linenumber) pair
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
  unsigned is_orderonly:1;
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

struct stirtgt {
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
  struct linked_list_node remainllnode;
  struct linked_list_node cleanllnode;
  unsigned is_phony:1;
  unsigned is_maybe:1;
  unsigned is_rectgt:1;
  unsigned is_executed:1;
  unsigned is_actually_executed:1; // command actually invoked
  unsigned is_executing:1;
  unsigned is_queued:1;
  unsigned is_cleanqueued:1;
  unsigned remain:1;
  unsigned st_mtim_valid:1;
  unsigned is_inc:1; // whether this is included from dependency file
  unsigned is_dist:1;
  unsigned is_forked:1;
  size_t diridx;
  struct cmd cmd;
  struct timespec st_mtim;
  int ruleid;
  struct abce_rb_tree_nocmp tgts[TGTS_SIZE];
  struct linked_list_head tgtlist;
  struct abce_rb_tree_nocmp deps[DEPS_SIZE];
  struct linked_list_head deplist;
  struct abce_rb_tree_nocmp deps_remain[DEPS_REMAIN_SIZE];
  size_t deps_remain_cnt;
  size_t scopeidx;
};

struct linked_list_head rules_remain_list =
  STIR_LINKED_LIST_HEAD_INITER(rules_remain_list);

static inline void ruleremain_add(struct rule *rule)
{
  if (rule->remain)
  {
    return;
  }
  linked_list_add_tail(&rule->remainllnode, &rules_remain_list);
  rule->remain = 1;
}
static inline void ruleremain_rm(struct rule *rule)
{
  if (!rule->remain)
  {
    return;
  }
  linked_list_delete(&rule->remainllnode);
  rule->remain = 0;
}

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

static inline int tgt_cmp_asym(size_t tgtidx, struct abce_rb_tree_node *n2, void *ud)
{
  struct stirtgt *e2 = ABCE_CONTAINER_OF(n2, struct stirtgt, node);
  if (tgtidx > e2->tgtidx)
  {
    return 1;
  }
  if (tgtidx < e2->tgtidx)
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

size_t tgt_cnt;


void ins_tgt(struct rule *rule, size_t tgtidx)
{
  uint32_t hash = abce_murmur32(0x12345678U, tgtidx);
  struct stirtgt *e;
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

struct stirtgt *rule_get_tgt(struct rule *rule, size_t tgtidx)
{
  struct abce_rb_tree_node *n;
  uint32_t hash = abce_murmur32(0x12345678U, tgtidx);
  struct abce_rb_tree_nocmp *head;
  head = &rule->tgts[hash % (sizeof(rule->tgts)/sizeof(*rule->tgts))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, tgt_cmp_asym, NULL, tgtidx);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct stirtgt, node);
  }
  return NULL;
}

size_t stirdep_cnt;

void ins_dep(struct rule *rule, size_t depidx, int is_recursive, int orderonly)
{
  uint32_t hash = abce_murmur32(0x12345678U, depidx);
  struct stirdep *e;
  struct abce_rb_tree_nocmp *head;
  int ret;
  stirdep_cnt++;
  e = my_malloc(sizeof(*e));
  e->nameidx = depidx;
  e->is_recursive = !!is_recursive;
  e->is_orderonly = !!orderonly;
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

//const int limit = 2;

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
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;

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
  struct linked_list_node *node;
  if (no_cycles[cur])
  {
    return;
  }
  if (parents[cur])
  {
    size_t i;
    fprintf(stderr, "stirmake: cycle found\n");
    for (i = 0; i < rules_size; i++)
    {
      if (parents[i])
      {
        fprintf(stderr, " rule in cycle: (");
        LINKED_LIST_FOR_EACH(node, &rules[i]->tgtlist)
        {
          struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
          fprintf(stderr, " %s", sttable[e->tgtidx]);
        }
        fprintf(stderr, " )\n");
      }
    }
    errxit("cycle found, cannot proceed further");
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

void add_dep_from_rules(struct tgt *tgts, size_t tgtsz,
                        struct dep *deps, size_t depsz,
                        int phony)
{
  size_t i, j;
  for (i = 0; i < tgtsz; i++)
  {
    struct add_deps *entry = add_deps_ensure(stringtab_add(tgts[i].name));
    if (phony)
    {
      entry->phony = 1;
    }
    for (j = 0; j < depsz; j++)
    {
      add_dep_ensure(entry, stringtab_add(deps[j].name));
    }
  }
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

void process_additional_deps(size_t global_scopeidx)
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
      rule->is_inc = 1;

      rule->scopeidx = global_scopeidx;
      rule->ruleid = rules_size++;
      ins_ruleid_by_tgt(entry->tgtidx, rule->ruleid);
      ins_tgt(rule, entry->tgtidx);
      LINKED_LIST_FOR_EACH(node2, &entry->add_deplist)
      {
        struct add_dep *dep = ABCE_CONTAINER_OF(node2, struct add_dep, llnode);
        ins_dep(rule, dep->depidx, 0, 0);
      }
      rule->is_phony = !!entry->phony;
      rule->is_rectgt = 0;
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
      ins_dep(rule, dep->depidx, 0, 0);
    }
    LINKED_LIST_FOR_EACH(node2, &rule->deplist)
    {
      struct stirdep *dep = ABCE_CONTAINER_OF(node2, struct stirdep, llnode);
      ins_ruleid_by_dep(dep->nameidx, rule->ruleid);
      //printf(" dep: %s\n", dep->name);
    }
  }
}

void add_rule(struct tgt *tgts, size_t tgtsz,
              struct dep *deps, size_t depsz,
              char ***cmdargs, size_t cmdargsz,
              int phony, int rectgt, int maybe, int dist,
              char *prefix, size_t scopeidx)
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
  rule->scopeidx = scopeidx;
  rule->ruleid = rules_size++;
  rule->cmd = cmd;
  rule->is_phony = !!phony;
  rule->is_maybe = !!maybe;
  rule->is_rectgt = !!rectgt;
  rule->is_dist = !!dist;

  for (i = 0; i < tgtsz; i++)
  {
    size_t tgtidx = stringtab_add(tgts[i].name);
    ins_tgt(rule, tgtidx);
    ins_ruleid_by_tgt(tgtidx, rule->ruleid);
  }
  for (i = 0; i < depsz; i++)
  {
    size_t nameidx = stringtab_add(deps[i].name);
    ins_dep(rule, nameidx, !!deps[i].rec, !!deps[i].orderonly);
    ins_ruleid_by_dep(nameidx, rule->ruleid);
  }
  rule->diridx = stringtab_add(prefix);
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

void print_cmd(const char *prefix, char **argiter_orig)
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
  iovs = malloc(sizeof(*iovs)*(argcnt*2+4));
  iovs[0].iov_base = "[";
  iovs[0].iov_len = 1;
  iovs[1].iov_base = (void*)prefix;
  iovs[1].iov_len = strlen(prefix);
  iovs[2].iov_base = "] ";
  iovs[2].iov_len = 2;
  for (i = 0; i < argcnt; i++)
  {
    iovs[3+2*i+0].iov_base = argiter_orig[i];
    iovs[3+2*i+0].iov_len = strlen(argiter_orig[i]);
    iovs[3+2*i+1].iov_base = " ";
    iovs[3+2*i+1].iov_len = 1;
  }
  iovs[3+2*argcnt].iov_base = "\n";
  iovs[3+2*argcnt].iov_len = 1;
  writev(1, iovs, 4+2*argcnt);
}

const char *makecmds[] = {
  "make",
  "gmake",
  "/usr/bin/make",
  "/usr/bin/gmake",
  "/usr/local/bin/make",
  "/usr/local/bin/gmake",
  "/usr/pkg/bin/make",
  "/usr/pkg/bin/gmake",
  "/opt/bin/make",
  "/opt/bin/gmake",
  "/opt/gnu/bin/make",
  "/opt/gnu/bin/gmake",
  "/bin/make",
  "/bin/gmake",
};
size_t makecmds_size = sizeof(makecmds)/sizeof(*makecmds);

int is_makecmd(const char *cmd)
{
  size_t i;
  for (i = 0; i < makecmds_size; i++)
  {
    if (strcmp(cmd, makecmds[i]) == 0)
    {
      return 1;
    }
  }
  return 0;
}

void do_makecmd(const char *cmd)
{
  if (is_makecmd(cmd))
  {
    char env[128] = {0};
    snprintf(env, sizeof(env), " --jobserver-fds=%d,%d -j",
             jobserver_fd[0], jobserver_fd[1]);
    setenv("MAKEFLAGS", env, 1);
  }
  else
  {
    close(jobserver_fd[0]);
    close(jobserver_fd[1]);
  }
}

void child_execvp_wait(const char *prefix, const char *cmd, char **args)
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
    do_makecmd(cmd);
    print_cmd(prefix, args);
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
  const char *dir = sttable[rules[ruleid]->diridx];
  char ***argiter;
  char **oneargiter;
  size_t argcnt = 0;

  args = cmd.args;
  argiter = args;

  if (debug)
  {
    printf("start args:\n");
  }
  while (*argiter)
  {
    oneargiter = *argiter++;
    if (debug)
    {
      printf(" ");
    }
    while (*oneargiter)
    {
      if (debug)
      {
        printf(" %s", *oneargiter);
      }
      oneargiter++;
    }
    if (debug)
    {
      printf("\n");
    }
    argcnt++;
  }
  if (debug)
  {
    printf("end args\n");
  }

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
    if (chdir(dir) != 0)
    {
      write(1, "CHDIRERR\n", 9);
      _exit(1);
    }
    close(self_pipe_fd[0]);
    close(self_pipe_fd[1]);
    update_recursive_pid(0);
    while (argcnt > 1)
    {
      child_execvp_wait(dir, (*argiter)[0], &(*argiter)[0]);
      argiter++;
      argcnt--;
    }
    update_recursive_pid(1);
    do_makecmd((*argiter)[0]);
    print_cmd(dir, &(*argiter)[0]);
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
    rules[ruleid]->is_forked = 1;
    return pid;
  }
}

void mark_executed(int ruleid, int was_actually_executed);

struct timespec rec_mtim(struct rule *r, const char *name)
{
  struct timespec max;
  struct stat statbuf;
  DIR *dir = opendir(name);
  //printf("Statting %s\n", name);
  if (stat(name, &statbuf) != 0)
  {
    errxit("can't open file %s", name);
    exit(1);
  }
  max = statbuf.st_mtim;
  if (lstat(name, &statbuf) != 0)
  {
    errxit("can't open file %s", name);
    exit(1);
  }
  if (ts_cmp(statbuf.st_mtim, max) > 0)
  {
    max = statbuf.st_mtim;
  }
  if (dir == NULL)
  {
    errxit("can't open dir %s", name);
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
      cur = rec_mtim(r, nam2);
    }
    else
    {
      if (stat(nam2, &statbuf) != 0)
      {
        errxit("can't open file %s", nam2);
        exit(1);
      }
      cur = statbuf.st_mtim;
      if (lstat(nam2, &statbuf) != 0)
      {
        errxit("can't open file %s", nam2);
        exit(1);
      }
      if (ts_cmp(statbuf.st_mtim, cur) > 0)
      {
        cur = statbuf.st_mtim;
      }
    }
    int found_rectgt = 0;
    if (r->is_rectgt)
    {
      size_t stidx = stringtab_get(nam2);
      if (stidx != (size_t)-1)
      {
        if (rule_get_tgt(r, stidx) != NULL)
        {
          found_rectgt = 1;
        }
      }
    }
    if (ts_cmp(cur, max) > 0 && !found_rectgt)
    {
      //printf("nam2 file new %s\n", nam2);
      max = cur;
    }
    if ((statbuf.st_mode & S_IFMT) == S_IFDIR)
    {
      cur = rec_mtim(r, nam2);
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

int do_exec(int ruleid)
{
  struct rule *r = rules[ruleid];
  struct stirtgt *first_tgt =
    ABCE_CONTAINER_OF(r->tgtlist.node.next, struct stirtgt, llnode);
  //Rule &r = rules.at(ruleid);
  if (debug)
  {
    printf("do_exec %s\n", sttable[first_tgt->tgtidx]);
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
            if (debug)
            {
              printf("rule %d/%s is phony\n", depid, sttable[e->nameidx]);
            }
            has_to_exec = 1;
            continue;
          }
          if (debug)
          {
            printf("ruleid %d/%s not phony\n", depid, sttable[e->nameidx]);
          }
        }
        else
        {
          if (debug)
          {
            printf("ruleid for tgt %s not found\n", sttable[e->nameidx]);
          }
        }
        if (e->is_recursive)
        {
          struct timespec st_rectim = rec_mtim(r, sttable[e->nameidx]);
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
          // break; // No break, we want to get accurate st_mtim
          continue;
          //perror("can't stat");
          //fprintf(stderr, "file was: %s\n", it->c_str());
          //abort();
        }
        if (!e->is_orderonly)
        {
          if (!seen_nonphony || ts_cmp(statbuf.st_mtim, st_mtim) > 0)
          {
              st_mtim = statbuf.st_mtim;
          }
          seen_nonphony = 1;
        }
        if (lstat(sttable[e->nameidx], &statbuf) != 0)
        {
          has_to_exec = 1;
          // break; // No break, we want to get accurate st_mtim
          continue;
          //perror("can't lstat");
          //fprintf(stderr, "file was: %s\n", it->c_str());
          //abort();
        }
        if (!e->is_orderonly)
        {
          if (!seen_nonphony || ts_cmp(statbuf.st_mtim, st_mtim) > 0)
          {
            st_mtim = statbuf.st_mtim;
          }
          seen_nonphony = 1;
        }
      }
      r->st_mtim_valid = seen_nonphony;
      if (seen_nonphony)
      {
        r->st_mtim = st_mtim;
      }
      LINKED_LIST_FOR_EACH(node, &r->tgtlist)
      {
        struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
        struct stat statbuf;
        if (!cmdequal_db(&db, e->tgtidx, &r->cmd, r->diridx))
        {
          has_to_exec = 1;
        }
        if (debug)
        {
          printf("statting %s\n", sttable[e->tgtidx]);
        }
        if (stat(sttable[e->tgtidx], &statbuf) != 0)
        {
          has_to_exec = 1;
          //break; // can't break, has to compare all commands from DB
          continue;
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
    else // no deps, check that all targets exist
    {
      struct linked_list_node *node;
      LINKED_LIST_FOR_EACH(node, &r->tgtlist)
      {
        struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
        struct stat statbuf;
        if (stat(sttable[e->tgtidx], &statbuf) != 0)
        {
          has_to_exec = 1;
          break;
        }
      }
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
        printf("do_exec: mark_executed %s has_to_exec %d\n",
               sttable[first_tgt->tgtidx], has_to_exec);
      }
      r->is_queued = 1;
      mark_executed(ruleid, 0);
      return 1;
    }
  }
  else
  {
    if (debug)
    {
      printf("do_exec: is queued already\n");
    }
  }
  return 0;
}

int consider(int ruleid)
{
  struct rule *r = rules[ruleid];
  struct stirtgt *first_tgt =
    ABCE_CONTAINER_OF(r->tgtlist.node.next, struct stirtgt, llnode);
  struct linked_list_node *node;
  int toexecute = 0;
  int execed_some = 0;
  if (debug)
  {
    printf("considering %s\n", sttable[first_tgt->tgtidx]);
  }
  if (r->is_executed)
  {
    if (debug)
    {
      printf("already execed %s\n", sttable[first_tgt->tgtidx]);
    }
    return 0;
  }
  if (r->is_executing)
  {
    if (debug)
    {
      printf("already execing %s\n", sttable[first_tgt->tgtidx]);
    }
    return 0;
  }
  r->is_executing = 1;
  LINKED_LIST_FOR_EACH(node, &r->deplist)
  {
    struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
    int idbytgt = get_ruleid_by_tgt(e->nameidx);
    if (idbytgt >= 0)
    {
      if (consider(idbytgt))
      {
        execed_some = 1;
      }
      //execed_some = execed_some || consider(idbytgt); // BAD! DON'T DO THIS!
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
      if (access(sttable[e->nameidx], F_OK) == -1)
      {
        errxit("No %s and rule not found", sttable[e->nameidx]);
        exit(1);
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
    return do_exec(ruleid);
    //ruleids_to_run.push_back(ruleid);
    //r.queued = true;
  }
  return execed_some;
/*
  ruleids_to_run.push_back(ruleid);
  r.executed = true;
*/
}

void reconsider(int ruleid, int ruleid_executed)
{
  struct rule *r = rules[ruleid];
  struct stirtgt *first_tgt =
    ABCE_CONTAINER_OF(r->tgtlist.node.next, struct stirtgt, llnode);
  int toexecute = 0;
  if (debug)
  {
    printf("reconsidering %s\n", sttable[first_tgt->tgtidx]);
  }
  if (r->is_executed)
  {
    if (debug)
    {
      printf("already execed %s\n", sttable[first_tgt->tgtidx]);
    }
    return;
  }
  if (!r->is_executing)
  {
    if (debug)
    {
      printf("rule not executing %s\n", sttable[first_tgt->tgtidx]);
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

void mark_executed(int ruleid, int was_actually_executed)
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
  ruleremain_rm(r);
  r->is_executed = 1;
  r->is_actually_executed = was_actually_executed;
#if 0
  if (ruleid == 0) // FIXME should we return??? This is totally incorrect.
  {
    return;
  }
#endif
  if (r->is_rectgt && r->st_mtim_valid)
  {
    LINKED_LIST_FOR_EACH(node, &r->deplist)
    {
      struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
      if (e->is_recursive)
      {
        struct timespec st_mtim2 = rec_mtim(r, sttable[e->nameidx]);
        if (ts_cmp(st_mtim2, r->st_mtim) > 0)
        {
          r->st_mtim = st_mtim2;
        }
      }
    }
    LINKED_LIST_FOR_EACH(node, &r->tgtlist)
    {
      struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
      struct timespec timespecs[2];
      struct timeval times[2];
      int utimeret;
      timespecs[0] = r->st_mtim;
      timespecs[1] = r->st_mtim;
      times[0].tv_sec = r->st_mtim.tv_sec;
      times[0].tv_usec = (r->st_mtim.tv_nsec+999)/1000;
      times[1].tv_sec = r->st_mtim.tv_sec;
      times[1].tv_usec = (r->st_mtim.tv_nsec+999)/1000;
      if (1)
      {
        utimeret = utimensat(AT_FDCWD, sttable[e->tgtidx], timespecs, 0);
      }
      else
      {
        utimeret = utimes(sttable[e->tgtidx], times);
      }
      if (debug)
      {
        printf("utime %s succeeded? %d\n", sttable[e->tgtidx], (utimeret == 0));
      }
    }
  }
  else if (!r->is_phony && !r->is_maybe)
  {
    struct stat statbuf;
    LINKED_LIST_FOR_EACH(node, &r->tgtlist)
    {
      struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
      if (stat(sttable[e->tgtidx], &statbuf) != 0)
      {
        fprintf(stderr, "stirmake: *** Target %s was not created by rule.\n",
               sttable[e->tgtidx]);
        fprintf(stderr, "stirmake: *** Hint: use @phonyrule for phony rules.\n");
        fprintf(stderr, "stirmake: *** Hint: use @mayberule for rules that may or may not update target.\n");
        fprintf(stderr, "stirmake: *** Hint: use @rectgtrule for rules that have targets inside @recdep.\n");
        errxit("Target %s was not created by rule", sttable[e->tgtidx]);
      }
      if (r->st_mtim_valid && ts_cmp(statbuf.st_mtim, r->st_mtim) < 0)
      {
        fprintf(stderr, "stirmake: *** Target %s was not updated by rule.\n",
               sttable[e->tgtidx]);
        fprintf(stderr, "stirmake: *** Hint: use @phonyrule for phony rules.\n");
        fprintf(stderr, "stirmake: *** Hint: use @mayberule for rules that may or may not update target.\n");
        fprintf(stderr, "stirmake: *** Hint: use @rectgtrule for rules that have targets inside @recdep.\n");
        errxit("Target %s was not updated by rule", sttable[e->tgtidx]);
      }
    }
  }
  /* FIXME we need to handle:
   * a: b
   * b: c
   * c:
   * ...and then somebody types "smka b" and we don't want to build a
   */
  LINKED_LIST_FOR_EACH(node, &r->tgtlist)
  {
    struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
    struct ruleid_by_dep_entry *e2 = find_ruleids_by_dep(e->tgtidx);
    if (e2 == NULL)
    {
      continue;
    }
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
void sigalrm_handler(int x)
{
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
  process_additional_deps(0);
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
    fprintf(stderr, "%s [-vdcb] [-j jobcnt] [target...]\n", argv0);
    fprintf(stderr, "  You can start %s as smka, smkt or smkp or use main command stirmake\n", argv0);
    fprintf(stderr, "  smka, smkt and smkp do not take -t | -p | -a whereas stirmake takes\n");
    fprintf(stderr, "  smka, smkt and smkp do not take -f Stirfile whereas stirmake takes\n");
  }
  else
  {
    fprintf(stderr, "%s [-vdcb] [-j jobcnt] -f Stirfile | -t | -p | -a [target...]\n", argv0);
    fprintf(stderr, "  You can start %s as smka, smkt or smkp or use main command %s\n", argv0, argv0);
    fprintf(stderr, "  smka, smkt and smkp do not take -t | -p | -a whereas %s takes\n", argv0);
    fprintf(stderr, "  smka, smkt and smkp do not take -f Stirfile whereas %s takes\n", argv0);
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
      fprintf(stderr, "stirmake: Seven Processes for the Programmers waiting to type,\n");
      fprintf(stderr, "stirmake: Three for the Operators in their halls of racks,\n");
      fprintf(stderr, "stirmake: Three for the End-users believing all hype.\n");
      fprintf(stderr, "stirmake: One for the Evil Cracker for his dark hacks\n");
      fprintf(stderr, "stirmake: In the land of Recursion where the Shadows lie.\n");
      fprintf(stderr, "stirmake: One Build Tool to rule them all, one Build Tool to find them,\n");
      fprintf(stderr, "stirmake: One Build Tool to bring them all, and in the darkness bind them,\n");
      fprintf(stderr, "stirmake: In the land of Recursion where the Shadows lie.\n");
      fprintf(stderr, "\n");
      fprintf(stderr, "stirmake: *** Recursion misuse detected.\n");
      fprintf(stderr, "stirmake: *** Stirmake is designed to be used non-recursively.\n");
      fprintf(stderr, "stirmake: *** Exiting.\n");
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

void *my_memrchr(const void *s, int c, size_t n)
{
  unsigned const char *ptr = s + n;
  while (n > 0)
  {
    ptr--;
    if (*ptr == c)
    {
      return (void*)ptr;
    }
    n--;
  }
  return NULL;
}

char *process_mflags(void)
{
  char *iter = getenv("MAKEFLAGS");
  while (iter && *iter == ' ')
  {
    iter++;
  }
  while (iter && *iter == '-')
  {
    if (strncmp(iter, "--jobserver-fds=", strlen("--jobserver-fds=")) == 0)
    {
      iter += strlen("--jobserver-fds=");
      return iter;
    }
    iter = strchr(iter, ' ');
    while (iter && *iter == ' ')
    {
      iter++;
    }
  }
  return NULL;
}

char *calc_forward_path(char *storcwd, size_t upcnt)
{
  char *fwd_path = NULL;
  size_t idx = strlen(storcwd);
  size_t i;
  for (i = 0; i < upcnt; i++)
  {
    char *ptr;
    ptr = my_memrchr(storcwd, '/', idx);
    if (ptr == NULL)
    {
      idx = 0;
    }
    else
    {
      idx = ptr - storcwd;
    }
  }
  if (storcwd[idx] == '/')
  {
    fwd_path = storcwd + idx + 1;
  }
  else
  {
    fwd_path = storcwd + idx;
  }
  if (*fwd_path == '\0')
  {
    fwd_path = ".";
  }
  return fwd_path;
}

int process_jobserver(int fds[2])
{
  char *mflags;
  mflags = process_mflags();
  if (mflags == NULL)
  {
    return -ENOENT;
  }
  if (sscanf(mflags, "%d,%d", &fds[0], &fds[1]) != 2)
  {
    fprintf(stderr, "stirmake: Jobserver unavailable\n");
    return -EINVAL;
  }
  if (fcntl(jobserver_fd[0], F_GETFD) == -1)
  {
    fprintf(stderr, "stirmake: Jobserver unavailable\n");
    return -EBADF;
  }
  if (fcntl(jobserver_fd[1], F_GETFD) == -1)
  {
    fprintf(stderr, "stirmake: Jobserver unavailable\n");
    return -EBADF;
  }
  fprintf(stderr, "stirmake: Jobserver available\n");
  return 0;
}

struct linked_list_head cleanlist = STIR_LINKED_LIST_HEAD_INITER(cleanlist);

char *dir_up(char *old)
{
  size_t oldlen = strlen(old);
  size_t uncanonized_capacity = oldlen + 4;
  char *uncanonized = malloc(uncanonized_capacity);
  char *canonized;
  if (snprintf(uncanonized, uncanonized_capacity, "%s/..", old) >=
      uncanonized_capacity)
  {
    abort();
  }
  canonized = canon(uncanonized);
  free(uncanonized);
  return canonized;
}

void do_clean(char *fwd_path, int objs, int bins)
{
  size_t i, fp_len;
  int all;
  struct linked_list_node *node, *node2, *node3, *nodetmp;
  all = (strcmp(fwd_path, ".") == 0);
  fp_len = strlen(fwd_path);
  for (i = 0; i < rules_size; i++)
  {
    int doit = all;
    char *prefix = sttable[rules[i]->diridx];
    if (strncmp(prefix, fwd_path, fp_len) == 0)
    {
      if (prefix[fp_len] == '/' || prefix[fp_len] == '\0')
      {
        doit = 1;
      }
    }
    if (!doit)
    {
      continue; // optimization
    }
    if (rules[i]->is_phony)
    {
      continue;
    }
    if (rules[i]->is_inc)
    {
      continue;
    }
    if (rules[i]->is_cleanqueued)
    {
      continue;
    }
    LINKED_LIST_FOR_EACH(node2, &rules[i]->tgtlist)
    {
      struct stirtgt *tgt = ABCE_CONTAINER_OF(node2, struct stirtgt, llnode);
      char *oldname = strdup(sttable[tgt->tgtidx]);
      char *name;
      size_t stidx;
      int ruleid;
      struct linked_list_head tmplist; // extra list for reversing
      int prefixok;
      char *rprefix;
      linked_list_head_init(&tmplist);
      if (debug)
      {
        printf("itering tgt %s\n", oldname);
      }
      for (;;)
      {
        if (debug)
        {
          printf("itering dir %s\n", oldname);
        }
        name = dir_up(oldname);
        if (debug)
        {
          printf("dir-up %s\n", name);
        }
        free(oldname);
        oldname = name;
        stidx = stringtab_get(name);
        if (stidx == (size_t)-1)
        {
          break;
        }
        ruleid = get_ruleid_by_tgt(stidx);
        if (ruleid < 0)
        {
          break;
        }
        if (rules[ruleid]->is_phony)
        {
          break;
        }
        if (rules[ruleid]->is_inc)
        {
          break;
        }
        if (rules[ruleid]->is_cleanqueued)
        {
          break;
        }
        prefixok = all;
        rprefix = sttable[rules[ruleid]->diridx];
        if (strncmp(rprefix, fwd_path, fp_len) == 0)
        {
          if (rprefix[fp_len] == '/' || rprefix[fp_len] == '\0')
          {
            prefixok = 1;
          }
        }
        if (!prefixok)
        {
          break;
        }
        rules[ruleid]->is_cleanqueued = 1;
        if (debug)
        {
          printf("adding rule %d to rm list\n", ruleid);
        }
        linked_list_add_head(&rules[ruleid]->cleanllnode, &tmplist);
      }
      free(oldname);
      LINKED_LIST_FOR_EACH_SAFE(node3, nodetmp, &tmplist)
      {
        struct rule *rule = ABCE_CONTAINER_OF(node3, struct rule, cleanllnode);
        linked_list_delete(&rule->cleanllnode);
        linked_list_add_head(&rule->cleanllnode, &cleanlist);
        if (!rule->is_cleanqueued)
        {
          printf("rule %d not cleanqueued\n", rule->ruleid);
          abort();
        }
      }
    }
    rules[i]->is_cleanqueued = 1;
    linked_list_add_head(&rules[i]->cleanllnode, &cleanlist);
  }
  LINKED_LIST_FOR_EACH(node, &cleanlist)
  {
    struct rule *rule = ABCE_CONTAINER_OF(node, struct rule, cleanllnode);
    int dist = rule->is_dist;
    LINKED_LIST_FOR_EACH(node2, &rule->tgtlist)
    {
      struct stirtgt *tgt = ABCE_CONTAINER_OF(node2, struct stirtgt, llnode);
      if ((objs && !dist) || (bins && dist))
      {
        char *name = sttable[tgt->tgtidx];
        struct stat statbuf;
        int ret = 0;
        ret = stat(name, &statbuf);
        if (ret != 0 && errno == ENOENT)
        {
          continue;
        }
        if (ret != 0)
        {
          perror("stirmake: *** Can't stat file");
          errxit("Can't stat file %s", name);
        }
        ret = 0;
        if (S_ISREG(statbuf.st_mode))
        {
          printf("unlink regular: %s\n", name);
          ret = unlink(name);
        }
        else if (S_ISLNK(statbuf.st_mode))
        {
          printf("unlink symlink: %s\n", name);
          ret = unlink(name);
        }
        else if (S_ISSOCK(statbuf.st_mode))
        {
          printf("unlink socket: %s\n", name);
          ret = unlink(name);
        }
        else if (S_ISFIFO(statbuf.st_mode))
        {
          printf("unlink fifo: %s\n", name);
          ret = unlink(name);
        }
        else if (S_ISDIR(statbuf.st_mode))
        {
          printf("rmdir: %s\n", name);
          ret = rmdir(name);
          if (ret != 0)
          {
            perror("stirmake: (ignoring) can't rmdir");
            ret = 0;
          }
        }
        if (ret != 0)
        {
          perror("stirmake: *** Can't unlink file");
          errxit("Can't unlink file %s", name);
        }
      }
    }
  }
}

struct cmd dbyycmd_add(struct dbyycmd *cmds, size_t cmdssz)
{
  struct cmd ret = {};
  size_t i, j;
  char ***result = my_malloc((cmdssz+1) * sizeof(*result));
  for (i = 0; i < cmdssz; i++)
  {
    result[i] = my_malloc((cmds[i].argssz+1) * sizeof(*(result[i])));
    for (j = 0; j < cmds[i].argssz; j++)
    {
      result[i][j] = my_strdup(cmds[i].args[j]);
    }
    result[i][cmds[i].argssz] = NULL;
  }
  result[cmdssz] = NULL;
  ret.args = result;
  return ret;
}

void load_db(void)
{
  struct dbyy dbyy = {};
  size_t i;
  linked_list_head_init(&db.ll);
  dbyynameparse(".stir.db", &dbyy, 0);
  for (i = 0; i < dbyy.rulesz; i++)
  {
    struct dbe *dbe = my_malloc(sizeof(struct dbe));
    dbe->tgtidx = stringtab_add(dbyy.rules[i].tgt);
    dbe->diridx = stringtab_add(dbyy.rules[i].dir);
    dbe->cmds = dbyycmd_add(dbyy.rules[i].cmds, dbyy.rules[i].cmdssz);
    ins_dbe(&db, dbe);
  }
}

void merge_db(void)
{
  size_t i;
  struct linked_list_node *node;
  FILE *f;
  int firstrule = 1;
  for (i = 0; i < rules_size; i++)
  {
    struct rule *rule = rules[i];
    if (!rule->is_actually_executed)
    {
      if (rule->is_forked)
      {
        LINKED_LIST_FOR_EACH(node, &rule->tgtlist)
        {
          struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
          if (debug)
          {
            printf("removing %s from DB\n", sttable[e->tgtidx]);
          }
          maybe_del_dbe(&db, e->tgtidx);
        }
      }
      continue;
    }
    LINKED_LIST_FOR_EACH(node, &rule->tgtlist)
    {
      struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
      struct dbe *dbe = my_malloc(sizeof(struct dbe));
      dbe->tgtidx = e->tgtidx;
      dbe->diridx = rule->diridx;
      dbe->cmds = rule->cmd;
      ins_dbe(&db, dbe);
    }
  }
  f = fopen(".stir.db", "w");
  if (f == NULL)
  {
    fprintf(stderr, "Can't open .stir.db"); // can't use errxit
    exit(1);
  }
  LINKED_LIST_FOR_EACH(node, &db.ll)
  {
    struct dbe *dbe = ABCE_CONTAINER_OF(node, struct dbe, llnode);
    char ***argiter = dbe->cmds.args;
    char **oneargiter;
    // dir tgt:
    if (firstrule)
    {
      firstrule = 0;
    }
    else
    {
      fprintf(f, "\n");
    }
    fprintf(f, "\"");
    escape_string(f, sttable[dbe->diridx]);
    fprintf(f, "\" \"");
    escape_string(f, sttable[dbe->tgtidx]);
    fprintf(f, "\":\n");
    while (*argiter)
    {
      int first = 1;
      fprintf(f, "\t");
      oneargiter = *argiter++;
      while (*oneargiter)
      {
        if (first)
        {
          first = 0;
        }
        else
        {
          fprintf(f, " ");
        }
        fprintf(f, "\"");
        escape_string(f, *oneargiter);
        fprintf(f, "\"");
        oneargiter++;
      }
      fprintf(f, "\n");
    }
  }
  fclose(f);
}


void create_pipe(int jobcnt)
{
  int err = 0;
  int buf;
  int i;
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, jobserver_fd) != 0)
  {
    printf("28\n");
    abort();
  }
  //set_nonblock(jobserver_fd[0]); // Blocking on purpose (because of GNU make)
  set_nonblock(jobserver_fd[1]);

  buf = 512*1024;
  setsockopt(jobserver_fd[0], SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));
  buf = 512*1024;
  setsockopt(jobserver_fd[1], SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));
  buf = 512*1024;
  setsockopt(jobserver_fd[0], SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
  buf = 512*1024;
  setsockopt(jobserver_fd[1], SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));

  for (i = 0; i < jobcnt - 1; i++)
  {
    if (write(jobserver_fd[1], ".", 1) == -1)
    {
      err = 1;
      break;
    }
  }
  if (!err)
  {
    return;
  }
  printf("stirmake: Socket pair didn't work, using pipe instead\n");
  close(jobserver_fd[0]);
  close(jobserver_fd[1]);
  if (pipe(jobserver_fd) != 0)
  {
    printf("28.2\n");
    abort();
  }
  //set_nonblock(jobserver_fd[0]); // Blocking on purpose (because of GNU make)
  set_nonblock(jobserver_fd[1]);
  err = 0;
  for (i = 0; i < jobcnt - 1; i++)
  {
    if (write(jobserver_fd[1], ".", 1) == -1)
    {
      err = 1;
      break;
    }
  }
  if (!err)
  {
    return;
  }
  printf("stirmake: Could not write all tokens to jobserver (only %d)\n", i);
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
  int filename_set = 0;
  const char *filename = "Stirfile";
  uint32_t forkedchildcnt = 0;
  int narration = 0;
  int jobcnt = 1;
  int cleanbinaries = 0;
  int clean = 0;
  char cwd[PATH_MAX];
  char cwd_sameproj[PATH_MAX];
  char storcwd[PATH_MAX];
  char curcwd[PATH_MAX];
  size_t upcnt = 0;
  size_t upcnt_sameproj = 0;
  char *fwd_path = ".";
  int ruleid_first = 0;
  int ruleid_first_set = 0;

  char *dupargv0 = strdup(argv[0]);
  char *basenm = basename(dupargv0);

  sizeof_my_arena = 1536*1024*1024;
  my_arena = mmap(NULL, sizeof_my_arena, PROT_READ | PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (my_arena == NULL || my_arena == MAP_FAILED)
  {
    errxit("Can't mmap arena");
    exit(1);
  }
  my_arena_ptr = my_arena;

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

  debug = 0;
  while ((opt = getopt(argc, argv, "vdf:Htpaj:hcb")) != -1)
  {
    switch (opt)
    {
    case 'v':
      version(argv[0]);
    case 'd':
      debug = 1;
      break;
    case 'H':
      narration = 1;
      setlocale(LC_CTYPE, "");
      break;
    case 'f': // FIXME what if optarg contains directories?
      filename_set = 1;
      filename = optarg;
      break;
    case 'c': // clean
      clean = 1;
      break;
    case 'b': // clean with binaries
      cleanbinaries = 1;
      break;
    case 'j':
      jobcnt = atoi(optarg);
      if (jobcnt < 1)
      {
        usage(argv[0]);
      }
      if (jobcnt > MAX_JOBCNT)
      {
        usage(argv[0]);
      }
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
    case 'h':
      usage(argv[0]);
    }
  }

  if (mode == MODE_NONE && !filename_set)
  {
    usage(argv[0]);
  }
  if (mode != MODE_NONE && filename_set)
  {
    usage(argv[0]);
  }

  recursion_misuse_prevention();

  if (!filename_set)
  {
    size_t curupcnt = 0;
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
      abort();
    }
    if (getcwd(storcwd, sizeof(storcwd)) == NULL)
    {
      abort();
    }
    if (getcwd(curcwd, sizeof(curcwd)) == NULL)
    {
      abort();
    }
    if (getcwd(cwd_sameproj, sizeof(cwd_sameproj)) == NULL)
    {
      abort();
    }
    for (;;)
    {
      if (strcmp(curcwd, "/") == 0)
      {
        break;
      }
      if (chdir("..") != 0)
      {
        abort();
      }
      if (getcwd(curcwd, sizeof(curcwd)) == NULL)
      {
        printf("can't getcwd\n");
        abort();
      }
      curupcnt++;
      abce_init(&abce);
      init_main_for_realpath(&main, storcwd); // FIXME leaks
      main.abce = &abce;
      main.freeform_token_seen = 1;
      stiryy_init(&stiryy, &main, ".", abce.dynscope);
      f = fopen("Stirfile", "r");
      if (f)
      {
        stiryydoparse(f, &stiryy);
        fclose(f);
        if (main.subdirseen)
        {
          upcnt = curupcnt;
          if (snprintf(cwd, sizeof(cwd), "%s", curcwd) >= sizeof(cwd))
          {
            printf("can't snprintf\n");
            abort();
          }
        }
        if (main.subdirseen_sameproject)
        {
          upcnt_sameproj = curupcnt;
          if (snprintf(cwd_sameproj, sizeof(cwd_sameproj), "%s", curcwd)
              >= sizeof(cwd_sameproj))
          {
            printf("can't snprintf\n");
            abort();
          }
        }
      }
      stiryy_free(&stiryy);
      abce_free(&abce);
    }
    printf("stirmake: Using directory %s\n", cwd);
    if (chdir(cwd) != 0)
    {
      abort();
    }
  }

  load_db();
  abce_init(&abce);
  main.abce = &abce;
  main.freeform_token_seen = 0;
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

  if (mode == MODE_ALL || mode == MODE_NONE)
  {
    fwd_path = ".";
  }
  else if (mode == MODE_THIS)
  {
    fwd_path = calc_forward_path(storcwd, upcnt);
    printf("stirmake: Forward path: %s\n", fwd_path);
  }
  else if (mode == MODE_PROJECT)
  {
    fwd_path = calc_forward_path(cwd_sameproj, upcnt - upcnt_sameproj);
    printf("stirmake: Forward path: %s\n", fwd_path);
  }
  else
  {
    abort();
  }
  for (i = 0; i < main.rulesz; i++)
  {
    if (main.rules[i].targetsz > 0) // FIXME chg to if (1)
    {
      if (main.rules[i].deponly)
      {
        continue;
      }
      if (debug)
      {
        printf("ADDING RULE\n");
      }
      add_rule(main.rules[i].targets, main.rules[i].targetsz,
               main.rules[i].deps, main.rules[i].depsz,
               main.rules[i].shells, main.rules[i].shellsz,
               main.rules[i].phony, main.rules[i].rectgt, main.rules[i].maybe,
               main.rules[i].dist,
               main.rules[i].prefix, main.rules[i].scopeidx);
      if (   (!ruleid_first_set)
          && (   strcmp(fwd_path, ".") == 0
              || strcmp(fwd_path, main.rules[i].prefix) == 0))
      {
        if (rules_size == 0)
        {
          abort();
        }
        ruleid_first = rules_size - 1;
        ruleid_first_set = 1;
      }
    }
  }
  for (i = 0; i < main.rulesz; i++)
  {
    if (main.rules[i].targetsz > 0) // FIXME chg to if (1)
    {
      if (!main.rules[i].deponly)
      {
        continue;
      }
      if (debug)
      {
        printf("ADDING DEP\n");
      }
      add_dep_from_rules(main.rules[i].targets, main.rules[i].targetsz,
                         main.rules[i].deps, main.rules[i].depsz, 0);
    }
  }
  if (!ruleid_first_set)
  {
    errxit("no applicable rules");
  }
  if (optind == argc)
  {
    free(better_cycle_detect(ruleid_first));
    if (clean || cleanbinaries)
    {
      do_clean(fwd_path, clean, cleanbinaries);
      exit(0); // don't process first rule
    }
    ruleremain_add(rules[ruleid_first]);
  }
  else if (mode == MODE_ALL || mode == MODE_NONE)
  {
    for (i = optind; i < argc; i++)
    {
      size_t stidx = stringtab_add(argv[i]);
      int ruleid = get_ruleid_by_tgt(stidx);
      if (ruleid < 0)
      {
        errxit("rule '%s' not found", argv[i]);
      }
      free(better_cycle_detect(ruleid));
      ruleremain_add(rules[ruleid]);
    }
    if (clean || cleanbinaries)
    {
      do_clean(".", clean, cleanbinaries);
    }
  }
  else if (mode == MODE_THIS)
  {
    for (i = optind; i < argc; i++)
    {
      size_t bufsz = strlen(fwd_path) + strlen(argv[i]) + 2;
      char *buf = malloc(bufsz);
      char *can;
      size_t stidx;
      int ruleid;
      snprintf(buf, bufsz, "%s/%s", fwd_path, argv[i]);
      can = canon(buf);
      free(buf);
      stidx = stringtab_add(can);
      ruleid = get_ruleid_by_tgt(stidx);
      if (ruleid < 0)
      {
        errxit("rule '%s' not found", argv[i]);
      }
      free(better_cycle_detect(ruleid));
      ruleremain_add(rules[ruleid]);
      free(can);
    }
    if (clean || cleanbinaries)
    {
      do_clean(fwd_path, clean, cleanbinaries);
    }
  }
  else
  {
    for (i = optind; i < argc; i++)
    {
      size_t bufsz = strlen(fwd_path) + strlen(argv[i]) + 2;
      char *buf = malloc(bufsz);
      char *can;
      size_t stidx;
      int ruleid;
      snprintf(buf, bufsz, "%s/%s", fwd_path, argv[i]);
      can = canon(buf);
      free(buf);
      stidx = stringtab_add(can);
      ruleid = get_ruleid_by_tgt(stidx);
      if (ruleid < 0)
      {
        errxit("rule '%s' not found", argv[i]);
      }
      free(better_cycle_detect(ruleid));
      ruleremain_add(rules[ruleid]);
      free(can);
    }
    if (clean || cleanbinaries)
    {
      do_clean(fwd_path, clean, cleanbinaries);
    }
  }

  for (i = 0; i < stiryy.main->cdepincludesz; i++)
  {
    struct incyy incyy = {.prefix = stiryy.main->cdepincludes[i].prefix};
    size_t j;
    size_t fnamesz =
      strlen(incyy.prefix) + strlen(stiryy.main->cdepincludes[i].name) + 2;
    char *fname = malloc(fnamesz);
    if (snprintf(fname, fnamesz, "%s/%s", incyy.prefix, stiryy.main->cdepincludes[i].name) >= fnamesz)
    {
      printf("24.5\n");
      abort();
    }
    if (debug)
    {
      printf("reading cdepincludes from %s\n", fname);
    }
    f = fopen(fname, "r");
    if (!f)
    {
      printf("25\n");
      abort();
    }
    free(fname);
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

  process_additional_deps(abce.dynscope.u.area->u.sc.locidx);

#if 0
  unsigned char *no_cycles = better_cycle_detect(0); // FIXME 0 incorrect!
#endif
  for (i = 0; i < rules_size; i++)
  {
    calc_deps_remain(rules[i]);
  }

  // Delete unreachable rules from ruleids_by_dep
#if 0
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

  if (process_jobserver(jobserver_fd) != 0)
  {
    create_pipe(jobcnt);
  }

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = sigchld_handler;
  sigaction(SIGCHLD, &sa, NULL);

  struct sigaction saint;
  sigemptyset(&saint.sa_mask);
  saint.sa_flags = 0;
  saint.sa_handler = sigalrm_handler;
  sigaction(SIGALRM, &saint, NULL);

  struct linked_list_node *node;
back:
  LINKED_LIST_FOR_EACH(node, &rules_remain_list)
  {
    int ruleid = ABCE_CONTAINER_OF(node, struct rule, remainllnode)->ruleid;
    if (consider(ruleid))
    {
      if (debug)
      {
        printf("goto back\n");
      }
      goto back; // this can edit the list, need to re-start iteration
    }
  }

  //consider(0);

  while (ruleids_to_run_size > 0)
  {
    if (children)
    {
      if (!read_jobserver())
      {
        break;
      }
    }
    if (debug)
    {
      printf("forking1 child\n");
    }
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
    if (debug)
    {
      printf("select returned\n");
    }
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
            int ruleid = ruleid_by_pid_erase(pid);
            if (ruleid < 0)
            {
              printf("31.1\n");
              abort();
            }
            children--;
            fprintf(stderr, "stirmake: recipe for target '%s' failed\n", sttable[ABCE_CONTAINER_OF(rules[ruleid]->tgtlist.node.next, struct stirtgt, llnode)->tgtidx]);
            if (WIFSIGNALED(wstatus))
            {
              errxit("Error: signaled");
            }
            else if (WIFEXITED(wstatus))
            {
              errxit("Error %d", (int)WEXITSTATUS(wstatus));
            }
            else
            {
              errxit("Unknown error");
            }
            abort();
          }
        }
        if (children <= 0 && ruleids_to_run_size == 0)
        {
          if (pid < 0 && errno == ECHILD)
          {
            if (linked_list_is_empty(&rules_remain_list))
            {
              goto out;
            }
            else
            {
              fprintf(stderr, "out of children, yet not all targets made\n");
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
        //ruleremain_rm(rules[ruleid]);
        mark_executed(ruleid, 1);
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
        if (!read_jobserver())
        {
          break;
        }
      }
      if (debug)
      {
        printf("forking child\n");
      }
      //std::cout << "forking child" << std::endl;
      fork_child(ruleids_to_run[ruleids_to_run_size-1]);
      ruleids_to_run_size--;
      //ruleids_to_run.pop_back();
    }
  }
out:
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
  merge_db();
  return 0;
}
