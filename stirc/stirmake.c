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
#include <time.h>
#include <stdarg.h>
#include "stircommon.h"

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/sysctl.h>
#if __FreeBSD_version >= 1003000
#define HAS_UTIMENSAT 1
#endif
#endif
#ifdef __linux__
#include <sys/sysinfo.h>
#define HAS_UTIMENSAT 1
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
// Don't know how to detect MacOS version, so HAS_UTIMENSAT not set
#endif
#ifdef __NetBSD__
#include <sys/param.h>
#include <sys/sysctl.h>
#if __NetBSD_Version__ >= 600000000
#define HAS_UTIMENSAT 1
#endif
#endif

#include <sys/select.h>
#include <sys/mman.h>
#include "yyutils.h"
#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "incyyutils.h"
#include "dbyy.h"
#include "dbyyutils.h"
#include "stirtrap.h"
#include "syncbuf.h"

int indentlevel = 0;

void print_indent(void)
{
  int i;
  for (i = 0; i < indentlevel; i++)
  {
    putc(' ', stdout);
  }
}

struct pretend {
  char *fname;
  struct pretend *next;
  int relative;
};
struct pretend *pretend = NULL;

int ispretend(const char *test)
{
  struct pretend *iter = pretend;
  while (iter != NULL)
  {
    if (strcmp(iter->fname, test) == 0)
    {
      return 1;
    }
    iter = iter->next;
  }
  return 0;
}

sig_atomic_t sigterm_atomic;
sig_atomic_t sigint_atomic;
sig_atomic_t sighup_atomic;
sig_atomic_t subproc_sigterm_atomic;
sig_atomic_t subproc_sigint_atomic;
sig_atomic_t subproc_sighup_atomic;

void subproc_sigterm_handler(int x)
{
  subproc_sigterm_atomic = 1;
}
void subproc_sigint_handler(int x)
{
  subproc_sigint_atomic = 1;
}
void subproc_sighup_handler(int x)
{
  subproc_sighup_atomic = 1;
}

struct abce abce = {};
int abce_inited = 0;

int test = 0;

enum out_sync {
  OUT_SYNC_NONE = 0,
  OUT_SYNC_LINE = 1, // unsupported now
  OUT_SYNC_TARGET = 2,
  OUT_SYNC_RECURSE = 3,
};

enum out_sync out_sync = OUT_SYNC_NONE;

void st_compact(void);

void my_abort(void)
{
  fflush(stdout);
  fflush(stderr);
  signal(SIGABRT, SIG_DFL);
  if (abce_inited)
  {
    abce_compact(&abce);
  }
  st_compact();
  abort();
}

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
  STATHASH_SIZE = 256,
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

enum {
  HASH_SEED = 0x12345678U,
};


int debug = 0;
int ignoreerr = 0;
int doasmuchascan = 0;
int cmdfailed = 0;
int unconditional = 0;
int dry_run = 0;

int self_pipe_fd[2];

int jobserver_fd[2];

struct stringtabentry {
  struct abce_rb_tree_node node;
  char *string;
  size_t len;
  size_t idx;
};

int children = 0;

void merge_db(void);
int ruleid_by_pid_erase(pid_t pid, int *fd);
void errxit(const char *fmt, ...);

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
  uint32_t hash = abce_murmur32(HASH_SEED, tgtidx);
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
  uint32_t hash = abce_murmur32(HASH_SEED, dbe->tgtidx);
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

void escape_string(FILE *f, const char *str)
{
  const char *ptr;
  for (ptr = str; *ptr; ptr++)
  {
    unsigned char uch = *ptr;
    switch (uch)
    {
      case '\\':
        fprintf(f, "\\\\");
        break;
      case '\'':
        fprintf(f, "\\'");
        break;
      case '"':
        fprintf(f, "\\\"");
        break;
      case '\t':
        fprintf(f, "\\t");
        break;
      case '\r':
        fprintf(f, "\\r");
        break;
      case '\n':
        fprintf(f, "\\n");
        break;
      default:
        if (uch < 0x20 || uch >= 0x7F)
        {
          fprintf(f, "\\x%.2X", uch);
          break;
        }
        putc((char)uch, f);
        break;
    }
  }
}

void update_recursive_pid(int parent)
{
  pid_t pid = parent ? getppid() : getpid();
  char buf[64] = {};
  snprintf(buf, sizeof(buf), "%d", (int)pid);
  setenv("STIRMAKEPID", buf, 1);
}

struct string_plus_len {
  const char *str;
  size_t len;
};

static inline int stringtabentry_cmp_asym(const struct string_plus_len *stringlen, struct abce_rb_tree_node *n2, void *ud)
{
  struct stringtabentry *e = ABCE_CONTAINER_OF(n2, struct stringtabentry, node);
  int ret;
  const char *str2, *str1;
  size_t len2, len1;
  size_t minlen;
  str1 = stringlen->str;
  len1 = stringlen->len;
  str2 = e->string;
  len2 = e->len;
  minlen = (len1 < len2) ? len1 : len2;
  ret = memcmp(str1, str2, minlen);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 < len2)
  {
    return -1;
  }
  if (len1 > len2)
  {
    return 1;
  }
  return 0;
}
static inline int stringtabentry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct stringtabentry *e1 = ABCE_CONTAINER_OF(n1, struct stringtabentry, node);
  struct stringtabentry *e2 = ABCE_CONTAINER_OF(n2, struct stringtabentry, node);
  int ret;
  size_t len1 = e1->len;
  size_t len2 = e2->len;
  size_t minlen = (len1 < len2) ? len1 : len2;
  ret = memcmp(e1->string, e2->string, minlen);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 < len2)
  {
    return -1;
  }
  if (len1 > len2)
  {
    return 1;
  }
  return 0;
}

static inline size_t stir_topages(size_t limit)
{
  long pagesz = sysconf(_SC_PAGE_SIZE);
  size_t pages, actlimit;
  if (pagesz <= 0)
  {
    abort();
  }
  pages = (limit + (pagesz-1)) / pagesz;
  actlimit = pages * pagesz;
  return actlimit;
}

void *stir_do_mmap_madvise(size_t bytes)
{
  void *ptr;
  bytes = stir_topages(bytes);
  // Ugh. I wish all systems had simple and compatible interface.
#ifdef MAP_ANON
  ptr = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);
#else
  #ifdef MAP_ANONYMOUS
    #ifdef MAP_NORESERVE
  ptr = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
    #else
  ptr = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    #endif
  #else
  {
    int fd;
    fd = open("/dev/zero", O_RDWR);
    if (fd < 0)
    {
      abort();
    }
    ptr = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS|MAP_NORESERVE, fd, 0);
    close(fd);
  }
  #endif
#endif
#ifdef MADV_DONTNEED
  #ifdef __linux__
  if (ptr && ptr != MAP_FAILED)
  {
    madvise(ptr, bytes, MADV_DONTNEED); // Linux-ism
  }
  #endif
#endif
  if (ptr == MAP_FAILED)
  {
    return NULL;
  }
  return ptr;
}

void stir_do_munmap(void *ptr, size_t bytes)
{
  munmap(ptr, stir_topages(bytes));
}


char *my_arena;
char *my_arena_ptr;
size_t sizeof_my_arena;

void *my_malloc(size_t sz)
{
  void *result = my_arena_ptr;
  my_arena_ptr += (sz+7)/8*8;
  if (sz > sizeof_my_arena)
  {
    fprintf(stderr, "too large alloc: %zu bytes\n", sz);
    my_abort();
  }
  if (my_arena_ptr >= my_arena + sizeof_my_arena)
  {
    if (debug)
    {
      print_indent();
      printf("allocating new arena\n");
    }
    my_arena = stir_do_mmap_madvise(sizeof_my_arena);
    if (my_arena == NULL || my_arena == MAP_FAILED)
    {
      errxit("Can't mmap new arena");
      exit(2);
    }
    my_arena_ptr = my_arena;
    result = my_arena_ptr;
    my_arena_ptr += (sz+7)/8*8;
    if (my_arena_ptr >= my_arena + sizeof_my_arena)
    {
      fprintf(stderr, "out of memory\n");
      my_abort();
    }
  }
  return result;
}
void my_free(void *ptr)
{
  // nop
}
void *my_strdup_len(const char *str, size_t sz)
{
  char *result = my_malloc(sz + 1);
  memcpy(result, str, sz);
  result[sz] = '\0';
  return result;
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
    if (errno == EINTR)
    {
      return 0;
    }
    printf("111 ret %d, errno %d\n", ret, errno);
    my_abort();
  }
  if (!(pfd.revents & POLLIN))
  {
    my_abort();
  }

#ifdef MSG_DONTWAIT
  ret = recv(jobserver_fd[0], &ch, 1, MSG_DONTWAIT);
#else
  ret = -1;
  errno = ENOTSOCK;
#endif
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
    // TODO to a re-poll?
    ret = read(jobserver_fd[0], &ch, 1);
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_usec = 0;
    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &new_value, NULL);

    if (ret > 1)
    {
      printf("222 ret %d, errno %d\n", ret, errno);
      my_abort();
    }
    return (ret == 1);
  }
  return (ret == 1);
}

struct sttable_entry {
  char *s;
  int is_remade;
};

struct abce_rb_tree_nocmp st[STRINGTAB_SIZE];
struct sttable_entry *sttable = NULL;
size_t st_cap = 1024*1024;
size_t st_cnt;

void st_compact(void)
{
  char *ptr2;
  int errno_save;
  size_t bytes_total, bytes_in_use;
  bytes_total = stir_topages(st_cap * sizeof(*sttable));
  bytes_in_use = stir_topages(st_cnt * sizeof(*sttable));
  ptr2 = (void*)sttable;
  ptr2 += bytes_in_use;
  errno_save = errno;
  munmap(ptr2, bytes_total - bytes_in_use);
  errno = errno_save;
  // don't report errors
}

size_t stringtab_cnt = 0;

size_t stringtab_get(const char *symbol)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  struct string_plus_len stringlen = {.str = symbol, .len = strlen(symbol)};
  hashval = abce_murmur_buf(HASH_SEED, symbol, stringlen.len);
  hashloc = hashval % (sizeof(st)/sizeof(*st));
  n = ABCE_RB_TREE_NOCMP_FIND(&st[hashloc], stringtabentry_cmp_asym,
NULL, &stringlen);
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
  struct string_plus_len stringlen = {.str = symbol, .len = strlen(symbol)};
  hashval = abce_murmur_buf(HASH_SEED, symbol, stringlen.len);
  hashloc = hashval % (sizeof(st)/sizeof(*st));
  n = ABCE_RB_TREE_NOCMP_FIND(&st[hashloc], stringtabentry_cmp_asym, NULL, &stringlen);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct stringtabentry, node)->idx;
  }
  stringtab_cnt++;
  struct stringtabentry *stringtabentry = my_malloc(sizeof(struct stringtabentry));
  stringtabentry->string = my_strdup_len(symbol, stringlen.len);
  stringtabentry->len = stringlen.len;
  if (st_cnt >= st_cap)
  {
    errxit("stringtab full");
    exit(2);
  }
  sttable[st_cnt].s = stringtabentry->string;
  sttable[st_cnt].is_remade = 0;
  stringtabentry->idx = st_cnt++;
  if (abce_rb_tree_nocmp_insert_nonexist(&st[hashloc], stringtabentry_cmp_sym, NULL, &stringtabentry->node) != 0)
  {
    printf("23\n");
    my_abort();
  }
  return stringtabentry->idx;
}

size_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen)
{
  if (strlen(symbol) != symlen)
  {
    printf("22\n");
    my_abort(); // RFE what to do?
  }
  return stringtab_add(symbol);
}

int cmdequal_db(struct db *db, size_t tgtidx, struct cmd *cmd, size_t diridx)
{
  uint32_t hash = abce_murmur32(HASH_SEED, tgtidx);
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;
  struct dbe *dbe;
  head = &db->byname[hash % (sizeof(db->byname)/sizeof(*db->byname))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, dbe_cmp_asym, NULL, tgtidx);
  if (n == NULL)
  {
    if (debug)
    {
      print_indent();
      printf("target %s not found in cmd DB\n", sttable[tgtidx].s);
    }
    return 0;
  }
  dbe = ABCE_CONTAINER_OF(n, struct dbe, node);
  if (dbe->diridx != diridx)
  {
    if (debug)
    {
      print_indent();
      printf("target %s has different dir in cmd DB\n", sttable[tgtidx].s);
    }
    return 0;
  }
  if (!cmd_equal(cmd, &dbe->cmds))
  {
    if (debug)
    {
      print_indent();
      printf("target %s has different cmd in cmd DB\n", sttable[tgtidx].s);
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
    errxit("ruleid by tgt %s already exists", sttable[tgtidx].s);
    exit(2); // FIXME print (filename, linenumber) pair
  }
  linked_list_add_tail(&e->llnode, &ruleid_by_tgt_list);
}

int get_ruleid_by_tgt(size_t tgt)
{
  uint32_t hash = abce_murmur32(HASH_SEED, tgt);
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
  struct linked_list_node primaryllnode;
  struct linked_list_node dupellnode;
  size_t nameidx;
  size_t nameidxnodir;
  unsigned is_recursive:1;
  unsigned is_orderonly:1;
  unsigned is_wait:1;
};

struct dep_remain {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  int ruleid;
  int waitcnt;
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
  size_t tgtidxnodir;
  unsigned is_dist:1;
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
  unsigned is_detouch:1;
  unsigned is_executed:1;
  unsigned is_actually_executed:1; // command actually invoked
  unsigned is_executing:1;
  unsigned is_queued:1;
  unsigned is_cleanqueued:1;
  unsigned remain:1;
  unsigned st_mtim_valid:1;
  unsigned is_inc:1; // whether this is included from dependency file
  unsigned is_dist:1;
  unsigned is_cleanhook:1;
  unsigned is_distcleanhook:1;
  unsigned is_bothcleanhook:1;
  unsigned is_forked:1;
  unsigned is_traversed:1;
  unsigned is_under_consideration:1;
  size_t diridx;
  struct cmdsrc cmdsrc;
  struct cmd cmd; // calculated from cmdsrc
  struct timespec st_mtim;
  int ruleid;
  struct abce_rb_tree_nocmp tgts[TGTS_SIZE];
  struct linked_list_head tgtlist;
  struct abce_rb_tree_nocmp deps[DEPS_SIZE];
  struct linked_list_head deplist;
  struct linked_list_head depremainlist;
  struct linked_list_head primarydeplist;
  struct linked_list_head dupedeplist;
  struct abce_rb_tree_nocmp deps_remain[DEPS_REMAIN_SIZE];
  size_t deps_remain_cnt;
  size_t wait_remain_cnt;
  size_t scopeidx;
  struct syncbuf output;
  struct stirdep *waitloc;
};

extern struct rule **rules; // Needs doubly indirect, otherwise pointers messed up
extern fd_set globfds;

void drain_pipe(struct rule *rule, int fdit);

// FIXME do updates to DB here as well... Now they are not done.
void errxit(const char *fmt, ...)
{
  va_list args;
  const char *prefix = "stirmake: *** ";
  const char *suffix = ". Exiting.\n";
  size_t sz = strlen(prefix) + strlen(fmt) + strlen(suffix) + 1;
  char *fmtdup = malloc(sz);
  int wstatus;
  int fd = -1;
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
    exit(2);
  }
  if (pid > 0)
  {
    int ruleid = ruleid_by_pid_erase(pid, &fd);
    if (ruleid < 0)
    {
      printf("31.1\n");
      my_abort();
    }
    if (fd >= 0)
    {
      drain_pipe(rules[ruleid], fd);
      syncbuf_dump(&rules[ruleid]->output, 1);
      close(fd);
      FD_CLR(fd, &globfds);
      fd = -1;
    }
    if (children <= 0)
    {
      printf("27.E\n");
      my_abort();
    }
    children--;
    if (children != 0)
    {
      write(jobserver_fd[1], ".", 1);
    }
    if (!ignoreerr && wstatus != 0 && pid > 0)
    {
      // FIXME how to find ruleid?
      if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
      {
        struct linked_list_node *node;
        LINKED_LIST_FOR_EACH(node, &rules[ruleid]->tgtlist)
        {
          struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
          unlink(sttable[e->tgtidx].s);
        }
      }
    }
  }
  fprintf(stderr, "stirmake: *** Waiting for child processes to die.\n");
  for (;;)
  {
    int ruleid = -1;
    pid = waitpid(-1, &wstatus, 0);
    if (pid == 0)
    {
      printf("28.E\n");
      my_abort();
    }
    if (pid > 0)
    {
      ruleid = ruleid_by_pid_erase(pid, &fd);
      if (ruleid < 0)
      {
        printf("31.1\n");
        my_abort();
      }
      if (fd >= 0)
      {
        drain_pipe(rules[ruleid], fd);
        syncbuf_dump(&rules[ruleid]->output, 1);
        close(fd);
        FD_CLR(fd, &globfds);
        fd = -1;
      }
    }
    if (!ignoreerr && wstatus != 0 && pid > 0)
    {
      // FIXME how to find ruleid?
      if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
      {
        struct linked_list_node *node;
        LINKED_LIST_FOR_EACH(node, &rules[ruleid]->tgtlist)
        {
          struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
          unlink(sttable[e->tgtidx].s);
        }
      }
    }
    if (children <= 0)
    {
      if (pid < 0 && errno == ECHILD)
      {
        fprintf(stderr, "stirmake: *** No children left. Exiting.\n");
        merge_db();
        exit(2);
      }
      printf("29.E\n");
      my_abort();
    }
    if (pid < 0)
    {
      printf("30.E\n");
      perror("Error was");
      printf("number of children: %d\n", children);
      my_abort();
    }
#if 0 // Let's not do this, just in case the data is messed up.
    int ruleid = ruleid_by_pid_erase(pid);
    if (ruleid < 0)
    {
      printf("31.E\n");
      my_abort();
    }
#endif
    children--;
    if (children != 0)
    {
      write(jobserver_fd[1], ".", 1);
    }
  }
}


char **argdup(int ignore, int noecho, int ismake, char **cmdargs);

static size_t
cmdsrc_cache_add_str_nul(struct abce *abce, const char *str, int *err)
{
  size_t ret;
  ret = abce_cache_add_str_nul(abce, str);
  if (ret == (size_t)-1)
  {
    *err = 1;
  }
  return ret;
}

char *st_ignore = "I";
char *st_noignore = "NI";
char *st_noecho = "NE";
char *st_echo = "E";
char *st_make = "M";
char *st_nomake = "NM";


char ***cmdsrc_eval(struct abce *abce, struct rule *rule)
{
  size_t i, j, k;
  struct cmdsrc *cmdsrc = &rule->cmdsrc;
  char ***result = NULL;
  size_t resultsz = 0;
  size_t resultcap = 16;
  struct stirtgt *first_tgt =
    ABCE_CONTAINER_OF(rule->tgtlist.node.next, struct stirtgt, llnode);
  char *tgt;
  struct linked_list_node *node;
  struct abce_mb scope = abce->cachebase[rule->scopeidx]; // no refup!
  struct abce_mb oldscope = abce->dynscope; // no refup, it's in cache anyway
  size_t atidx, plusidx, baridx, hatidx, ltidx;
  int err = 0;

  atidx = cmdsrc_cache_add_str_nul(abce, "@", &err);
  plusidx = cmdsrc_cache_add_str_nul(abce, "+", &err);
  baridx = cmdsrc_cache_add_str_nul(abce, "|", &err);
  hatidx = cmdsrc_cache_add_str_nul(abce, "^", &err);
  ltidx = cmdsrc_cache_add_str_nul(abce, "<", &err);

  if (err)
  {
    my_abort();
  }

  if (first_tgt->tgtidxnodir != (size_t)-1)
  {
    tgt = sttable[first_tgt->tgtidxnodir].s;
  }
  else
  {
    tgt = neighpath(sttable[rule->diridx].s, sttable[first_tgt->tgtidx].s);
  }
  result = malloc(resultcap * sizeof(*result));
  for (i = 0; i < cmdsrc->itemsz; i++)
  {
    if (cmdsrc->items[i].iscode || cmdsrc->items[i].isfun)
    {
      unsigned char tmpbuf[64] = {};
      size_t tmpsiz = 0;
      struct abce_mb mbstruct = {};
      struct abce_mb *mb = NULL;
      //struct abce_mb mbkey = {};
      struct abce_mb mbnil = {.typ = ABCE_T_N};
      struct abce_mb *mbval = NULL;
      int first = 1;

#if 0
      mbkey = abce_mb_create_string(abce, "@", 1);
      if (mbkey.typ == ABCE_T_N)
      {
        return NULL;
      }
#endif
      mbval = abce_mb_cpush_create_string(abce, tgt, strlen(tgt));
      if (mbval == NULL)
      {
        //abce_mb_refdn(abce, &mbkey);
        return NULL;
      }
      if (abce_sc_replace_val_mb(abce, &scope, &abce->cachebase[atidx], mbval) != 0)
      {
        //abce_mb_refdn(abce, &mbkey);
        abce_cpop(abce);
        return NULL;
      }
      //abce_mb_refdn(abce, &mbkey);
      //abce_mb_refdn(abce, &mbval);
      abce_cpop(abce);

#if 0
      mbkey = abce_mb_create_string(abce, "+", 1);
      if (mbkey.typ == ABCE_T_N)
      {
        return NULL;
      }
#endif
      mbval = abce_mb_cpush_create_array(abce);
      if (mbval == NULL)
      {
        //abce_mb_refdn(abce, &mbkey);
        return NULL;
      }
      if (abce_sc_replace_val_mb(abce, &scope, &abce->cachebase[plusidx], mbval) != 0)
      {
        //abce_mb_refdn(abce, &mbkey);
        //abce_mb_refdn(abce, &mbval);
        abce_cpop(abce);
        return NULL;
      }
      //abce_mb_refdn(abce, &mbkey);
      LINKED_LIST_FOR_EACH(node, &rule->dupedeplist)
      {
        struct stirdep *dep =
          ABCE_CONTAINER_OF(node, struct stirdep, dupellnode);
        char *namenodir;
        if (dep->is_orderonly)
        {
          continue;
        }
        if (dep->nameidxnodir != (size_t)-1)
        {
          namenodir = sttable[dep->nameidxnodir].s;
          mb = abce_mb_cpush_create_string(abce, namenodir, strlen(namenodir));
        }
        else
        {
          namenodir = neighpath(sttable[rule->diridx].s, sttable[dep->nameidx].s);
          mb = abce_mb_cpush_create_string(abce, namenodir, strlen(namenodir));
          free(namenodir);
        }
        if (mb == NULL)
        {
          //abce_mb_refdn(abce, &mbval);
	  abce_cpop(abce);
          return NULL;
        }
        if (abce_mb_array_append(abce, mbval, mb) != 0)
        {
          //abce_mb_refdn(abce, &mbval);
          //abce_mb_refdn(abce, &mb);
	  abce_cpop(abce);
	  abce_cpop(abce);
          return NULL;
        }
	mb = NULL;
        //abce_mb_refdn(abce, &mb);
	abce_cpop(abce);
      }
      abce_cpop(abce);
      mbval = NULL;

#if 0
      mbkey = abce_mb_create_string(abce, "|", 1);
      if (mbkey.typ == ABCE_T_N)
      {
        return NULL;
      }
#endif
      mbval = abce_mb_cpush_create_array(abce);
      if (mbval == NULL)
      {
        //abce_mb_refdn(abce, &mbkey);
        return NULL;
      }
      if (abce_sc_replace_val_mb(abce, &scope, &abce->cachebase[baridx], mbval) != 0)
      {
        //abce_mb_refdn(abce, &mbkey);
        //abce_mb_refdn(abce, &mbval);
	abce_cpop(abce);
        return NULL;
      }
      //abce_mb_refdn(abce, &mbkey);
      LINKED_LIST_FOR_EACH(node, &rule->deplist)
      {
        struct stirdep *dep =
          ABCE_CONTAINER_OF(node, struct stirdep, llnode);
        char *namenodir;
        if (!dep->is_orderonly)
        {
          continue;
        }
        if (dep->nameidxnodir != (size_t)-1)
        {
          namenodir = sttable[dep->nameidxnodir].s;
          mb = abce_mb_cpush_create_string(abce, namenodir, strlen(namenodir));
        }
        else
        {
          namenodir = neighpath(sttable[rule->diridx].s, sttable[dep->nameidx].s);
          mb = abce_mb_cpush_create_string(abce, namenodir, strlen(namenodir));
          free(namenodir);
        }
        if (mb == NULL)
        {
          //abce_mb_refdn(abce, &mbval);
	  abce_cpop(abce);
          return NULL;
        }
        if (abce_mb_array_append(abce, mbval, mb) != 0)
        {
          //abce_mb_refdn(abce, &mbval);
          //abce_mb_refdn(abce, &mb);
	  abce_cpop(abce);
	  abce_cpop(abce);
          return NULL;
        }
        //abce_mb_refdn(abce, &mb);
	abce_cpop(abce);
	mb = NULL;
      }
      //abce_mb_refdn(abce, &mbval);
      abce_cpop(abce);

#if 0
      mbkey = abce_mb_create_string(abce, "^", 1);
      if (mbkey.typ == ABCE_T_N)
      {
        return NULL;
      }
#endif
      mbval = abce_mb_cpush_create_array(abce);
      if (mbval == NULL)
      {
        //abce_mb_refdn(abce, &mbkey);
        return NULL;
      }
      if (abce_sc_replace_val_mb(abce, &scope, &abce->cachebase[hatidx], mbval) != 0)
      {
        //abce_mb_refdn(abce, &mbkey);
        //abce_mb_refdn(abce, &mbval);
	abce_cpop(abce);
        return NULL;
      }
      //abce_mb_refdn(abce, &mbkey);
      LINKED_LIST_FOR_EACH(node, &rule->deplist)
      {
        struct stirdep *dep =
          ABCE_CONTAINER_OF(node, struct stirdep, llnode);
        char *namenodir;
        if (dep->is_orderonly)
        {
          continue;
        }
        if (dep->nameidxnodir != (size_t)-1)
        {
          namenodir = sttable[dep->nameidxnodir].s;
          mb = abce_mb_cpush_create_string(abce, namenodir, strlen(namenodir));
        }
        else
        {
          namenodir = neighpath(sttable[rule->diridx].s, sttable[dep->nameidx].s);
          mb = abce_mb_cpush_create_string(abce, namenodir, strlen(namenodir));
          free(namenodir);
        }
        if (mb == NULL)
        {
          //abce_mb_refdn(abce, &mbval);
	  abce_cpop(abce);
          return NULL;
        }
        if (abce_mb_array_append(abce, mbval, mb) != 0)
        {
          //abce_mb_refdn(abce, &mbval);
          //abce_mb_refdn(abce, &mb);
	  abce_cpop(abce);
	  abce_cpop(abce);
          return NULL;
        }
        if (first)
        {
#if 0
          mbkey = abce_mb_create_string(abce, "<", 1);
          if (mbkey.typ == ABCE_T_N)
          {
            abce_mb_refdn(abce, &mbval);
            return NULL;
          }
#endif
          if (abce_sc_replace_val_mb(abce, &scope, &abce->cachebase[ltidx], mb) != 0)
          {
            //abce_mb_refdn(abce, &mbval);
            //abce_mb_refdn(abce, &mbkey);
            //abce_mb_refdn(abce, &mb);
	    abce_cpop(abce);
	    abce_cpop(abce);
            return NULL;
          }
          //abce_mb_refdn(abce, &mbkey);
          first = 0;
        }
	abce_cpop(abce);
	mb = NULL;
        //abce_mb_refdn(abce, &mb);
      }
      abce_cpop(abce);
      mbval = NULL;
      //abce_mb_refdn(abce, &mbval);
      if (first) // set it to nil if no targets
      {
        mbstruct.typ = ABCE_T_N;
        mbstruct.u.d = 0;
#if 0
        mbkey = abce_mb_create_string(abce, "<", 1);
        if (mbkey.typ == ABCE_T_N)
        {
          abce_mb_refdn(abce, &mbval);
          return NULL;
        }
#endif
        if (abce_sc_replace_val_mb(abce, &scope, &abce->cachebase[ltidx], &mbstruct) != 0)
        {
          //abce_mb_refdn(abce, &mbval);
          //abce_mb_refdn(abce, &mbkey);
          return NULL;
        }
        //abce_mb_refdn(abce, &mbkey);
        first = 0;
      }

      if (cmdsrc->items[i].iscode)
      {
        abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_PUSH_DBL);
        abce_add_double_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf),
                            cmdsrc->items[i].u.locidx);
        abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_JMP);
      }
      else if (cmdsrc->items[i].isfun)
      {
        abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_PUSH_DBL);
        abce_add_double_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf),
                            cmdsrc->items[i].u.funarg.funidx);
        abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_FUNIFY);
        abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_PUSH_DBL);
        abce_add_double_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf),
                            cmdsrc->items[i].u.funarg.argidx);
        abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf),
                         ABCE_OPCODE_PUSH_FROM_CACHE);
        abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_PUSH_DBL);
        abce_add_double_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), 1); // arg cnt
        abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_CALL);
        abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_EXIT);
      }
      if (abce->sp != 0)
      {
        abort();
      }
      abce->dynscope = scope;
      if (abce_engine(abce, tmpbuf, tmpsiz) != 0)
      {
        abce->dynscope = oldscope;
        printf("error %s\n", abce_err_to_str(abce->err.code));
        printf("Backtrace:\n");
        for (i = 0; i < abce->btsz; i++)
        {
          if (abce->btbase[i].typ == ABCE_T_S)
          {
            printf("%s\n", abce->btbase[i].u.area->u.str.buf);
          }
          else
          {
            printf("(-)\n");
          }
        }
        printf("Additional information:\n");
        abce_mb_dump(&abce->err.mb);
        return NULL;
      }
      abce->dynscope = oldscope;
      if (abce_getmbptr(&mb, abce, 0) != 0)
      {
        return NULL;
      }
      if (abce->sp != 1)
      {
        abort();
      }
      // Beware. Now only ref is out of stack. Can't alloc abce memory!
      if (mb->typ != ABCE_T_A)
      {
        abce->err.code = ABCE_E_EXPECT_ARRAY;
	abce_mb_errreplace_noinline(abce, mb);
        //abce->err.mb = abce_mb_refup(abce, mb);
        //abce_mb_refdn(abce, &mb);
	abce_pop(abce);
        return NULL;
      }
      if (cmdsrc->items[i].merge)
      {
        for (j = 0; j < mb->u.area->u.ar.size; j++)
        {
          if (mb->u.area->u.ar.mbs[j].typ != ABCE_T_A)
          {
            abce->err.code = ABCE_E_EXPECT_ARRAY;
            abce->err.mb = abce_mb_refup(abce, &mb->u.area->u.ar.mbs[j]);
            //abce_mb_refdn(abce, &mb);
	    abce_pop(abce);
            return NULL;
          }
          char **cmd = my_malloc((mb->u.area->u.ar.mbs[j].u.area->u.ar.size+4)*sizeof(*cmd));
          cmd[0] = cmdsrc->items[i].ignore ? st_ignore : st_noignore;
          cmd[1] = cmdsrc->items[i].noecho ? st_noecho : st_echo;
          cmd[2] = cmdsrc->items[i].ismake ? st_make : st_nomake;
          for (k = 0; k < mb->u.area->u.ar.mbs[j].u.area->u.ar.size; k++)
          {
            if (mb->u.area->u.ar.mbs[j].u.area->u.ar.mbs[k].typ != ABCE_T_S)
            {
              abce->err.code = ABCE_E_EXPECT_STR;
              abce->err.mb =
                abce_mb_refup(
                  abce, &mb->u.area->u.ar.mbs[j].u.area->u.ar.mbs[k]);
              //abce_mb_refdn(abce, &mb);
	      abce_pop(abce);
              return NULL;
            }
            cmd[3+k] =
              my_strdup(
                mb->u.area->u.ar.mbs[j].u.area->u.ar.mbs[k].u.area->u.str.buf);
          }
          cmd[3+mb->u.area->u.ar.mbs[j].u.area->u.ar.size] = NULL;
          if (resultsz >= resultcap)
          {
            resultcap = 2*resultsz + 16;
            result = realloc(result, resultcap * sizeof(*result));
          }
          result[resultsz++] = cmd;
        }
      }
      else
      {
        char **cmd = my_malloc((mb->u.area->u.ar.size+4)*sizeof(*cmd));
        cmd[0] = cmdsrc->items[i].ignore ? st_ignore : st_noignore;
        cmd[1] = cmdsrc->items[i].noecho ? st_noecho : st_echo;
        cmd[2] = cmdsrc->items[i].ismake ? st_make : st_nomake;
        for (j = 0; j < mb->u.area->u.ar.size; j++)
        {
          if (mb->u.area->u.ar.mbs[j].typ != ABCE_T_S)
          {
            abce->err.code = ABCE_E_EXPECT_STR;
            abce->err.mb = abce_mb_refup(abce, &mb->u.area->u.ar.mbs[j]);
            //abce_mb_refdn(abce, &mb);
	    abce_pop(abce);
            return NULL;
          }
          cmd[3+j] = my_strdup(mb->u.area->u.ar.mbs[j].u.area->u.str.buf);
        }
        cmd[3+mb->u.area->u.ar.size] = NULL;
        if (resultsz >= resultcap)
        {
          resultcap = 2*resultsz + 16;
          result = realloc(result, resultcap * sizeof(*result));
        }
        result[resultsz++] = cmd;
      }
      abce_pop(abce);
      if (abce->sp != 0)
      {
        abort();
      }
      if (abce_sc_replace_val_mb(abce, &scope, &abce->cachebase[atidx], &mbnil) != 0)
      {
        return NULL;
      }
      if (abce_sc_replace_val_mb(abce, &scope, &abce->cachebase[plusidx], &mbnil) != 0)
      {
        return NULL;
      }
      if (abce_sc_replace_val_mb(abce, &scope, &abce->cachebase[ltidx], &mbnil) != 0)
      {
        return NULL;
      }
      if (abce_sc_replace_val_mb(abce, &scope, &abce->cachebase[baridx], &mbnil) != 0)
      {
        return NULL;
      }
      if (abce_sc_replace_val_mb(abce, &scope, &abce->cachebase[hatidx], &mbnil) != 0)
      {
        return NULL;
      }
      //abce_mb_refdn(abce, &mb);
      continue;
    }
    if (!cmdsrc->items[i].merge)
    {
      if (resultsz >= resultcap)
      {
        resultcap = 2*resultsz + 16;
        result = realloc(result, resultcap * sizeof(*result));
      }
      result[resultsz++] = argdup(cmdsrc->items[i].ignore, cmdsrc->items[i].noecho, cmdsrc->items[i].ismake, cmdsrc->items[i].u.args);
      continue;
    }
    for (j = 0; cmdsrc->items[i].u.cmds[j] != NULL; j++)
    {
      if (resultsz >= resultcap)
      {
        resultcap = 2*resultsz + 16;
        result = realloc(result, resultcap * sizeof(*result));
      }
      result[resultsz++] = argdup(cmdsrc->items[i].ignore, cmdsrc->items[i].noecho, cmdsrc->items[i].ismake, cmdsrc->items[i].u.cmds[j]);
    }
  }
  if (resultsz >= resultcap)
  {
    resultcap = 2*resultsz + 16;
    result = realloc(result, resultcap * sizeof(*result));
  }
  result[resultsz++] = NULL;
  return result;
}

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


void ins_tgt(struct rule *rule, size_t tgtidx, size_t tgtidxnodir, int is_dist)
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
    errxit("Target %s already exists in rule", sttable[tgtidx].s);
    exit(2); // FIXME print (filename, linenumber) pair
  }
  linked_list_add_tail(&e->llnode, &rule->tgtlist);
}

struct stirtgt *rule_get_tgt(struct rule *rule, size_t tgtidx)
{
  struct abce_rb_tree_node *n;
  uint32_t hash = abce_murmur32(HASH_SEED, tgtidx);
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

int ins_dep(struct rule *rule,
            size_t depidx, size_t diridx, size_t depidxnodir,
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
  head = &rule->deps[hash % (sizeof(rule->deps)/sizeof(*rule->deps))];
  ret = abce_rb_tree_nocmp_insert_nonexist(head, dep_cmp_sym, NULL, &e->node);
  if (ret == 0)
  {
    linked_list_add_tail(&e->llnode, &rule->deplist);
  }
  else
  {
    if (debug)
    {
      size_t tgtidx = ABCE_CONTAINER_OF(rule->tgtlist.node.next, struct stirtgt, llnode)->tgtidx;
      fprintf(stderr, "stirmake: duplicate dep %s: %s detected\n",
              sttable[tgtidx].s, sttable[depidx].s);
    }
    //my_abort();
    ret = -EEXIST;
  }
  linked_list_add_tail(&e->dupellnode, &rule->dupedeplist);
  if (primary)
  {
    linked_list_add_tail(&e->primaryllnode, &rule->primarydeplist);
  }
  return ret;
}

int deps_remain_has(struct rule *rule, int ruleid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur32(HASH_SEED, ruleid);
  hashloc = hashval % (sizeof(rule->deps_remain)/sizeof(*rule->deps_remain));
  n = ABCE_RB_TREE_NOCMP_FIND(&rule->deps_remain[hashloc], dep_remain_cmp_asym, NULL, ruleid);
  return n != NULL;
}

void deps_remain_forwait(struct rule *rule, int ruleid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur32(HASH_SEED, ruleid);
  hashloc = hashval % (sizeof(rule->deps_remain)/sizeof(*rule->deps_remain));
  n = ABCE_RB_TREE_NOCMP_FIND(&rule->deps_remain[hashloc], dep_remain_cmp_asym, NULL, ruleid);
  if (n == NULL)
  {
    abort();
  }
  struct dep_remain *dep_remain = ABCE_CONTAINER_OF(n, struct dep_remain, node);
  dep_remain->waitcnt++;
}

void deps_remain_erase(struct rule *rule, int ruleid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur32(HASH_SEED, ruleid);
  hashloc = hashval % (sizeof(rule->deps_remain)/sizeof(*rule->deps_remain));
  n = ABCE_RB_TREE_NOCMP_FIND(&rule->deps_remain[hashloc], dep_remain_cmp_asym, NULL, ruleid);
  if (n == NULL)
  {
    return;
  }
  struct dep_remain *dep_remain = ABCE_CONTAINER_OF(n, struct dep_remain, node);
  abce_rb_tree_nocmp_delete(&rule->deps_remain[hashloc], &dep_remain->node);
  linked_list_delete(&dep_remain->llnode);
  rule->deps_remain_cnt--;
  rule->wait_remain_cnt -= dep_remain->waitcnt;
  my_free(dep_remain);
}

size_t dep_remain_cnt;

void deps_remain_insert(struct rule *rule, int ruleid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur32(HASH_SEED, ruleid);
  hashloc = hashval % (sizeof(rule->deps_remain)/sizeof(*rule->deps_remain));
  n = ABCE_RB_TREE_NOCMP_FIND(&rule->deps_remain[hashloc], dep_remain_cmp_asym, NULL, ruleid);
  if (n != NULL)
  {
    return;
  }
  dep_remain_cnt++;
  struct dep_remain *dep_remain = my_malloc(sizeof(struct dep_remain));
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
  uint32_t hash = abce_murmur32(HASH_SEED, depidx);
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
  uint32_t hash = abce_murmur32(HASH_SEED, depidx);
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
    my_abort();
  }
  linked_list_add_tail(&e->llnode, &ruleids_by_dep_list);
  return e;
}

size_t one_ruleid_by_dep_entry_cnt;

void ins_ruleid_by_dep(size_t depidx, int ruleid)
{
  struct ruleid_by_dep_entry *e = ensure_ruleid_by_dep(depidx);
  uint32_t hash = abce_murmur32(HASH_SEED, ruleid);
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
    my_abort();
  }
  return;
}

void better_cycle_detect_impl(int cur, unsigned char *no_cycles, unsigned char *parents, int mark_traversed)
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
          fprintf(stderr, " %s", sttable[e->tgtidx].s);
        }
        fprintf(stderr, " )\n");
      }
    }
    errxit("cycle found, cannot proceed further");
    exit(2);
  }
  parents[cur] = 1;
  LINKED_LIST_FOR_EACH(node, &rules[cur]->deplist)
  {
    struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
    int ruleid = get_ruleid_by_tgt(e->nameidx);
    if (ruleid >= 0)
    {
      better_cycle_detect_impl(ruleid, no_cycles, parents, mark_traversed);
    }
  }
  parents[cur] = 0;
  no_cycles[cur] = 1;
  if (mark_traversed)
  {
    rules[cur]->is_traversed = 1;
  }
}

unsigned char *better_cycle_detect(int cur, int mark_traversed)
{
  unsigned char *no_cycles, *parents;
  no_cycles = malloc(rules_size);
  parents = malloc(rules_size);

  memset(no_cycles, 0, rules_size);
  memset(parents, 0, rules_size);

  better_cycle_detect_impl(cur, no_cycles, parents, mark_traversed);
  free(parents);
  return no_cycles;
}

struct add_dep {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  size_t depidx;
  size_t depidxnodir;
  unsigned auto_phony:1;
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

struct add_dep *add_dep_ensure(struct add_deps *entry, size_t depidx, size_t depidxnodir)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  hashval = abce_murmur32(HASH_SEED, depidx);
  hashloc = hashval % (sizeof(entry->add_deps)/sizeof(*entry->add_deps));
  n = ABCE_RB_TREE_NOCMP_FIND(&entry->add_deps[hashloc], add_dep_cmp_asym, NULL, depidx);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct add_dep, node);
  }
  add_dep_cnt++;
  struct add_dep *entry2 = my_malloc(sizeof(struct add_dep));
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

size_t add_deps_cnt;

struct add_deps *add_deps_ensure(size_t tgtidx)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  size_t i;
  hashval = abce_murmur32(HASH_SEED, tgtidx);
  hashloc = hashval % (sizeof(add_deps)/sizeof(*add_deps));
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
    my_abort();
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
      struct add_dep *add;
      add = add_dep_ensure(entry, stringtab_add(deps[j].name), (size_t)-1);
    }
  }
}

void add_dep(char **tgts, size_t tgts_sz,
             char **deps, size_t deps_sz,
             int phony, int auto_phony)
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
      struct add_dep *add;
      add = add_dep_ensure(entry, stringtab_add(deps[j]), (size_t)-1);
      if (auto_phony)
      {
        add->auto_phony = 1;
      }
    }
  }
}

void zero_rule(struct rule *rule)
{
  memset(rule, 0, sizeof(*rule));
  linked_list_head_init(&rule->deplist);
  linked_list_head_init(&rule->primarydeplist);
  linked_list_head_init(&rule->dupedeplist);
  linked_list_head_init(&rule->tgtlist);
  syncbuf_init(&rule->output);
  rule->deps_remain_cnt = 0;
  rule->wait_remain_cnt = 0;
  linked_list_head_init(&rule->depremainlist);
}

char **null_cmds[] = {NULL};

char **argdup(int ignore, int noecho, int ismake, char **cmdargs)
{
  size_t cnt = 0;
  size_t i;
  char **result;
  while (cmdargs[cnt] != NULL)
  {
    cnt++;
  }
  result = my_malloc((cnt+4) * sizeof(*result));
  result[0] = ignore ? st_ignore : st_noignore;
  result[1] = noecho ? st_noecho : st_echo;
  result[2] = ismake ? st_make : st_nomake;
  for (i = 0; i < cnt; i++)
  {
    result[i+3] = my_strdup(cmdargs[i]);
  }
  result[cnt+3] = NULL;
  return result;
}

char ***argsdupcnt(char ***cmdargs, size_t cnt)
{
  size_t i;
  char ***result;
  result = my_malloc((cnt+1) * sizeof(*result));
  for (i = 0; i < cnt; i++)
  {
    result[i] = cmdargs[i] ? argdup(0, 0, 0, cmdargs[i]) : NULL;
  }
  result[cnt] = NULL;
  return result;
}

size_t rule_cnt;

int add_dep_after_parsing_stage(char **tgts, size_t tgtsz,
                                char **deps, size_t depsz,
                                char *prefix,
                                int rec, int orderonly, int wait)
{
  size_t i, j;
  size_t prefixlen = strlen(prefix);
  for (i = 0; i < tgtsz; i++)
  {
    size_t fulltgtsz = strlen(tgts[i]) + prefixlen + 2;
    char *fulltgt;
    char *can;
    size_t tgtidx;
    int ruleid;
    struct rule *rule;
    fulltgt = malloc(fulltgtsz);
    if (snprintf(fulltgt, fulltgtsz, "%s/%s", prefix, tgts[i]) >= fulltgtsz)
    {
      my_abort();
    }
    can = canon(fulltgt);
    tgtidx = stringtab_add(can);
    free(can);
    free(fulltgt);
    ruleid = get_ruleid_by_tgt(tgtidx);
    if (ruleid < 0)
    {
      fprintf(stderr, "stirmake: target %s not found while adding dep\n",
              tgts[i]);
      return -ENOENT;
    }
    rule = rules[ruleid];
    if (rule->is_executed)
    {
      fprintf(stderr, "stirmake: target %s already executed while adding dep\n",
              tgts[i]);
      return -EINVAL;
    }
    if (rule->is_queued)
    {
      fprintf(stderr, "stirmake: target %s already queued while adding dep\n",
              tgts[i]);
      return -EINVAL;
    }
    for (j = 0; j < depsz; j++)
    {
      size_t fulldepsz = strlen(deps[j]) + prefixlen + 2;
      char *fulldep;
      size_t depidx;
      int otherid;

      fulldep = malloc(fulldepsz);
      if (snprintf(fulldep, fulldepsz, "%s/%s", prefix, deps[j]) >= fulldepsz)
      {
        my_abort();
      };
      can = canon(fulldep);
      depidx = stringtab_add(can);
      free(can);
      free(fulldep);

      otherid = get_ruleid_by_tgt(depidx);
      if (otherid < 0)
      {
        fprintf(stderr, "stirmake: dep %s not found while adding dep\n",
                deps[j]);
        return -ENOENT;
      }
      ins_dep(rule, depidx, rule->diridx, (size_t)-1, rec, orderonly, wait, 0);
      deps_remain_insert(rule, otherid);
      ins_ruleid_by_dep(depidx, ruleid);
    }
  }
  return 0;
}

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
      rule->diridx = stringtab_add("."); // FIXME any ill side effects?

      rule->scopeidx = global_scopeidx;
      rule->ruleid = rules_size++;
      ins_ruleid_by_tgt(entry->tgtidx, rule->ruleid);
      ins_tgt(rule, entry->tgtidx, (size_t)-1, 0);
      LINKED_LIST_FOR_EACH(node2, &entry->add_deplist)
      {
        struct add_dep *dep = ABCE_CONTAINER_OF(node2, struct add_dep, llnode);
        ins_dep(rule, dep->depidx, rule->diridx, (size_t)-1, 0, 0, 0, 0);
      }
      rule->is_phony = !!entry->phony;
      rule->is_rectgt = 0;
      rule->is_detouch = 0;
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
      ins_dep(rule, dep->depidx, rule->diridx, (size_t)-1, 0, 0, 0, 0);
    }
    LINKED_LIST_FOR_EACH(node2, &rule->deplist)
    {
      struct stirdep *dep = ABCE_CONTAINER_OF(node2, struct stirdep, llnode);
      ins_ruleid_by_dep(dep->nameidx, rule->ruleid);
      //printf(" dep: %s\n", dep->name);
    }
  }
  // Auto-phony-adder
#if 1
  LINKED_LIST_FOR_EACH(node, &add_deplist)
  {
    struct add_deps *entry = ABCE_CONTAINER_OF(node, struct add_deps, llnode);
    LINKED_LIST_FOR_EACH(node2, &entry->add_deplist)
    {
      struct add_dep *dep = ABCE_CONTAINER_OF(node2, struct add_dep, llnode);
      struct rule *rule;
      int ruleid = get_ruleid_by_tgt(dep->depidx);
      if (ruleid >= 0)
      {
        continue;
      }
      if (!dep->auto_phony)
      {
        continue;
      }
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
      rule->cmd.args = argsdupcnt(null_cmds, 1);
      rule->is_inc = 1;
      rule->diridx = stringtab_add("."); // FIXME any ill side effects?

      rule->scopeidx = global_scopeidx;
      rule->ruleid = rules_size++;
      ins_ruleid_by_tgt(dep->depidx, rule->ruleid);
      ins_tgt(rule, dep->depidx, (size_t)-1, 0);
      rule->is_phony = 0; // is_inc is enough
      rule->is_rectgt = 0;
      rule->is_detouch = 0;
    }
  }
#endif
}

void add_rule(struct tgt *tgts, size_t tgtsz,
              struct dep *deps, size_t depsz,
              struct cmdsrc *shells,
              int phony, int rectgt, int detouch, int maybe, int dist,
              int cleanhook, int distcleanhook, int bothcleanhook,
              char *prefix, size_t scopeidx)
{
  struct rule *rule;
  size_t i;

  if (tgtsz <= 0)
  {
    errxit("Rules must have at least 1 target");
    exit(2);
  }
  if (phony && tgtsz != 1)
  {
    errxit("Phony rules must not have multiple targets");
    exit(2);
  }
  if (debug)
  {
    print_indent();
    printf("Rule %s (%s): add_rule\n", tgts[0].name, prefix);
  }
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
  rule->cmdsrc = *shells; // FIXME duplicate?
  rule->scopeidx = scopeidx;
  rule->ruleid = rules_size++;
  rule->is_phony = !!phony;
  rule->is_maybe = !!maybe;
  rule->is_rectgt = !!rectgt;
  rule->is_detouch = !!detouch;
  rule->is_dist = !!dist;
  rule->is_cleanhook = !!cleanhook;
  rule->is_distcleanhook = !!distcleanhook;
  rule->is_bothcleanhook = !!bothcleanhook;
  rule->diridx = stringtab_add(prefix);

  for (i = 0; i < tgtsz; i++)
  {
    size_t tgtidx = stringtab_add(tgts[i].name);
    size_t tgtidxnodir = stringtab_add(tgts[i].namenodir);
    ins_tgt(rule, tgtidx, tgtidxnodir, !!tgts[i].is_dist);
    ins_ruleid_by_tgt(tgtidx, rule->ruleid);
  }
  for (i = 0; i < depsz; i++)
  {
    size_t nameidx = stringtab_add(deps[i].name);
    size_t nameidxnodir = stringtab_add(deps[i].namenodir);
    if (ins_dep(rule, nameidx, rule->diridx, nameidxnodir, !!deps[i].rec, !!deps[i].orderonly, !!deps[i].wait, 1) == 0)
    {
      ins_ruleid_by_dep(nameidx, rule->ruleid);
    }
  }
}

int *ruleids_to_run;
size_t ruleids_to_run_size;
size_t ruleids_to_run_capacity;

struct ruleid_by_pid {
  struct abce_rb_tree_node node;
  struct abce_rb_tree_node fdnode;
  struct linked_list_node llnode;
  pid_t pid;
  int ruleid;
  int fd;
};

static inline int ruleid_by_pid_fd_cmp_asym(int fd, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_pid *e = ABCE_CONTAINER_OF(n2, struct ruleid_by_pid, fdnode);
  if (fd < 0 || e->fd < 0)
  {
    my_abort();
  }
  if (fd > e->fd)
  {
    return 1;
  }
  if (fd < e->fd)
  {
    return -1;
  }
  return 0;
}

static inline int ruleid_by_pid_fd_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_pid *e1 = ABCE_CONTAINER_OF(n1, struct ruleid_by_pid, fdnode);
  struct ruleid_by_pid *e2 = ABCE_CONTAINER_OF(n2, struct ruleid_by_pid, fdnode);
  if (e1->fd < 0 || e2->fd < 0)
  {
    my_abort();
  }
  if (e1->fd > e2->fd)
  {
    return 1;
  }
  if (e1->fd < e2->fd)
  {
    return -1;
  }
  return 0;
}

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
struct abce_rb_tree_nocmp ruleid_by_pid_fd[RULEID_BY_PID_SIZE];
struct linked_list_head ruleid_by_pid_list =
  STIR_LINKED_LIST_HEAD_INITER(ruleid_by_pid_list);

int ruleid_by_fd(int fd)
{
  struct abce_rb_tree_node *n;
  uint32_t hashvalfd;
  size_t hashlocfd;
  if (fd < 0)
  {
    abort();
  }
  hashvalfd = abce_murmur32(HASH_SEED, fd);
  hashlocfd = hashvalfd % (sizeof(ruleid_by_pid_fd)/sizeof(*ruleid_by_pid_fd));
  n = ABCE_RB_TREE_NOCMP_FIND(&ruleid_by_pid_fd[hashlocfd], ruleid_by_pid_fd_cmp_asym, NULL, fd);
  if (n == NULL)
  {
    return -ENOENT;
  }
  struct ruleid_by_pid *bypid = ABCE_CONTAINER_OF(n, struct ruleid_by_pid, fdnode);
  return bypid->ruleid;
}

int ruleid_by_pid_erase(pid_t pid, int *fd)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval, hashvalfd;
  size_t hashloc, hashlocfd;
  int ruleid;
  hashval = abce_murmur32(HASH_SEED, pid);
  hashloc = hashval % (sizeof(ruleid_by_pid)/sizeof(*ruleid_by_pid));
  n = ABCE_RB_TREE_NOCMP_FIND(&ruleid_by_pid[hashloc], ruleid_by_pid_cmp_asym, NULL, pid);
  if (n == NULL)
  {
    return -ENOENT;
  }
  struct ruleid_by_pid *bypid = ABCE_CONTAINER_OF(n, struct ruleid_by_pid, node);
  abce_rb_tree_nocmp_delete(&ruleid_by_pid[hashloc], &bypid->node);
  if (bypid->fd >= 0)
  {
    hashvalfd = abce_murmur32(HASH_SEED, bypid->fd);
    hashlocfd = hashvalfd % (sizeof(ruleid_by_pid_fd)/sizeof(*ruleid_by_pid_fd));
    abce_rb_tree_nocmp_delete(&ruleid_by_pid_fd[hashlocfd], &bypid->fdnode);
  }
  ruleid = bypid->ruleid;
  linked_list_delete(&bypid->llnode);
  if (fd)
  {
    *fd = bypid->fd;
  }
  my_free(bypid);
  return ruleid;
}

//std::unordered_map<pid_t, int> ruleid_by_pid;

size_t ruleid_by_pid_cnt;

void print_cmd(const char *tgtname, const char *prefix, char **argiter_orig)
{
  size_t argcnt = 0;
  char **argiter = argiter_orig;
  size_t i;
  size_t tlen = 0;
  char *tbuf = NULL;
  size_t toff = 0;
  int tret;
  while (*argiter != NULL)
  {
    argiter++;
    argcnt++;
  }
  tlen += 1; // "["
  tlen += strlen(tgtname);
  tlen += 2; // ", "
  tlen += strlen(prefix);
  tlen += 2; // "] "
  for (i = 0; i < argcnt; i++)
  {
    tlen += strlen(argiter_orig[i]);
    tlen += 1;
  }
  tlen += 1; // "\n"
  tlen += 1; // "\0"
  /* Observe how we use standard malloc. A custom allocator using MAP_SHARED
   * would not be safe to use in the child process. */
  tbuf = malloc(tlen);
  tret = snprintf(tbuf+toff, tlen-toff, "[%s, %s]", prefix, tgtname);
  if (tret < 0 || (size_t)tret >= tlen - toff)
  {
    _exit(1);
  }
  toff += tret;
  for (i = 0; i < argcnt; i++)
  {
    tret = snprintf(tbuf+toff, tlen-toff, " %s", argiter_orig[i]);
    if (tret < 0 || (size_t)tret >= tlen - toff)
    {
      _exit(1);
    }
    toff += tret;
  }
  tret = snprintf(tbuf+toff, tlen-toff, "\n");
  if (tret < 0 || (size_t)tret >= tlen - toff)
  {
    _exit(1);
  }
  toff += tret;
  /*
   * Why not writev, you ask. Well, writev is not atomic for terminals, at
   * least on Linux. And, writev requires us to allocate memory for the iovec
   * list too.
   */
  write(1, tbuf, toff);
  free(tbuf); // We could let it leak, too...
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

void do_makecmd(int ismake, const char *cmd, int create_fd, int create_make_fd, int outpipewr)
{
  if (ismake || is_makecmd(cmd))
  {
    char env[128] = {0};
    if (create_make_fd)
    {
      snprintf(env, sizeof(env), " --jobserver-fds=%d,%d -j -Orecurse",
               jobserver_fd[0], jobserver_fd[1]);
    }
    else if (create_fd)
    {
      snprintf(env, sizeof(env), " --jobserver-fds=%d,%d -j -Otarget",
               jobserver_fd[0], jobserver_fd[1]);
    }
    else
    {
      snprintf(env, sizeof(env), " --jobserver-fds=%d,%d -j",
               jobserver_fd[0], jobserver_fd[1]);
    }
    setenv("MAKEFLAGS", env, 1);
    if (create_make_fd)
    {
      if (dup2(outpipewr, 1) < 0)
      {
        write(2, "DUP2ERR\n", 8);
        _exit(1);
      }
      if (dup2(outpipewr, 2) < 0)
      {
        write(2, "DUP2ERR\n", 8);
        _exit(1);
      }
      close(outpipewr);
    }
    else if (create_fd)
    {
      close(outpipewr);
    }
  }
  else
  {
    close(jobserver_fd[0]);
    close(jobserver_fd[1]);
    if (create_fd)
    {
      if (dup2(outpipewr, 1) < 0)
      {
        write(2, "DUP2ERR\n", 8);
        _exit(1);
      }
      if (dup2(outpipewr, 2) < 0)
      {
        write(2, "DUP2ERR\n", 8);
        _exit(1);
      }
      close(outpipewr);
    }
  }
}

void child_execvp_wait(int ignore, int noecho, int ismake, const char *tgtname, const char *prefix, const char *cmd, char **args, int create_fd, int create_make_fd, int outpipewr)
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
    do_makecmd(ismake, cmd, create_fd, create_make_fd, outpipewr);
    if (!noecho || dry_run)
    {
      print_cmd(tgtname, prefix, args);
    }
    if (dry_run)
    {
      _exit(0);
    }
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
      if (subproc_sigint_atomic)
      {
        kill(pid, SIGINT);
        signal(SIGINT, SIG_DFL);
        raise(SIGINT);
        _exit(1);
      }
      if (subproc_sigterm_atomic)
      {
        kill(pid, SIGTERM);
        signal(SIGTERM, SIG_DFL);
        raise(SIGTERM);
        _exit(1);
      }
      if (subproc_sighup_atomic)
      {
        kill(pid, SIGHUP);
        signal(SIGHUP, SIG_DFL);
        raise(SIGHUP);
        _exit(1);
      }
    } while (ret == -1 && errno == -EINTR);
    if (ret == -1)
    {
      _exit(1);
    }
    if (!ignoreerr && !WIFEXITED(wstatus))
    {
      _exit(1);
    }
    if (!ignoreerr && WEXITSTATUS(wstatus) != 0)
    {
      if (!ignore)
      {
        _exit(1);
      }
    }
    return;
  }
}

void set_nonblock(int fd);

extern FILE *dbf;

pid_t fork_child(int ruleid, int create_fd, int create_make_fd, int *fdout)
{
  char ***args;
  pid_t pid;
  struct cmd cmd = rules[ruleid]->cmd;
  const char *dir = sttable[rules[ruleid]->diridx].s;
  char ***argiter;
  char **oneargiter;
  size_t argcnt = 0;
  int outpipe[2] = {-1,-1};
  int outpiperd = -1, outpipewr = -1;
  struct stirtgt *first_tgt =
    ABCE_CONTAINER_OF(rules[ruleid]->tgtlist.node.next, struct stirtgt, llnode);

  args = cmd.args;
  argiter = args;

  if (create_fd)
  {
    if (pipe(outpipe) != 0)
    {
      errxit("can't pipe output of command");
      my_abort();
    }
    outpiperd = outpipe[0];
    outpipewr = outpipe[1];
    set_nonblock(outpiperd); // not for outpipewr
    fcntl(outpiperd, F_SETFD, fcntl(outpiperd, F_GETFD) | FD_CLOEXEC);
  }

  if (debug)
  {
    print_indent();
    printf("start args:\n");
  }
  while (*argiter)
  {
    oneargiter = *argiter++;
    if (debug)
    {
      print_indent();
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
    print_indent();
    printf("end args\n");
  }

  argiter = args;

  if (argcnt == 0)
  {
    printf("no arguments\n");
    my_abort();
  }

  pid = fork();
  if (pid < 0)
  {
    errxit("Unable to fork child");
    my_abort();
    exit(2);
  }
  else if (pid == 0)
  {
    struct sigaction sa, saint, saterm, sahup;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL; // SIG_IGN does not allow waitpid()
    sigaction(SIGCHLD, &sa, NULL);
    sigemptyset(&saint.sa_mask);
    saint.sa_flags = 0;
    saint.sa_handler = subproc_sigint_handler;
    sigaction(SIGINT, &saint, NULL);
    sigemptyset(&saterm.sa_mask);
    saterm.sa_flags = 0;
    saterm.sa_handler = subproc_sigterm_handler;
    sigaction(SIGTERM, &saterm, NULL);
    sigemptyset(&sahup.sa_mask);
    sahup.sa_flags = 0;
    sahup.sa_handler = subproc_sighup_handler;
    sigaction(SIGHUP, &sahup, NULL);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGSYS, SIG_DFL);
    signal(SIGXCPU, SIG_DFL);
    signal(SIGXFSZ, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    signal(SIGALRM, SIG_DFL);
    if (chdir(dir) != 0)
    {
      write(1, "CHDIRERR\n", 9);
      _exit(1);
    }
    close(fileno(dbf));
    close(self_pipe_fd[0]);
    close(self_pipe_fd[1]);
    if (create_fd)
    {
      close(outpiperd);
    }
    update_recursive_pid(0);
    while (argcnt > 1)
    {
      child_execvp_wait(strcmp((*argiter)[0], st_ignore) == 0, strcmp((*argiter)[1], st_noecho) == 0, strcmp((*argiter)[2], st_make) == 0, sttable[first_tgt->tgtidx].s, dir, (*argiter)[3], &(*argiter)[3], create_fd, create_make_fd, outpipewr);
      argiter++;
      argcnt--;
    }
    if (strcmp((*argiter)[0], st_ignore) == 0)
    {
      child_execvp_wait(strcmp((*argiter)[0], st_ignore) == 0, strcmp((*argiter)[1], st_noecho) == 0, strcmp((*argiter)[2], st_make) == 0, sttable[first_tgt->tgtidx].s, dir, (*argiter)[3], &(*argiter)[3], create_fd, create_make_fd, outpipewr);
      _exit(0);
    }
    else
    {
      update_recursive_pid(1);
      do_makecmd(strcmp((*argiter)[2], st_make) == 0, (*argiter)[3], create_fd, create_make_fd, outpipewr);
      if (strcmp((*argiter)[1], st_noecho) != 0 || dry_run)
      {
        print_cmd(sttable[first_tgt->tgtidx].s, dir, &(*argiter)[3]);
      }
      if (dry_run)
      {
        _exit(0);
      }
      execvp((*argiter)[3], &(*argiter)[3]);
      //write(1, "Err\n", 4);
      _exit(1);
    }
  }
  else
  {
    ruleid_by_pid_cnt++;
    struct ruleid_by_pid *bypid = my_malloc(sizeof(*bypid)); // RFE use malloc() instead?
    uint32_t hashval;
    size_t hashloc;
    uint32_t hashvalfd;
    size_t hashlocfd;
    bypid->pid = pid;
    bypid->ruleid = ruleid;
    bypid->fd = outpiperd;
    children++;
    if (create_fd)
    {
      close(outpipewr);
    }
    hashval = abce_murmur32(HASH_SEED, pid);
    hashloc = hashval % (sizeof(ruleid_by_pid)/sizeof(*ruleid_by_pid));
    if (abce_rb_tree_nocmp_insert_nonexist(&ruleid_by_pid[hashloc], ruleid_by_pid_cmp_sym, NULL, &bypid->node) != 0)
    {
      printf("12\n");
      my_abort();
    }
    if (bypid->fd >= 0)
    {
      hashvalfd = abce_murmur32(HASH_SEED, bypid->fd);
      hashlocfd = hashvalfd % (sizeof(ruleid_by_pid_fd)/sizeof(*ruleid_by_pid_fd));
      if (abce_rb_tree_nocmp_insert_nonexist(&ruleid_by_pid_fd[hashlocfd], ruleid_by_pid_fd_cmp_sym, NULL, &bypid->fdnode) != 0)
      {
        printf("12.5\n");
        my_abort();
      }
    }
    linked_list_add_tail(&bypid->llnode, &ruleid_by_pid_list);
    rules[ruleid]->is_forked = 1;
    if (fdout)
    {
      *fdout = bypid->fd;
    }
    return pid;
  }
}

struct stathashentry {
  struct abce_rb_tree_node node;
  struct linked_list_node llnode;
  size_t nameidx;
  int ret;
  mode_t st_mode;
  struct timespec st_mtim;
};
struct abce_rb_tree_nocmp stathash[STATHASH_SIZE];
struct stathashentry stathashentries[STATHASH_SIZE];
struct linked_list_head statlrulist =
  STIR_LINKED_LIST_HEAD_INITER(statlrulist);
struct linked_list_head statfreelist =
  STIR_LINKED_LIST_HEAD_INITER(statfreelist);

void statcache_init(void)
{
  size_t i;
  for (i = 0; i < sizeof(stathashentries)/sizeof(*stathashentries); i++)
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
  stathashentry_evict();
  if (linked_list_is_empty(&statfreelist))
  {
    abort();
  }
}

static inline int stathashentry_cmp_asym(size_t str, struct abce_rb_tree_node *n2, void *ud)
{
  struct stathashentry *e = ABCE_CONTAINER_OF(n2, struct stathashentry, node);
  int ret;
  size_t str2;
  str2 = e->nameidx;
  ret = sizecmp(str, str2);
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
void lstat_evict_named(size_t nameidx)
{
  struct stathashentry *e;
  uint32_t hash;
  struct abce_rb_tree_nocmp *head;
  struct abce_rb_tree_node *n;

  hash = abce_murmur32(HASH_SEED, nameidx);
  head = &stathash[hash % (sizeof(stathash)/sizeof(*stathash))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, stathashentry_cmp_asym, NULL, nameidx);
  if (n == NULL)
  {
    return;
  }
  e = ABCE_CONTAINER_OF(n, struct stathashentry, node);
  linked_list_delete(&e->llnode);
  abce_rb_tree_nocmp_delete(head, &e->node);
  linked_list_add_head(&e->llnode, &statfreelist);
}
struct stathashentry *lstat_cached(size_t nameidx)
{
  struct abce_rb_tree_node *n;
  struct stathashentry *e;
  uint32_t hash;
  struct abce_rb_tree_nocmp *head;
  int ret;
  struct stat statbuf;
  hash = abce_murmur32(HASH_SEED, nameidx);
  head = &stathash[hash % (sizeof(stathash)/sizeof(*stathash))];
  n = ABCE_RB_TREE_NOCMP_FIND(head, stathashentry_cmp_asym, NULL, nameidx);
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
  }
  else
  {
    e->st_mode = 0;
    e->st_mtim.tv_sec = 0;
    e->st_mtim.tv_nsec = 0;
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
    exit(2);
  }
  max = statbuf.st_mtim;
  if (lstat(name, &statbuf) != 0)
  {
    errxit("can't open file %s", name);
    exit(2);
  }
  if (ts_cmp(statbuf.st_mtim, max) > 0)
  {
    max = statbuf.st_mtim;
  }
  if (dir == NULL)
  {
    errxit("can't open dir %s", name);
    exit(2);
  }
  for (;;)
  {
    struct dirent *de = readdir(dir);
    struct timespec cur;
    char nam2[PATH_MAX + 1] = {0}; // RFE avoid large static recursive allocs?
    //std::string nam2(name);
    if (snprintf(nam2, sizeof(nam2), "%s", name) >= sizeof(nam2))
    {
      errxit("Pathname too long");
      my_abort();
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
      errxit("Pathname too long");
      my_abort();
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
        exit(2);
      }
      cur = statbuf.st_mtim;
      if (lstat(nam2, &statbuf) != 0)
      {
        errxit("can't open file %s", nam2);
        exit(2);
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

int utimensat_both_emul(const char *pathname, struct timespec time, int l,
                        int forwards)
{
  struct timespec timespecs[2];
  struct timeval times[2];
  timespecs[0] = time;
  timespecs[1] = time;
  times[0].tv_sec = time.tv_sec;
  times[0].tv_usec = (time.tv_nsec+999*(!!forwards))/1000;
  times[1].tv_sec = time.tv_sec;
  times[1].tv_usec = (time.tv_nsec+999*(!!forwards))/1000;
#ifdef HAS_UTIMENSAT
  return utimensat(AT_FDCWD, pathname, timespecs, l ? AT_SYMLINK_NOFOLLOW : 0);
#else
  {
    struct timespec req;
    struct timespec rem;
    utimeret = utimes(pathname, times); // Ugh. Can't change symlink time!
    if (forwards)
    {
      req.tv_sec = 0;
      req.tv_nsec = 2000; // let's sleep for 2 us to be rather safe than sorry
      for (;;)
      {
        int ret = nanosleep(&req, &rem);
        if (ret == 0 || (ret != 0 && errno != EINTR))
        {
          break;
        }
        req = rem;
      }
    }
  }
#endif
}

void reccap_mtim(const char *name, struct timespec cap)
{
  struct stat statbuf;
  DIR *dir = opendir(name);
  //printf("Statting %s\n", name);
  if (dir == NULL)
  {
    errxit("can't open dir %s", name);
    exit(2);
  }
  for (;;)
  {
    struct dirent *de = readdir(dir);
    char nam2[PATH_MAX + 1] = {0}; // RFE avoid large static recursive allocs?
    //std::string nam2(name);
    if (snprintf(nam2, sizeof(nam2), "%s", name) >= sizeof(nam2))
    {
      errxit("Pathname too long");
      my_abort();
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
      errxit("Pathname too long");
      my_abort();
    }
    //if (de->d_type == DT_DIR)
    if (0)
    {
      reccap_mtim(nam2, cap);
    }
    else
    {
      if (stat(nam2, &statbuf) != 0)
      {
        errxit("can't open file %s", nam2);
        exit(2);
      }
      if (ts_cmp(statbuf.st_mtim, cap) > 0)
      {
        utimensat_both_emul(nam2, cap, 0, 0);
      }
      if (lstat(nam2, &statbuf) != 0)
      {
        errxit("can't open file %s", nam2);
        exit(2);
      }
      if (ts_cmp(statbuf.st_mtim, cap) > 0)
      {
        utimensat_both_emul(nam2, cap, 1, 0);
      }
    }
#if 0
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
#endif
    if ((statbuf.st_mode & S_IFMT) == S_IFDIR)
    {
      reccap_mtim(nam2, cap);
    }
  }
  closedir(dir);

  if (stat(name, &statbuf) != 0)
  {
    errxit("can't open file %s", name);
    exit(2);
  }
  if (ts_cmp(statbuf.st_mtim, cap) > 0)
  {
    utimensat_both_emul(name, cap, 0, 0);
  }
  if (lstat(name, &statbuf) != 0)
  {
    errxit("can't open file %s", name);
    exit(2);
  }
  if (ts_cmp(statbuf.st_mtim, cap) > 0)
  {
    utimensat_both_emul(name, cap, 1, 0);
  }
}

void calc_cmd(int ruleid)
{
  struct rule *r = rules[ruleid];
  struct stirtgt *first_tgt =
    ABCE_CONTAINER_OF(r->tgtlist.node.next, struct stirtgt, llnode);
  if (r->cmd.args != NULL)
  {
    return;
  }
  r->cmd.args = cmdsrc_eval(&abce, r);
  if (r->cmd.args == NULL)
  {
    errxit("evaluating shell commands for %s failed",
           sttable[first_tgt->tgtidx].s);
  }
}

int do_exec(int ruleid)
{
  struct rule *r = rules[ruleid];
  struct stirtgt *first_tgt =
    ABCE_CONTAINER_OF(r->tgtlist.node.next, struct stirtgt, llnode);
  //Rule &r = rules.at(ruleid);
  if (debug)
  {
    print_indent();
    printf("do_exec %s\n", sttable[first_tgt->tgtidx].s);
  }
  if (!r->is_queued)
  {
    int has_to_exec = 0;
    if (unconditional)
    {
      has_to_exec = 1;
    }
    calc_cmd(ruleid);
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
        if (ispretend(sttable[e->nameidx].s) || sttable[e->nameidx].is_remade)
        {
          has_to_exec = 1;
        }
        if (depid >= 0)
        {
          if (rules[depid]->is_phony)
          {
            if (debug)
            {
              print_indent();
              printf("rule %d/%s is phony\n", depid, sttable[e->nameidx].s);
            }
            has_to_exec = 1;
            continue;
          }
          if (debug)
          {
            print_indent();
            printf("ruleid %d/%s not phony\n", depid, sttable[e->nameidx].s);
          }
        }
        else
        {
          if (debug)
          {
            print_indent();
            printf("ruleid for tgt %s not found\n", sttable[e->nameidx].s);
          }
        }
        if (e->is_recursive)
        {
          struct timespec st_rectim = rec_mtim(r, sttable[e->nameidx].s);
          if (!seen_nonphony || ts_cmp(st_rectim, st_mtim) > 0)
          {
            st_mtim = st_rectim;
          }
          seen_nonphony = 1;
          continue;
        }
        int recommended = 0;
        struct stathashentry *she;
        she = lstat_cached(e->nameidx);
        if (she->ret != 0)
        {
          has_to_exec = 1;
          // break; // No break, we want to get accurate st_mtim
          continue;
          //perror("can't lstat");
          //fprintf(stderr, "file was: %s\n", it->c_str());
          //my_abort();
        }
        if (S_ISDIR(she->st_mode) && !e->is_orderonly && !recommended)
        {
          char *tgtname = sttable[ABCE_CONTAINER_OF(r->tgtlist.node.next, struct stirtgt, llnode)->tgtidx].s;
          printf("stirmake: Recommend making directory dep %s of %s either @orderonly or @recdep.\n", sttable[e->nameidx].s, tgtname);
          recommended = 1;
        }
        if (!e->is_orderonly)
        {
          if (debug)
          {
            print_indent();
            printf("dep: %llu %llu\n", (unsigned long long)she->st_mtim.tv_sec, (unsigned long long)she->st_mtim.tv_nsec);
          }
          if (!seen_nonphony || ts_cmp(she->st_mtim, st_mtim) > 0)
          {
            st_mtim = she->st_mtim;
          }
          seen_nonphony = 1;
        }
        if (!S_ISLNK(she->st_mode))
        {
          continue;
        }
        if (stat(sttable[e->nameidx].s, &statbuf) != 0)
        {
          has_to_exec = 1;
          // break; // No break, we want to get accurate st_mtim
          continue;
          //perror("can't stat");
          //fprintf(stderr, "file was: %s\n", it->c_str());
          //my_abort();
        }
        if (S_ISDIR(statbuf.st_mode) && !e->is_orderonly && !recommended)
        {
          char *tgtname = sttable[ABCE_CONTAINER_OF(r->tgtlist.node.next, struct stirtgt, llnode)->tgtidx].s;
          printf("stirmake: Recommend making directory dep %s of %s either @orderonly or @recdep.\n", sttable[e->nameidx].s, tgtname);
          recommended = 1;
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
        if (!cmdequal_db(&db, e->tgtidx, &r->cmd, r->diridx))
        {
          has_to_exec = 1;
        }
        if (debug)
        {
          print_indent();
          printf("statting %s\n", sttable[e->tgtidx].s);
        }
        struct stathashentry *she;
        she = lstat_cached(e->tgtidx);
        if (she->ret != 0)
        {
          if (debug)
          {
            print_indent();
            printf("immediate has_to_exec\n");
          }
          has_to_exec = 1;
          //break; // can't break, has to compare all commands from DB
          continue;
        }
        if (debug)
        {
          print_indent();
          printf("tgt: %llu %llu\n", (unsigned long long)she->st_mtim.tv_sec, (unsigned long long)she->st_mtim.tv_nsec);
        }
        if (!seen_tgt || ts_cmp(she->st_mtim, st_mtimtgt) < 0)
        {
          st_mtimtgt = she->st_mtim;
        }
        seen_tgt = 1;
      }
      if (!has_to_exec)
      {
        if (!seen_tgt)
        {
          printf("15\n");
          my_abort(); // shouldn't happen if there's at least 1 tgt
        }
        if (seen_nonphony && ts_cmp(st_mtimtgt, st_mtim) < 0)
        {
          if (debug)
          {
            print_indent();
            printf("delayed has_to_exec\n");
          }
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
        if (lstat(sttable[e->tgtidx].s, &statbuf) != 0)
        {
          has_to_exec = 1;
          break;
        }
        if (!cmdequal_db(&db, e->tgtidx, &r->cmd, r->diridx))
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
        print_indent();
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
        print_indent();
        printf("do_exec: mark_executed %s has_to_exec %d\n",
               sttable[first_tgt->tgtidx].s, has_to_exec);
      }
      r->is_queued = 1;
      indentlevel++;
      mark_executed(ruleid, 0);
      indentlevel--;
      return 1;
    }
  }
  else
  {
    if (debug)
    {
      print_indent();
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
    print_indent();
    printf("considering %s\n", sttable[first_tgt->tgtidx].s);
  }
  if (r->is_executed)
  {
    if (debug)
    {
      print_indent();
      printf("already execed %s\n", sttable[first_tgt->tgtidx].s);
    }
    return 0;
  }
  if (r->is_executing)
  {
    if (debug)
    {
      print_indent();
      printf("already execing %s\n", sttable[first_tgt->tgtidx].s);
    }
    return 0;
  }
  r->is_executing = 1;
  r->is_under_consideration = 1;
  LINKED_LIST_FOR_EACH(node, &r->deplist)
  {
    struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
    int idbytgt = get_ruleid_by_tgt(e->nameidx);
    r->waitloc = e;
    if (r->wait_remain_cnt > 0 && e->is_wait)
    {
      break;
    }
    if (idbytgt >= 0)
    {
      indentlevel++;
      if (consider(idbytgt))
      {
        execed_some = 1;
      }
      indentlevel--;
      //execed_some = execed_some || consider(idbytgt); // BAD! DON'T DO THIS!
      if (!rules[idbytgt]->is_executed)
      {
        if (debug)
        {
          print_indent();
          printf("rule %d not executed, executing rule %d\n", idbytgt, ruleid);
          //std::cout << "rule " << ruleid_by_tgt[it->name] << " not executed, executing rule " << ruleid << std::endl;
        }
        toexecute = 1;
        deps_remain_forwait(r, idbytgt);
        r->wait_remain_cnt++;
      }
    }
    else
    {
      if (debug)
      {
        print_indent();
        printf("ruleid by target %s not found\n", sttable[e->nameidx].s);
      }
      if (access(sttable[e->nameidx].s, F_OK) == -1)
      {
        errxit("No %s and rule not found", sttable[e->nameidx].s);
        exit(2);
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
    indentlevel++;
    int ret = do_exec(ruleid);
    indentlevel--;
    r->is_under_consideration = 0;
    return ret;
    //ruleids_to_run.push_back(ruleid);
    //r.queued = true;
  }
  r->is_under_consideration = 0;
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
  struct linked_list_node *node;
  if (debug)
  {
    print_indent();
    printf("reconsidering %s\n", sttable[first_tgt->tgtidx].s);
  }
  if (r->is_executed)
  {
    if (debug)
    {
      print_indent();
      printf("already execed %s\n", sttable[first_tgt->tgtidx].s);
    }
    return;
  }
  if (!r->is_executing || r->is_under_consideration)
  {
    if (debug)
    {
      print_indent();
      printf("rule not executing or is under consideration %s\n", sttable[first_tgt->tgtidx].s);
    }
    // Must do this always in case the rule is to be executed in future.
    deps_remain_erase(r, ruleid_executed);
    return;
  }
  deps_remain_erase(r, ruleid_executed);
  //if (!linked_list_is_empty(&r->depremainlist))
  if (r->deps_remain_cnt > 0)
  {
    toexecute = 1;
    if (debug)
    {
      print_indent();
      printf("deps remain: %zu\n", r->deps_remain_cnt);
      LINKED_LIST_FOR_EACH(node, &r->depremainlist)
      {
        struct dep_remain *rem =
          ABCE_CONTAINER_OF(node, struct dep_remain, llnode);
        struct stirtgt *first_tgt =
          ABCE_CONTAINER_OF(rules[rem->ruleid]->tgtlist.node.next, struct stirtgt, llnode);
        print_indent();
        printf("  dep_remain: %d / %s\n", rem->ruleid, sttable[first_tgt->tgtidx].s);
      }
    }
  }
  int toexecute2 = 0;
  r->is_under_consideration = 1;
  for (node = &r->waitloc->llnode; node != &r->deplist.node; node = node->next)
  {
    struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
    int idbytgt = get_ruleid_by_tgt(e->nameidx);
    r->waitloc = e;
    if (r->wait_remain_cnt > 0 && e->is_wait)
    {
      break;
    }
    if (idbytgt >= 0)
    {
      indentlevel++;
      if (consider(idbytgt))
      {
        //execed_some = 1;
      }
      indentlevel--;
      //execed_some = execed_some || consider(idbytgt); // BAD! DON'T DO THIS!
      if (!rules[idbytgt]->is_executed)
      {
        if (debug)
        {
          print_indent();
          printf("rule %d not executed, executing rule %d\n", idbytgt, ruleid);
          //std::cout << "rule " << ruleid_by_tgt[it->name] << " not executed, executing rule " << ruleid << std::endl;
        }
        toexecute2 = 1;
        deps_remain_forwait(r, idbytgt);
        r->wait_remain_cnt++;
      }
    }
    else
    {
      if (debug)
      {
        print_indent();
        printf("ruleid by target %s not found\n", sttable[e->nameidx].s);
      }
      if (access(sttable[e->nameidx].s, F_OK) == -1)
      {
        errxit("No %s and rule not found", sttable[e->nameidx].s);
        exit(2);
      }
    }
  }
  if (!toexecute && !r->is_queued)
  {
    indentlevel++;
    do_exec(ruleid);
    indentlevel--;
    //ruleids_to_run.push_back(ruleid);
    //r.queued = true;
  }
  r->is_under_consideration = 0;
}

void mark_executed(int ruleid, int was_actually_executed)
{
  struct rule *r = rules[ruleid];
  struct linked_list_node *node, *node2;
  if (r->is_executed)
  {
    printf("16\n");
    my_abort();
  }
  if (!r->is_executing)
  {
    printf("17\n");
    my_abort();
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
  if (r->is_detouch && !dry_run)
  {
    struct timespec cap;
    int cap_valid = 0;
    LINKED_LIST_FOR_EACH(node, &r->tgtlist)
    {
      struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
      struct stat statbuf;
      if (stat(sttable[e->tgtidx].s, &statbuf) != 0)
      {
        errxit("Can't stat %s", sttable[e->tgtidx].s);
      }
      if (!cap_valid || ts_cmp(statbuf.st_mtim, cap) < 0)
      {
        cap = statbuf.st_mtim;
        cap_valid = 1;
      }
      if (lstat(sttable[e->tgtidx].s, &statbuf) != 0)
      {
        errxit("Can't lstat %s", sttable[e->tgtidx].s);
      }
      if (!cap_valid || ts_cmp(statbuf.st_mtim, cap) < 0)
      {
        cap = statbuf.st_mtim;
        cap_valid = 1;
      }
    }
    if (!cap_valid)
    {
      my_abort();
    }
    LINKED_LIST_FOR_EACH(node, &r->deplist)
    {
      struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
      if (e->is_recursive)
      {
        reccap_mtim(sttable[e->nameidx].s, cap);
      }
    }
  }
  else if (r->is_rectgt && r->st_mtim_valid && !dry_run)
  {
    LINKED_LIST_FOR_EACH(node, &r->deplist)
    {
      struct stirdep *e = ABCE_CONTAINER_OF(node, struct stirdep, llnode);
      if (e->is_recursive)
      {
        struct timespec st_mtim2 = rec_mtim(r, sttable[e->nameidx].s);
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
      struct stat statbuf;
      if (stat(sttable[e->tgtidx].s, &statbuf) != 0)
      {
        errxit("Can't stat %s", sttable[e->tgtidx].s);
      }
      if (ts_cmp(r->st_mtim, statbuf.st_mtim) <= 0)
      {
        if (debug)
        {
          print_indent();
          printf("utime %s won't move clock backwards!\n", sttable[e->tgtidx].s);
        }
        continue;
      }
      timespecs[0] = r->st_mtim;
      timespecs[1] = r->st_mtim;
      times[0].tv_sec = r->st_mtim.tv_sec;
      times[0].tv_usec = (r->st_mtim.tv_nsec+999)/1000;
      times[1].tv_sec = r->st_mtim.tv_sec;
      times[1].tv_usec = (r->st_mtim.tv_nsec+999)/1000;
#ifdef HAS_UTIMENSAT
      utimeret = utimensat(AT_FDCWD, sttable[e->tgtidx].s, timespecs, 0);
#else
      {
        struct timespec req;
        struct timespec rem;
        utimeret = utimes(sttable[e->tgtidx].s, times);
        req.tv_sec = 0;
        req.tv_nsec = 2000; // let's sleep for 2 us to be rather safe than sorry
        for (;;)
        {
          int ret = nanosleep(&req, &rem);
          if (ret == 0 || (ret != 0 && errno != EINTR))
          {
            break;
          }
          req = rem;
        }
      }
#endif
      if (debug)
      {
        print_indent();
        printf("utime %s succeeded? %d\n", sttable[e->tgtidx].s, (utimeret == 0));
      }
    }
  }
  else if (!r->is_phony && !r->is_maybe && !r->is_inc && !dry_run)
  {
    struct stat statbuf;
    LINKED_LIST_FOR_EACH(node, &r->tgtlist)
    {
      struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
      if (lstat(sttable[e->tgtidx].s, &statbuf) != 0)
      {
        fprintf(stderr, "stirmake: *** Target %s was not created by rule.\n",
               sttable[e->tgtidx].s);
        fprintf(stderr, "stirmake: *** Hint: use @phonyrule for phony rules.\n");
        fprintf(stderr, "stirmake: *** Hint: use @mayberule for rules that may or may not update target.\n");
        fprintf(stderr, "stirmake: *** Hint: use @rectgtrule for rules that have targets inside @recdep.\n");
        errxit("Target %s was not created by rule", sttable[e->tgtidx].s);
      }
      if (r->st_mtim_valid && ts_cmp(statbuf.st_mtim, r->st_mtim) < 0)
      {
        fprintf(stderr, "stirmake: *** Target %s was not updated by rule.\n",
               sttable[e->tgtidx].s);
        fprintf(stderr, "stirmake: *** Hint: use @phonyrule for phony rules.\n");
        fprintf(stderr, "stirmake: *** Hint: use @mayberule for rules that may or may not update target.\n");
        fprintf(stderr, "stirmake: *** Hint: use @rectgtrule for rules that have targets inside @recdep.\n");
        errxit("Target %s was not updated by rule", sttable[e->tgtidx].s);
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
    lstat_evict_named(e->tgtidx);
    if (dry_run && was_actually_executed)
    {
      sttable[e->tgtidx].is_remade = 1;
    }
  }
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
      indentlevel++;
      reconsider(one->ruleid, ruleid);
      indentlevel--;
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
    my_abort();
  }
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0)
  {
    printf("19\n");
    my_abort();
  }
}

void sigsegv_handler(int x)
{
  my_abort();
}
void sigabrt_handler(int x)
{
  my_abort();
}
void sigfpe_handler(int x)
{
  my_abort();
}
void sigill_handler(int x)
{
  my_abort();
}
void sigquit_handler(int x)
{
  my_abort();
}
void sigsys_handler(int x)
{
  my_abort();
}
void sigxcpu_handler(int x)
{
  my_abort();
}
void sigxfsz_handler(int x)
{
  my_abort();
}
void sigbus_handler(int x)
{
  my_abort();
}
void sigchld_handler(int x)
{
  write(self_pipe_fd[1], ".", 1);
}

void sigterm_handler(int x)
{
  sigterm_atomic = 1;
  write(self_pipe_fd[1], ".", 1);
}
void sigint_handler(int x)
{
  sigint_atomic = 1;
  write(self_pipe_fd[1], ".", 1);
}
void sighup_handler(int x)
{
  sighup_atomic = 1;
  write(self_pipe_fd[1], ".", 1);
}
void sigalrm_handler(int x)
{
}
void handle_signal(int signum);

char *myitoa(int i)
{
  char *res = my_malloc(16);
  snprintf(res, 16, "%d", i);
  return res;
}

#if 0
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
#endif

void stack_conf(void)
{
  const rlim_t stackSize = 16 * 1024 * 1024;
  struct rlimit rl;
  int result;
  result = getrlimit(RLIMIT_STACK, &rl);
  if (result != 0)
  {
    printf("20\n");
    my_abort();
  }
  if (rl.rlim_cur < stackSize)
  {
    rl.rlim_cur = stackSize;
    result = setrlimit(RLIMIT_STACK, &rl);
    if (result != 0)
    {
      printf("21\n");
      my_abort();
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
    fprintf(stderr, "%s [-vdcbq] [-j jobcnt] [target...]\n", argv0);
    fprintf(stderr, "  You can start %s as smka, smkt or smkp or use main command stirmake\n", argv0);
    fprintf(stderr, "  smka, smkt and smkp do not take -t | -p | -a whereas stirmake takes\n");
    fprintf(stderr, "  smka, smkt and smkp do not take -f Stirfile whereas stirmake takes\n");
    fprintf(stderr, "  -b and -c options are incompatible with -q\n");
  }
  else
  {
    fprintf(stderr, "%s [-vdcbq] [-j jobcnt] -f Stirfile | -t | -p | -a [target...]\n", argv0);
    fprintf(stderr, "  You can start %s as smka, smkt or smkp or use main command %s\n", argv0, argv0);
    fprintf(stderr, "  smka, smkt and smkp do not take -t | -p | -a whereas %s takes\n", argv0);
    fprintf(stderr, "  smka, smkt and smkp do not take -f Stirfile whereas %s takes\n", argv0);
    fprintf(stderr, "  -b and -c options are incompatible with -q\n");
  }
  exit(2);
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
  fflush(stdout);
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
      exit(2);
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

void process_mflags(char **fds, char **outputsync)
{
  char *iter = getenv("MAKEFLAGS");
  if (fds)
  {
    *fds = NULL;
  }
  if (outputsync)
  {
    *outputsync = NULL;
  }
  while (iter && *iter == ' ')
  {
    iter++;
  }
  // RFE is this 100% accurate?
  while (iter && *iter != '\0')
  {
    if (strncmp(iter, "-O", strlen("-O")) == 0)
    {
      iter += strlen("-O");
      while (iter && *iter == ' ')
      {
        iter++;
      }
      if (outputsync)
      {
        *outputsync = iter;
      }
    }
    if (strncmp(iter, "--jobserver-fds=", strlen("--jobserver-fds=")) == 0)
    {
      iter += strlen("--jobserver-fds=");
      if (fds)
      {
        *fds = iter;
      }
    }
    iter = strchr(iter, ' ');
    while (iter && *iter == ' ')
    {
      iter++;
    }
  }
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
  char *fdstr;
  process_mflags(&fdstr, NULL);
  if (fdstr == NULL)
  {
    return -ENOENT;
  }
  if (sscanf(fdstr, "%d,%d", &fds[0], &fds[1]) != 2)
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
    my_abort();
  }
  canonized = canon(uncanonized);
  free(uncanonized);
  return canonized;
}

void run_loop(void);

int deps_remain_calculated = 0;

void do_clean(char *fwd_path, int objs, int bins)
{
  size_t i, fp_len;
  int all;
  char *cleanstr = NULL;
  struct linked_list_node *node, *node2, *node3, *nodetmp;
  all = (strcmp(fwd_path, ".") == 0);
  fp_len = strlen(fwd_path);
  if (bins && objs)
  {
    cleanstr = "BOTHCLEAN";
  }
  else if (bins)
  {
    cleanstr = "DISTCLEAN";
  }
  else if (objs)
  {
    cleanstr = "CLEAN";
  }
  if (!cleanstr)
  {
    my_abort();
  }
  for (i = 0; i < rules_size; i++)
  {
    int doit = all;
    char *prefix = sttable[rules[i]->diridx].s;
    if (strncmp(prefix, fwd_path, fp_len) == 0)
    {
      if (prefix[fp_len] == '/' || prefix[fp_len] == '\0')
      {
        doit = 1;
      }
    }
    if (!doit)
    {
      continue;
    }
    doit = 0;
    if (objs && !bins)
    {
      if (rules[i]->is_cleanhook)
      {
        doit = 1;
      }
    }
    else if (bins && !objs)
    {
      if (rules[i]->is_distcleanhook)
      {
        doit = 1;
      }
    }
    else if (objs && bins)
    {
      if (rules[i]->is_bothcleanhook)
      {
        doit = 1;
      }
    }
    if (!doit)
    {
      continue;
    }

    size_t parentsz = strlen(prefix) + strlen(cleanstr) + 5;
    char *parent = malloc(parentsz);
    size_t cleanslashsz = strlen(cleanstr) + 4;
    char *cleanslash = malloc(cleanslashsz);
    if (snprintf(cleanslash, cleanslashsz, "%s///", cleanstr) >= cleanslashsz)
    {
      my_abort();
    }
    if (snprintf(parent, parentsz, "%s/../%s", prefix, cleanstr) >= parentsz)
    {
      my_abort();
    }
    char *cparent = canon(parent);
    free(parent);
    parentsz = strlen(cparent) + 4;
    parent = malloc(parentsz);
    if (snprintf(parent, parentsz, "%s///", cparent) >= parentsz)
    {
      my_abort();
    }
    free(cparent);

    if (strncmp(parent, "../", 3) != 0)
    {
      size_t tgtidx = ABCE_CONTAINER_OF(rules[i]->tgtlist.node.next, struct stirtgt, llnode)->tgtidx;
      // FIXME!!! The path to child is incorrect!
      add_dep(&parent, 1, &sttable[tgtidx].s, 1, /*&cleanslash,*/ 0, 0);
    }

    free(parent);
    free(cleanslash);

    ruleremain_add(rules[i]);
  }

  // XXX: this is bad, the same calc_deps_remain code is in two places
  if (!deps_remain_calculated)
  {
    process_additional_deps(abce.dynscope.u.area->u.sc.locidx);
    for (i = 0; i < rules_size; i++)
    {
      calc_deps_remain(rules[i]);
    }
    deps_remain_calculated = 1;
  }

  run_loop();
  for (i = 0; i < rules_size; i++)
  {
    int doit = all;
    char *prefix = sttable[rules[i]->diridx].s;
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
      char *oldname = strdup(sttable[tgt->tgtidx].s);
      char *name;
      size_t stidx;
      int ruleid;
      struct linked_list_head tmplist; // extra list for reversing
      int prefixok;
      char *rprefix;
      linked_list_head_init(&tmplist);
      if (debug)
      {
        print_indent();
        printf("itering tgt %s\n", oldname);
      }
      for (;;)
      {
        if (debug)
        {
          print_indent();
          printf("itering dir %s\n", oldname);
        }
        name = dir_up(oldname);
        if (debug)
        {
          print_indent();
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
        rprefix = sttable[rules[ruleid]->diridx].s;
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
          print_indent();
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
          my_abort();
        }
      }
    }
    rules[i]->is_cleanqueued = 1;
    linked_list_add_head(&rules[i]->cleanllnode, &cleanlist);
  }
  LINKED_LIST_FOR_EACH(node, &cleanlist)
  {
    struct rule *rule = ABCE_CONTAINER_OF(node, struct rule, cleanllnode);
    int alldist = rule->is_dist;
    LINKED_LIST_FOR_EACH(node2, &rule->tgtlist)
    {
      struct stirtgt *tgt = ABCE_CONTAINER_OF(node2, struct stirtgt, llnode);
      int dist = tgt->is_dist || alldist;
      if ((objs && !dist) || (bins && dist))
      {
        char *name = sttable[tgt->tgtidx].s;
        struct stat statbuf;
        int ret = 0;
        ret = lstat(name, &statbuf);
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
  stathashentry_evict_all();
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

FILE *dbf = NULL;

void load_db(void)
{
  struct dbyy dbyy = {};
  size_t i;
  struct flock fl = {};
  int ret;
  linked_list_head_init(&db.ll);
  //dbyynameparse(".stir.db", &dbyy, 0);
  dbf = fopen(".stir.db", "a+");
  if (dbf == NULL)
  {
    fprintf(stderr, "stirmake: *** Can't open DB. Exiting.\n");
    exit(2);
  }
  int dbfd = fileno(dbf);
  if (dbfd < 0)
  {
    fprintf(stderr, "stirmake: *** Can't get DB fileno. Exiting.\n");
    exit(2);
  }
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0; // XXX or 1?
  if (fcntl(dbfd, F_SETLK, &fl) != 0)
  {
    fprintf(stderr, "stirmake: *** Can't lock DB. Other stirmake running? Exiting.\n");
    exit(2);
  }
  ret = dbyydoparse(dbf, &dbyy);
  if (!test && ftruncate(dbfd, 0) != 0)
  {
    fprintf(stderr, "stirmake: *** Can't truncate DB. Exiting.\n");
    exit(2);
  }
  if (ret)
  {
    fprintf(stderr, "stirmake: *** Incompatible DB version. Truncating.\n");
    return;
  }
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
  if (test)
  {
    return;
  }
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
            print_indent();
            printf("removing %s from DB\n", sttable[e->tgtidx].s);
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
  /*
  f = fopen(".stir.db", "w");
  if (f == NULL)
  {
    fprintf(stderr, "Can't open .stir.db"); // can't use errxit
    exit(2);
  }
  */
  f = dbf;
  fprintf(f, "@v1@\n\n");
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
    escape_string(f, sttable[dbe->diridx].s);
    fprintf(f, "\" \"");
    escape_string(f, sttable[dbe->tgtidx].s);
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
  f = NULL;
  dbf = NULL;
}


void create_pipe(int jobcnt)
{
  int err = 0;
  int buf;
  int i;
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, jobserver_fd) != 0)
  {
    printf("28\n");
    my_abort();
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
  fflush(stdout);
  close(jobserver_fd[0]);
  close(jobserver_fd[1]);
  if (pipe(jobserver_fd) != 0)
  {
    printf("28.2\n");
    my_abort();
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
  fflush(stdout);
}

void do_setrlimit(void)
{
  struct rlimit corelimit;
  if (getrlimit(RLIMIT_CORE, &corelimit))
  {
    perror("can't getrlimit");
    exit(2);
  }
  corelimit.rlim_cur = 256*1024*1024;
  if (   corelimit.rlim_max != RLIM_INFINITY
      && corelimit.rlim_max < corelimit.rlim_cur)
  {
    corelimit.rlim_cur = corelimit.rlim_max;
  }
  if (setrlimit(RLIMIT_CORE, &corelimit))
  {
    perror("can't setrlimit");
    exit(2);
  }
}

void handle_signal(int signum)
{
  struct linked_list_node *node;
  LINKED_LIST_FOR_EACH(node, &ruleid_by_pid_list)
  {
    struct ruleid_by_pid *bypid =
      ABCE_CONTAINER_OF(node, struct ruleid_by_pid, llnode);
    kill(bypid->pid, signum);
  }
}

uint32_t forkedchildcnt = 0;
int narration = 0;

fd_set globfds;
int globmaxfd = -1;

void drain_pipe(struct rule *rule, int fdit)
{
  for (;;)
  {
    char buf[10240];
    ssize_t bytes_read;
    bytes_read = read(fdit, buf, sizeof(buf));
    if (bytes_read < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
    {
      break;
    }
    if (bytes_read < 0)
    {
      my_abort(); // RFE EINTR?
    }
    if (bytes_read == 0)
    {
      FD_CLR(fdit, &globfds); // don't close yet, FIXME close later!
      break;
    }
    syncbuf_append(&rule->output, buf, bytes_read);
  }
}

void run_loop(void)
{
  struct linked_list_node *node;
back:
  LINKED_LIST_FOR_EACH(node, &rules_remain_list)
  {
    int ruleid = ABCE_CONTAINER_OF(node, struct rule, remainllnode)->ruleid;
    if (consider(ruleid))
    {
      if (debug)
      {
        print_indent();
        printf("goto back\n");
      }
      goto back; // this can edit the list, need to re-start iteration
    }
  }

  if (ruleids_to_run_size == 0)
  {
    if (test)
    {
      exit(0);
    }
    fprintf(stderr, "stirmake: Nothing to be done.\n");
    goto out;
  }
  else if (test)
  {
    exit(1);
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
      print_indent();
      printf("forking1 child\n");
    }
    int pipefd = -1;
    fork_child(ruleids_to_run[ruleids_to_run_size-1], out_sync != OUT_SYNC_NONE, out_sync == OUT_SYNC_RECURSE, &pipefd);
    if (pipefd >= 0)
    {
      FD_SET(pipefd, &globfds);
      if (pipefd > globmaxfd)
      {
        globmaxfd = pipefd;
      }
    }
    ruleids_to_run_size--;
  }

  int locmaxfd = 0;
  if (self_pipe_fd[0] > locmaxfd)
  {
    locmaxfd = self_pipe_fd[0];
  }
  if (jobserver_fd[0] > locmaxfd)
  {
    locmaxfd = jobserver_fd[0];
  }
  char chbuf[100];
  while (children > 0)
  {
    int wstatus = 0;
    fd_set readfds = globfds;
    FD_SET(self_pipe_fd[0], &readfds);
    if (ruleids_to_run_size > 0)
    {
      FD_SET(jobserver_fd[0], &readfds);
    }
    select((locmaxfd > globmaxfd) ? (locmaxfd+1) : (globmaxfd+1),
           &readfds, NULL, NULL, NULL);
    if (debug)
    {
      print_indent();
      printf("select returned\n");
    }
    if (sigterm_atomic)
    {
      handle_signal(SIGTERM);
      errxit("Got SIGTERM");
      exit(2);
    }
    if (sigint_atomic)
    {
      handle_signal(SIGINT);
      errxit("Got SIGINT");
      exit(2);
    }
    if (sighup_atomic)
    {
      handle_signal(SIGHUP);
      errxit("Got SIGHUP");
      exit(2);
    }
    int fdit;
    for (fdit = 0; fdit <= globmaxfd; fdit++)
    {
      if (fdit == self_pipe_fd[0] || fdit == jobserver_fd[0])
      {
        continue;
      }
      if (!FD_ISSET(fdit, &readfds))
      {
        continue;
      }
      int ruleid = ruleid_by_fd(fdit);
      if (ruleid < 0)
      {
        abort();
      }
      drain_pipe(rules[ruleid], fdit);
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
        if (!ignoreerr && wstatus != 0 && pid > 0)
        {
          if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
          {
            int fd = -1;
            int ruleid = ruleid_by_pid_erase(pid, &fd);
            if (ruleid < 0)
            {
              printf("31.1\n");
              my_abort();
            }
            if (fd >= 0)
            {
              drain_pipe(rules[ruleid], fd);
              syncbuf_dump(&rules[ruleid]->output, 1);
              close(fd);
              FD_CLR(fd, &globfds);
            }
            children--;
            fprintf(stderr, "stirmake: recipe for target '%s' failed\n", sttable[ABCE_CONTAINER_OF(rules[ruleid]->tgtlist.node.next, struct stirtgt, llnode)->tgtidx].s);
            LINKED_LIST_FOR_EACH(node, &rules[ruleid]->tgtlist)
            {
              struct stirtgt *e = ABCE_CONTAINER_OF(node, struct stirtgt, llnode);
	      unlink(sttable[e->tgtidx].s);
	    }
            if (WIFSIGNALED(wstatus))
            {
              if (doasmuchascan)
              {
                fprintf(stderr, "Error: signaled\n");
                cmdfailed = 1;
              }
              else
              {
                errxit("Error: signaled");
              }
            }
            else if (WIFEXITED(wstatus))
            {
              if (doasmuchascan)
              {
                fprintf(stderr, "Error %d\n", (int)WEXITSTATUS(wstatus));
                cmdfailed = 1;
              }
              else
              {
                errxit("Error %d", (int)WEXITSTATUS(wstatus));
              }
            }
            else
            {
              errxit("Unknown error");
            }
            if (doasmuchascan)
            {
              continue;
            }
            my_abort();
          }
        }
        if (children <= 0 && ruleids_to_run_size == 0)
        {
          if (pid < 0 && errno == ECHILD)
          {
            goto out;
          }
          printf("29\n");
          my_abort();
        }
        if (pid < 0)
        {
          if (errno == ECHILD)
          {
            break;
          }
          printf("30\n");
          my_abort();
        }
        int fd = -1;
        int ruleid = ruleid_by_pid_erase(pid, &fd);
        if (ruleid < 0)
        {
          printf("31\n");
          my_abort();
        }
        // FIXME do something with fd
        if (fd >= 0)
        {
          drain_pipe(rules[ruleid], fd);
          syncbuf_dump(&rules[ruleid]->output, 1);
          close(fd);
          FD_CLR(fd, &globfds);
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
        print_indent();
        printf("forking child\n");
      }
      //std::cout << "forking child" << std::endl;
      int pipefd = -1;
      fork_child(ruleids_to_run[ruleids_to_run_size-1], out_sync != OUT_SYNC_NONE, out_sync == OUT_SYNC_RECURSE, &pipefd);
      if (pipefd >= 0)
      {
        FD_SET(pipefd, &globfds);
        if (pipefd > globmaxfd)
        {
          globmaxfd = pipefd;
        }
      }
      ruleids_to_run_size--;
      //ruleids_to_run.pop_back();
    }
  }
out:
  if (linked_list_is_empty(&rules_remain_list))
  {
    return;
  }
  else
  {
    if (doasmuchascan && cmdfailed)
    {
      errxit("Some of the commands failed");
    }
    fprintf(stderr, "stirmake: *** Out of children, yet not all targets made.\n");
    LINKED_LIST_FOR_EACH(node, &rules_remain_list)
    {
      int ruleid = ABCE_CONTAINER_OF(node, struct rule, remainllnode)->ruleid;
      free(better_cycle_detect(ruleid, 0));
    }
    my_abort();
  }

}

int my_get_nprocs(void)
{
#ifdef __linux__
  return get_nprocs();
#else
  #ifdef __APPLE__
  int count = 1;
  size_t count_len = sizeof(count);
  if (sysctlbyname("hw.logicalcpu", &count, &count_len, NULL, 0) != 0 ||
      count < 1)
  {
    fprintf(stderr, "stirmake: can't detect CPU count, assuming 1.\n");
    return 1;
  }
  return count;
  #else
    #ifdef __FreeBSD__
  int count = 1;
  size_t count_len = sizeof(count);
  if (sysctlbyname("hw.ncpu", &count, &count_len, NULL, 0) != 0 ||
      count < 1)
  {
    fprintf(stderr, "stirmake: can't detect CPU count, assuming 1.\n");
    return 1;
  }
  return count;
    #else
      #ifdef __NetBSD__
  int count = 1;
  size_t count_len = sizeof(count);
  if (sysctlbyname("hw.ncpuonline", &count, &count_len, NULL, 0) != 0 ||
      count < 1)
  {
    fprintf(stderr, "stirmake: can't detect CPU count, assuming 1.\n");
    return 1;
  }
  return count;
      #else
  fprintf(stderr, "stirmake: can't detect CPU count, assuming 1.\n");
  return 1;
      #endif
    #endif
  #endif
#endif
}

void process_orders(struct stiryy_main *main)
{
  size_t i;

  if (!deps_remain_calculated)
  {
    abort();
  }

  for (i = 0; i < main->ordersz; i++)
  {
    struct rule *rule;
    if (main->orders[i].rulecnt != 2)
    {
      my_abort();
    }
    size_t first = stringtab_get(main->orders[i].rules[0]);
    size_t second = stringtab_get(main->orders[i].rules[1]);
    if (first == (size_t)-1)
    {
      errxit("@order rule '%s' not found", main->orders[i].rules[0]);
    }
    if (second == (size_t)-1)
    {
      errxit("@order rule '%s' not found", main->orders[i].rules[1]);
    }
    int firstrule = get_ruleid_by_tgt(first);
    int secondrule = get_ruleid_by_tgt(second);
    if (firstrule < 0)
    {
      errxit("@order rule '%s' not found", main->orders[i].rules[0]);
    }
    if (secondrule < 0)
    {
      errxit("@order rule '%s' not found", main->orders[i].rules[1]);
    }
    if (!rules[firstrule]->is_traversed || !rules[secondrule]->is_traversed)
    {
      continue;
    }
    rule = rules[secondrule];
    ins_dep(rule, first, rule->diridx, (size_t)-1, 0, 0, 0, 0);
    deps_remain_insert(rule, firstrule);
    ins_ruleid_by_dep(first, secondrule);
    if (debug)
    {
      print_indent();
      printf("added order %s %s\n", main->orders[i].rules[0], main->orders[i].rules[1]);
    }
  }
}

int main(int argc, char **argv)
{
#if 0
  pathological_test();
#endif
  FILE *f;
  struct stiryy_main main = {.abce = &abce};
  struct stiryy stiryy = {};
  size_t i;
  int opt;
  int filename_set = 0;
  const char *filename = "Stirfile";
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
  char *this_path = ".";
  int ruleid_first = 0;
  int ruleid_first_set = 0;
  char *outsyncmflag = NULL;
  struct linked_list_node *node;

  char *dupargv0 = strdup(argv[0]);
  char *basenm = basename(dupargv0);

  statcache_init();

  struct sigaction saseg;
  sigemptyset(&saseg.sa_mask);
  saseg.sa_flags = 0;
  saseg.sa_handler = sigsegv_handler;
  sigaction(SIGSEGV, &saseg, NULL);

  struct sigaction safpe;
  sigemptyset(&safpe.sa_mask);
  safpe.sa_flags = 0;
  safpe.sa_handler = sigfpe_handler;
  sigaction(SIGFPE, &safpe, NULL);

  struct sigaction saill;
  sigemptyset(&saill.sa_mask);
  saill.sa_flags = 0;
  saill.sa_handler = sigill_handler;
  sigaction(SIGILL, &saill, NULL);

  struct sigaction saquit;
  sigemptyset(&saquit.sa_mask);
  saquit.sa_flags = 0;
  saquit.sa_handler = sigquit_handler;
  sigaction(SIGQUIT, &saquit, NULL);

  struct sigaction sasys;
  sigemptyset(&sasys.sa_mask);
  sasys.sa_flags = 0;
  sasys.sa_handler = sigsys_handler;
  sigaction(SIGSYS, &sasys, NULL);

  struct sigaction saxcpu;
  sigemptyset(&saxcpu.sa_mask);
  saxcpu.sa_flags = 0;
  saxcpu.sa_handler = sigxcpu_handler;
  sigaction(SIGXCPU, &saxcpu, NULL);

  struct sigaction saxfsz;
  sigemptyset(&saxfsz.sa_mask);
  saxfsz.sa_flags = 0;
  saxfsz.sa_handler = sigxfsz_handler;
  sigaction(SIGXFSZ, &saxfsz, NULL);

  struct sigaction saabrt;
  sigemptyset(&saabrt.sa_mask);
  saabrt.sa_flags = 0;
  saabrt.sa_handler = sigabrt_handler;
  sigaction(SIGABRT, &saabrt, NULL);

  struct sigaction sabus;
  sigemptyset(&sabus.sa_mask);
  sabus.sa_flags = 0;
  sabus.sa_handler = sigbus_handler;
  sigaction(SIGBUS, &sabus, NULL);

  do_setrlimit();

  sttable = stir_do_mmap_madvise(st_cap*sizeof(*sttable));
  if (sttable == NULL || sttable == MAP_FAILED)
  {
    errxit("Can't mmap sttable");
    exit(2);
  }

  sizeof_my_arena = 1024*1024;
  my_arena = stir_do_mmap_madvise(sizeof_my_arena);
  if (my_arena == NULL || my_arena == MAP_FAILED)
  {
    errxit("Can't mmap arena");
    exit(2);
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

  process_mflags(NULL, &outsyncmflag);
  if (outsyncmflag)
  {
    if (outsyncmflag[0] == 'n')
    {
      out_sync = OUT_SYNC_NONE;
    }
    else if (outsyncmflag[0] == 'l')
    {
      fprintf(stderr, "stirmake: line sync not supported, using target sync\n");
      out_sync = OUT_SYNC_TARGET;
    }
    else if (outsyncmflag[0] == 't')
    {
      out_sync = OUT_SYNC_TARGET;
    }
    else if (outsyncmflag[0] == 'r')
    {
      out_sync = OUT_SYNC_RECURSE;
    }
    else
    {
      fprintf(stderr, "stirmake: invalid output sync mode in MAKEFLAGS, disabling sync\n");
      out_sync = OUT_SYNC_NONE;
    }
  }

  debug = 0;
  while ((opt = getopt(argc, argv, "vdf:Htpaj:hcbO:qC:ikBW:X:n")) != -1)
  {
    switch (opt)
    {
    case 'C':
      if (chdir(optarg) != 0)
      {
        fprintf(stderr, "stirmake: failed to chdir to %s\n", optarg);
	exit(2);
      }
      break;
    case 'v':
      version(argv[0]);
    case 'W':
    {
      struct pretend *oldpretend = pretend;
      pretend = malloc(sizeof(*pretend));
      pretend->fname = canon(optarg);
      pretend->next = oldpretend;
      pretend->relative = 0;
      break;
    }
    case 'X':
    {
      struct pretend *oldpretend = pretend;
      pretend = malloc(sizeof(*pretend));
      pretend->fname = canon(optarg);
      pretend->next = oldpretend;
      pretend->relative = 1;
      break;
    }
    case 'i':
      ignoreerr = 1;
      break;
    case 'k':
      doasmuchascan = 1;
      break;
    case 'B':
      unconditional = 1;
      break;
    case 'd':
      debug = 1;
      break;
    case 'q':
      test = 1;
      break;
    case 'n':
      dry_run = 1;
      break;
    case 'O':
      if (optarg[0] == 'n')
      {
        out_sync = OUT_SYNC_NONE;
      }
      else if (optarg[0] == 'l')
      {
        fprintf(stderr, "stirmake: line sync not supported, using target sync\n");
        out_sync = OUT_SYNC_TARGET;
      }
      else if (optarg[0] == 't')
      {
        out_sync = OUT_SYNC_TARGET;
      }
      else if (optarg[0] == 'r')
      {
        out_sync = OUT_SYNC_RECURSE;
      }
      else
      {
        usage(argv[0]);
      }
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
      if (optarg[0] == 'a') // a for auto
      {
        jobcnt = my_get_nprocs();
        if (jobcnt < 1)
        {
          fprintf(stderr, "stirmake: Processor count %d insane, using 1\n",
                  jobcnt);
          jobcnt = 1;
        }
        if (jobcnt > MAX_JOBCNT)
        {
          jobcnt = MAX_JOBCNT;
        }
      }
      else
      {
        jobcnt = atoi(optarg);
        if (jobcnt < 1)
        {
          usage(argv[0]);
        }
        if (jobcnt > MAX_JOBCNT)
        {
          usage(argv[0]);
        }
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

  if (test && (clean || cleanbinaries))
  {
    usage(argv[0]);
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
      my_abort();
    }
    if (getcwd(storcwd, sizeof(storcwd)) == NULL)
    {
      my_abort();
    }
    if (getcwd(curcwd, sizeof(curcwd)) == NULL)
    {
      my_abort();
    }
    if (getcwd(cwd_sameproj, sizeof(cwd_sameproj)) == NULL)
    {
      my_abort();
    }
    for (;;)
    {
      if (strcmp(curcwd, "/") == 0)
      {
        break;
      }
      if (chdir("..") != 0)
      {
        my_abort();
      }
      if (getcwd(curcwd, sizeof(curcwd)) == NULL)
      {
        printf("can't getcwd\n");
        my_abort();
      }
      curupcnt++;
      f = fopen("Stirfile", "r");
      if (f)
      {
        int ret;
        abce_init_opts(&abce, 1);
        abce_inited = 1;
        abce.trap = stir_trap;
        abce.trap_baton = &main;
        init_main_for_realpath(&main, storcwd);
        main.abce = &abce;
        main.parsing = 1;
        main.trial = 1;
        main.freeform_token_seen = 1;
        stiryy_init(&stiryy, &main, ".", ".", abce.dynscope, curcwd, "Stirfile", 1);
        ret = stiryydoparse(f, &stiryy);
        fclose(f);
        if (ret == 0 && main.subdirseen)
        {
          upcnt = curupcnt;
          if (snprintf(cwd, sizeof(cwd), "%s", curcwd) >= sizeof(cwd))
          {
            printf("can't snprintf\n");
            my_abort();
          }
        }
        if (ret == 0 && main.subdirseen_sameproject)
        {
          upcnt_sameproj = curupcnt;
          if (snprintf(cwd_sameproj, sizeof(cwd_sameproj), "%s", curcwd)
              >= sizeof(cwd_sameproj))
          {
            printf("can't snprintf\n");
            my_abort();
          }
        }
        stiryy_free(&stiryy);
        stiryy_main_free(&main);
        abce_free(&abce);
        abce_inited = 0;
      }
    }
    printf("stirmake: Using directory %s\n", cwd);
    fflush(stdout);
    if (chdir(cwd) != 0)
    {
      my_abort();
    }
  }

  load_db();
  abce_init_opts(&abce, 1);
  abce_inited = 1;
  abce.trap = stir_trap;
  abce.trap_baton = &main;
  init_main_for_realpath(&main, ".");
  main.abce = &abce;
  main.parsing = 1;
  main.trial = 0;
  main.freeform_token_seen = 0;
  stiryy_init(&stiryy, &main, ".", ".", abce.dynscope, NULL, filename, 1);

  f = fopen(filename, "r");
  if (!f)
  {
    errxit("Stirfile not found");
    my_abort();
  }
  if (stiryydoparse(f, &stiryy) != 0)
  {
    errxit("Parsing failed");
  }
  main.parsing = 0;
  fclose(f);

  stack_conf();

  this_path = calc_forward_path(storcwd, upcnt);
  if (mode == MODE_ALL || mode == MODE_NONE)
  {
    fwd_path = ".";
  }
  else if (mode == MODE_THIS)
  {
    fwd_path = calc_forward_path(storcwd, upcnt);
    printf("stirmake: Forward path: %s\n", fwd_path);
    fflush(stdout);
  }
  else if (mode == MODE_PROJECT)
  {
    fwd_path = calc_forward_path(cwd_sameproj, upcnt - upcnt_sameproj);
    printf("stirmake: Forward path: %s\n", fwd_path);
    fflush(stdout);
  }
  else
  {
    my_abort();
  }
  if (pretend != NULL)
  {
    struct pretend *iter = pretend;
    char pathbuf[PATH_MAX+1];
    while (iter != NULL)
    {
      if (iter->relative)
      {
        if (snprintf(pathbuf, sizeof(pathbuf), "%s/%s", this_path, iter->fname) >= sizeof(pathbuf))
        {
          errxit("too long pathname to pretend being modified: %s", iter->fname);
        }
        iter->fname = canon(pathbuf);
        iter->relative = 0;
      }
      else
      {
        if (snprintf(pathbuf, sizeof(pathbuf), "%s/%s", fwd_path, iter->fname) >= sizeof(pathbuf))
        {
          errxit("too long pathname to pretend being modified: %s", iter->fname);
        }
        iter->fname = canon(pathbuf);
      }
      iter = iter->next;
    }
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
        print_indent();
        printf("ADDING RULE\n");
      }
      if (main.rules[i].ispat)
      {
        char *prefix = main.rules[i].prefix;
        size_t prefixlen = strlen(prefix);
        size_t j, k;
        if (main.rules[i].targetsz < 1)
        {
          my_abort();
        }
        for (j = 0; j < main.rules[i].basesz; j++)
        {
          char *base = main.rules[i].bases[j].name;
          char *basenodir = main.rules[i].bases[j].namenodir;
          char *tgt = main.rules[i].targets[0].name;
          struct tgt *tgts;
          struct dep *deps;
          char *loc, *locp1;
          size_t meatsz;
          char *meat;
          size_t locp1sz;
          tgts = malloc(sizeof(*tgts)*main.rules[i].targetsz);
          deps = malloc(sizeof(*deps)*main.rules[i].depsz);
          if (strcnt(tgt, '%') != 1)
          {
            errxit("Target %s must have exactly one %% sign", tgt);
            exit(2);
          }
          loc = strchr(tgt, '%');
          locp1 = loc+1;
          locp1sz = strlen(locp1);
          if (memcmp(basenodir, tgt, loc-tgt) != 0)
          {
            errxit("Target %s didn't match base %s", tgt, basenodir);
            exit(2);
          }
          if (memcmp(basenodir+strlen(basenodir)-locp1sz, locp1, locp1sz) != 0)
          {
            errxit("Target %s didn't match base %s", tgt, basenodir);
            exit(2);
          }
          meatsz = strlen(basenodir)-strlen(tgt)+1;
          meat = malloc(meatsz+1);
          memcpy(meat, basenodir+(loc-tgt), meatsz);
          meat[meatsz] = '\0';
          tgts[0].name = base;
          tgts[0].namenodir = basenodir;
          for (k = 1; k < main.rules[i].targetsz; k++)
          {
            char *tgt = main.rules[i].targets[k].name;
            size_t exptgtsz; // expanded target size
            char *exptgt;
            size_t namedirsz;
            char *namedir;
            if (strcnt(tgt, '%') != 1)
            {
              errxit("Target %s must have exactly one %% sign", tgt);
              exit(2);
            }
            loc = strchr(tgt, '%');
            locp1 = loc+1;
            locp1sz = strlen(locp1);
            exptgtsz = strlen(tgt) - 1 + meatsz;
            exptgt = malloc(exptgtsz + 1);
            memcpy(exptgt, tgt, loc-tgt);
            memcpy(&exptgt[loc-tgt], meat, meatsz);
            memcpy(&exptgt[loc-tgt+meatsz], locp1, locp1sz);
            exptgt[loc-tgt+meatsz+locp1sz] = '\0';
            if (loc-tgt+meatsz+locp1sz != exptgtsz)
            {
              my_abort();
            }
            namedirsz = prefixlen+strlen(exptgt)+2;
            namedir = malloc(namedirsz);
            if (   snprintf(namedir, namedirsz, "%s/%s", prefix, exptgt)
                >= namedirsz)
            {
              my_abort();
            }
            tgts[k].name = canon(namedir);
            tgts[k].namenodir = exptgt;
            free(namedir);
          }
          for (k = 0; k < main.rules[i].depsz; k++)
          {
            char *dep = main.rules[i].deps[k].name;
            size_t expdepsz; // expanded target size
            char *expdep;
            size_t namedirsz;
            char *namedir;
            if (strcnt(dep, '%') > 1)
            {
              errxit("Dep %s must have exactly zero or one %% signs", dep);
              exit(2);
            }
            loc = strchr(dep, '%');
            if (loc == NULL)
            {
              deps[k].name = dep;
              deps[k].namenodir = main.rules[i].deps[k].namenodir;
              deps[k].rec = main.rules[i].deps[k].rec;
              deps[k].orderonly = main.rules[i].deps[k].orderonly;
              continue;
            }
            locp1 = loc+1;
            locp1sz = strlen(locp1);
            expdepsz = strlen(dep) - 1 + meatsz;
            expdep = malloc(expdepsz + 1);
            memcpy(expdep, dep, loc-dep);
            memcpy(&expdep[loc-dep], meat, meatsz);
            memcpy(&expdep[loc-dep+meatsz], locp1, locp1sz);
            expdep[loc-dep+meatsz+locp1sz] = '\0';
            if (loc-dep+meatsz+locp1sz != expdepsz)
            {
              my_abort();
            }
            namedirsz = prefixlen+strlen(expdep)+2;
            namedir = malloc(namedirsz);
            if (   snprintf(namedir, namedirsz, "%s/%s", prefix, expdep)
                >= namedirsz)
            {
              my_abort();
            }
            deps[k].name = canon(namedir);
            deps[k].namenodir = expdep;
            deps[k].rec = main.rules[i].deps[k].rec;
            deps[k].orderonly = main.rules[i].deps[k].orderonly;
            free(namedir);
          }
          if (   main.rules[i].iscleanhook
              || main.rules[i].isdistcleanhook
              || main.rules[i].isbothcleanhook)
          {
            my_abort();
          }
          add_rule(tgts, main.rules[i].targetsz,
                   deps, main.rules[i].depsz,
                   &main.rules[i].shells, //main.rules[i].shellsz,
                   main.rules[i].phony, main.rules[i].rectgt,
                   main.rules[i].detouch, main.rules[i].maybe,
                   main.rules[i].dist,
                   main.rules[i].iscleanhook, main.rules[i].isdistcleanhook,
                   main.rules[i].isbothcleanhook,
                   main.rules[i].prefix, main.rules[i].scopeidx);
        }
        continue;
      }
      add_rule(main.rules[i].targets, main.rules[i].targetsz,
               main.rules[i].deps, main.rules[i].depsz,
               &main.rules[i].shells, //main.rules[i].shellsz,
               main.rules[i].phony, main.rules[i].rectgt,
               main.rules[i].detouch, main.rules[i].maybe,
               main.rules[i].dist,
               main.rules[i].iscleanhook, main.rules[i].isdistcleanhook,
               main.rules[i].isbothcleanhook,
               main.rules[i].prefix, main.rules[i].scopeidx);
      if (   (!ruleid_first_set)
          && (/* strcmp(fwd_path, ".") == 0 || */
              strcmp(fwd_path, main.rules[i].prefix) == 0))
      {
        if (rules_size == 0)
        {
          my_abort();
        }
        if (main.rules[i].iscleanhook)
        {
          continue;
        }
        if (main.rules[i].isdistcleanhook)
        {
          continue;
        }
        if (main.rules[i].isbothcleanhook)
        {
          continue;
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
        print_indent();
        printf("ADDING DEP\n");
      }
      add_dep_from_rules(main.rules[i].targets, main.rules[i].targetsz,
                         main.rules[i].deps, main.rules[i].depsz, 0);
    }
  }
  if (!ruleid_first_set && optind == argc)
  {
    errxit("no applicable rules");
  }

  for (i = 0; i < stiryy.main->cdepincludesz; i++)
  {
    struct incyy incyy = {
      .prefix = stiryy.main->cdepincludes[i].prefix,
      .auto_target = stiryy.main->cdepincludes[i].auto_target,
      .fnamenodir = stiryy.main->cdepincludes[i].name,
    };
    size_t j;
    size_t fnamesz =
      strlen(incyy.prefix) + strlen(stiryy.main->cdepincludes[i].name) + 2;
    char *fname = malloc(fnamesz);
    if (snprintf(fname, fnamesz, "%s/%s", incyy.prefix, stiryy.main->cdepincludes[i].name) >= fnamesz)
    {
      printf("24.5\n");
      my_abort();
    }
    if (debug)
    {
      print_indent();
      printf("reading cdepincludes from %s\n", fname);
    }
    f = fopen(fname, "r");
    if (!f)
    {
      if (stiryy.main->cdepincludes[i].ignore)
      {
        free(fname);
        continue;
      }
      errxit("Can't read cdepincludes from %s", fname);
      my_abort();
    }
    free(fname);
    incyydoparse(f, &incyy);
    //for (auto it = incyy.rules; it != incyy.rules + incyy.rulesz; it++)
    for (j = 0; j < incyy.rulesz; j++)
    {
      size_t k;
      //std::vector<std::string> tgt;
      //std::vector<std::string> dep;
      //std::copy(it->deps, it->deps+it->depsz, std::back_inserter(dep));
      //std::copy(it->targets, it->targets+it->targetsz, std::back_inserter(tgt));
      if (debug)
      {
        print_indent();
        printf("Adding dep\n");
        for (k = 0; k < incyy.rules[j].targetsz; k++)
        {
          printf("  target: %s\n", incyy.rules[j].targets[k]);
        }
        for (k = 0; k < incyy.rules[j].depsz; k++)
        {
          printf("  dep: %s\n", incyy.rules[j].deps[k]);
        }
      }
      add_dep(incyy.rules[j].targets, incyy.rules[j].targetsz,
              incyy.rules[j].deps, incyy.rules[j].depsz,
              0, !!stiryy.main->cdepincludes[i].auto_phony);
      //add_dep(tgt, dep, 0);
    }
    fclose(f);
    incyy_free(&incyy);
  }
  stiryy_free(&stiryy);

  //add_dep(v_l3e, v_l1g, 0); // offending rule

#if 0
  unsigned char *no_cycles = better_cycle_detect(0); // FIXME 0 incorrect!
#endif

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
      my_abort();
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
        hashval = abce_murmur32(HASH_SEED, one->ruleid);
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
      hashval = abce_murmur32(HASH_SEED, entry->depidx);
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
    hashval = abce_murmur32(HASH_SEED, entry->tgtidx);
    hashloc = hashval % (sizeof(ruleid_by_tgt)/sizeof(*ruleid_by_tgt));
    abce_rb_tree_nocmp_delete(&ruleid_by_tgt[hashloc], &entry->node);
    linked_list_delete(&entry->llnode);
    my_free(entry);
  }
#endif

  if (pipe(self_pipe_fd) != 0)
  {
    printf("27\n");
    my_abort();
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

  struct sigaction salrm;
  sigemptyset(&salrm.sa_mask);
  salrm.sa_flags = 0;
  salrm.sa_handler = sigalrm_handler;
  sigaction(SIGALRM, &salrm, NULL);

  struct sigaction saint, saterm, sahup;
  sigemptyset(&saint.sa_mask);
  saint.sa_flags = 0;
  saint.sa_handler = sigint_handler;
  sigaction(SIGINT, &saint, NULL);
  sigemptyset(&saterm.sa_mask);
  saterm.sa_flags = 0;
  saterm.sa_handler = sigterm_handler;
  sigaction(SIGTERM, &saterm, NULL);
  sigemptyset(&sahup.sa_mask);
  sahup.sa_flags = 0;
  sahup.sa_handler = sighup_handler;
  sigaction(SIGHUP, &sahup, NULL);

  //consider(0);

/*
  while (children < limit && !ruleids_to_run.empty())
  {
    std::cout << "forking1 child" << std::endl;
    fork_child(ruleids_to_run.back());
    ruleids_to_run.pop_back();
  }
*/

  if (optind == argc)
  {
    //free(better_cycle_detect(ruleid_first, 1));
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
      //free(better_cycle_detect(ruleid, 1));
      //ruleremain_add(rules[ruleid]); // later!
    }
    if (clean || cleanbinaries)
    {
      do_clean(".", clean, cleanbinaries);
    }
    for (i = optind; i < argc; i++)
    {
      size_t stidx = stringtab_add(argv[i]);
      int ruleid = get_ruleid_by_tgt(stidx);
      if (ruleid < 0)
      {
        errxit("rule '%s' not found", argv[i]);
      }
      //free(better_cycle_detect(ruleid, 0));
      ruleremain_add(rules[ruleid]);
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
      //free(better_cycle_detect(ruleid, 1));
      //ruleremain_add(rules[ruleid]); // later!
      free(can);
    }
    if (clean || cleanbinaries)
    {
      do_clean(fwd_path, clean, cleanbinaries);
    }
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
      //free(better_cycle_detect(ruleid, 0));
      ruleremain_add(rules[ruleid]);
      free(can);
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
      //free(better_cycle_detect(ruleid, 1));
      //ruleremain_add(rules[ruleid]); // later!
      free(can);
    }
    if (clean || cleanbinaries)
    {
      do_clean(fwd_path, clean, cleanbinaries);
    }
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
      //free(better_cycle_detect(ruleid, 0));
      ruleremain_add(rules[ruleid]);
      free(can);
    }
  }

  if (!deps_remain_calculated)
  {
    process_additional_deps(abce.dynscope.u.area->u.sc.locidx);
    for (i = 0; i < rules_size; i++)
    {
      calc_deps_remain(rules[i]);
    }
    deps_remain_calculated = 1;
  }

  LINKED_LIST_FOR_EACH(node, &rules_remain_list)
  {
    int ruleid = ABCE_CONTAINER_OF(node, struct rule, remainllnode)->ruleid;
    free(better_cycle_detect(ruleid, 1));
  }

  process_orders(&main);

  if (main.ordersz > 0)
  {
    LINKED_LIST_FOR_EACH(node, &rules_remain_list)
    {
      int ruleid = ABCE_CONTAINER_OF(node, struct rule, remainllnode)->ruleid;
      free(better_cycle_detect(ruleid, 0));
    }
  }

  run_loop();

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
#if 0
  free(dupargv0);
  stiryy_main_free(&main);
  abce_free(&abce);
  free(rules);
  rules = NULL;
#endif
  return 0;
}
