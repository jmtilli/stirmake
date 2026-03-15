#ifndef _STIRYY_H_
#define _STIRYY_H_

#include <stddef.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "abce/abce.h"
#include "canon.h"
#include "stirtrap.h"
#include "abce/abcescopes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t mysize_t;

void my_abort(void);

extern int yy_stored_lineno;
extern const char *yy_stored_prefix;

struct escaped_string {
  size_t sz;
  char *str;
};

struct CSnippet {
  char *data;
  size_t len;
  size_t capacity;
};

static inline char *fd_grok(int fd)
{
  char *buf = NULL;
  const size_t xfer = 1024;
  size_t sz = 0, cap = 2048;
  ssize_t ret;
  buf = malloc(cap);
  for (;;)
  {
    if (sz + xfer + 1 < cap)
    {
      cap = 2*cap + xfer + 1;
      buf = realloc(buf, cap);
    }
    ret = read(fd, buf + sz, xfer);
    if (ret == 0)
    {
      buf[sz] = '\0';
      return buf;
    }
    if (ret < 0)
    {
      free(buf);
      return NULL;
    }
    sz += (size_t)ret;
  }
}

static inline char *eval_cmd(char **argv)
{
  pid_t child;
  int pipefds[2];

  if (pipe(pipefds) != 0)
  {
    return NULL;
  }
  child = fork();
  if (child < 0)
  {
    close(pipefds[0]);
    close(pipefds[1]);
    return NULL;
  }
  if (child == 0)
  {
    close(0);
    if (open("/dev/null", O_RDONLY) != 0)
    {
      _exit(1);
    }
    dup2(pipefds[1], 1);
    close(pipefds[0]);
    close(pipefds[1]);
    execvp(argv[0], argv);
    _exit(1);
  }
  else
  {
    char *result;
    int wstatus;
    close(pipefds[1]);
    result = fd_grok(pipefds[0]);
    close(pipefds[0]);
    waitpid(child, &wstatus, 0);
    if (!WIFEXITED(wstatus))
    {
      free(result);
      return NULL;
    }
    if (WEXITSTATUS(wstatus) != 0)
    {
      free(result);
      return NULL;
    }
    return result;
  }
}

static inline void csadd(struct CSnippet *cs, char ch)
{
  if (cs->len + 2 >= cs->capacity)
  {
    size_t new_capacity = cs->capacity * 2 + 2;
    cs->data = (char*)realloc(cs->data, new_capacity);
    cs->capacity = new_capacity;
  }
  cs->data[cs->len] = ch;
  cs->data[cs->len + 1] = '\0';
  cs->len++;
}

static inline void csaddstr(struct CSnippet *cs, char *str)
{
  size_t len = strlen(str);
  if (cs->len + len + 1 >= cs->capacity)
  {
    size_t new_capacity = cs->capacity * 2 + 2;
    if (new_capacity < cs->len + len + 1)
    {
      new_capacity = cs->len + len + 1;
    }
    cs->data = (char*)realloc(cs->data, new_capacity);
    cs->capacity = new_capacity;
  }
  memcpy(cs->data + cs->len, str, len);
  cs->len += len;
  cs->data[cs->len] = '\0';
}

struct cmdsrcitem {
  unsigned merge:1;
  unsigned iscode:1;
  unsigned isfun:1;
  unsigned ignore:1;
  unsigned noecho:1;
  unsigned ismake:1;
  size_t sz; // for args
  size_t capacity; // for args
  union {
    struct {
      size_t funidx;
      size_t argidx;
    } funarg;
    size_t locidx;
    char **args; // NULL-terminated list
    char ***cmds; // NULL-terminated list of NULL-terminated lists
  } u;
};

struct cmdsrc {
  size_t itemsz;
  size_t itemcapacity;
  struct cmdsrcitem *items;
};

struct dep {
  char *name;
  char *namenodir;
  // for patrules, it's name or namenodir and wildcard and suffix
  // if suffix is NULL, then wildcard is not used
  char *suffix;
  int percent_special;
  int rec;
  int orderonly;
  int wait;
};
struct tgt {
  char *name;
  char *namenodir;
  // for patrules, it's name or namenodir and wildcard and suffix
  // if suffix is NULL, then wildcard is not used
  char *suffix;
  int percent_special;
  int is_dist;
};

struct stiryyrule {
  struct tgt *bases;
  size_t basesz;
  size_t basecapacity;
  struct dep *deps;
  size_t depsz;
  size_t depcapacity;
  struct tgt *targets;
  size_t targetsz;
  size_t targetcapacity;
  struct cmdsrc shells;
  size_t scopeidx;
  char *prefix;
  int lineno;
  unsigned phony:1;
  unsigned rectgt:1;
  unsigned detouch:1;
  unsigned maybe:1;
  unsigned dist:1;
  unsigned deponly:1;
  unsigned iscleanhook:1;
  unsigned isdistcleanhook:1;
  unsigned isbothcleanhook:1;
  unsigned ispat:1;
  unsigned patfrozen:1;
};

struct stiryyorder {
  char *rules[2];
  char *rulesnodir[2];
  int rulecnt;
};

struct stiryy_main {
  struct stiryyrule *rules;
  size_t rulesz;
  size_t rulecapacity;
  struct stiryyorder *orders;
  size_t ordersz;
  size_t ordercapacity;
  struct abce *abce;
  char *realpathname;
  int subdirseen;
  int subdirseen_sameproject;
  int freeform_token_seen;
  int parsing;
  int trial;
  int rule_in_progress;

  struct cdepinclude *cdepincludes;
  size_t cdepincludesz;
  size_t cdepincludecapacity;
};

struct cdepinclude {
  char *name;
  char *prefix;
  int auto_phony;
  int auto_target;
  int ignore;
};

struct stiryy {
  void *baton;
#if 0
  uint8_t *bytecode;
  size_t bytecapacity;
  size_t bytesz;
#endif

  struct stiryy_main *main;
  struct amyplan_locvarctx *ctx;
  char *curprefix;
  char *curprojprefix;
  size_t curscopeidx;
  struct abce_mb curscope;
  int sameproject;
  int expect_toplevel;
  unsigned abort_early:1;
  unsigned indicator_seen:1;
  unsigned was_toplevel:1;
  const char *dirname;
  const char *filename;
  int do_emit;
};

#define STIRYY_EMPTY {.baton = NULL}

static inline void init_main_for_realpath(struct stiryy_main *stirmain, char *cwd)
{
  char buf2[PATH_MAX+16];
  char buf3[PATH_MAX+16];
  memset(stirmain, 0, sizeof(*stirmain));
  if (realpath(cwd, buf2) == NULL)
  {
    my_abort();
  }
  if (snprintf(buf3, sizeof(buf3), "%s/Stirfile", buf2) >= (int)sizeof(buf3))
  {
    my_abort();
  }
  stirmain->realpathname = canon(buf3);
  stirmain->subdirseen = 0;
  stirmain->rule_in_progress = 0;
}

static inline void stiryy_init(struct stiryy *yy, struct stiryy_main *stirmain,
                               char *prefix, char *projprefix,
                               struct abce_mb curscope,
                               const char *dirname, const char *filename,
                               int expect_toplevel)
{
  yy->main = stirmain;
  yy->sameproject = 1;
  yy->abort_early = 0;
  yy->indicator_seen = 0;
  yy->was_toplevel = 0;
  //abce_init(&yy->abce);
  yy->ctx = NULL;
  yy->curprefix = strdup(prefix);
  yy->curprojprefix = strdup(projprefix);
  yy->curscopeidx = abce_cache_add(yy->main->abce, &curscope); // avoid GC abort
  yy->curscope = curscope;
  yy->filename = filename;
  yy->dirname = dirname;
  yy->do_emit = 1;
  yy->expect_toplevel = !!expect_toplevel;
}

static inline size_t stiryy_symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen)
{
  return abce_cache_add_str(stiryy->main->abce, symbol, symlen);
}
static inline size_t stiryy_add_fun_sym(struct stiryy *stiryy, const char *symbol, int maybe, size_t loc)
{
  struct abce_mb mb;
  const struct abce_mb *oldmb;
  int ret;
  size_t retloc;
  mb.typ = ABCE_T_F;
  mb.u.d = loc;
  oldmb = abce_sc_get_rec_str_area(stiryy->main->abce->dynscope.u.area, symbol, 1);
  if (oldmb != NULL)
  {
    retloc = abce_cache_add(stiryy->main->abce, oldmb);
  }
  else
  {
    retloc = (size_t)-1;
  }
  ret = abce_sc_put_val_str_maybe_old(stiryy->main->abce, &stiryy->main->abce->dynscope, symbol, &mb, maybe, NULL);
  if (ret != 0 && ret != -EEXIST)
  {
    printf("can't add symbol %s\n", symbol);
    exit(2);
  }
  return retloc;
}

static inline void stiryy_add_byte(struct stiryy *stiryy, uint16_t ins)
{
  abce_add_ins(stiryy->main->abce, ins);
}

static inline void stiryy_add_double(struct stiryy *stiryy, double dbl)
{
  abce_add_double(stiryy->main->abce, dbl);
}

static inline void stiryy_set_double(struct stiryy *stiryy, size_t i, double dbl)
{
  abce_set_double(stiryy->main->abce, i, dbl);
}

mysize_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen);

static inline void stiryy_set_cdepinclude(struct stiryy *stiryy, const char *cd, int auto_phony, int auto_target, int ignore)
{
  size_t newcapacity;
  if (stiryy->main->cdepincludesz >= stiryy->main->cdepincludecapacity)
  {
    newcapacity = 2*stiryy->main->cdepincludecapacity + 1;
    stiryy->main->cdepincludes = realloc(stiryy->main->cdepincludes, sizeof(*stiryy->main->cdepincludes)*newcapacity);
    stiryy->main->cdepincludecapacity = newcapacity;
  }
  stiryy->main->cdepincludes[stiryy->main->cdepincludesz].name = strdup(cd);
  stiryy->main->cdepincludes[stiryy->main->cdepincludesz].prefix = strdup(stiryy->curprefix);
  stiryy->main->cdepincludes[stiryy->main->cdepincludesz].auto_phony = !!auto_phony;
  stiryy->main->cdepincludes[stiryy->main->cdepincludesz].auto_target = !!auto_target;
  stiryy->main->cdepincludes[stiryy->main->cdepincludesz].ignore = !!ignore;
  stiryy->main->cdepincludesz++;
}

static inline void stiryy_main_set_patdep(struct stiryy_main *stirmain, const char *curprefix, const char *dep, int rec, int orderonly, int wait, int percent_special)
{
  struct stiryyrule *rule = &stirmain->rules[stirmain->rulesz - 1];
  size_t newcapacity;
  size_t sz = strlen(curprefix) + strlen(dep) + 2;
  char *can, *tmp = malloc(sz);
  if (!rule->ispat || !rule->patfrozen)
  {
    abort();
  }
  if (dep[0] == '/')
  {
    if (snprintf(tmp, sz, "%s", dep) >= (int)sz)
    {
      my_abort();
    }
  }
#if 0
  else if (dep[0] == '%')
  {
    if (snprintf(tmp, sz, "%s", dep) >= (int)sz)
    {
      my_abort();
    }
  }
#endif
  else
  {
    if (snprintf(tmp, sz, "%s/%s", curprefix, dep) >= (int)sz)
    {
      my_abort();
    }
  }
  can = canon(tmp);
  free(tmp);
  if (rule->depsz >= rule->depcapacity)
  {
    newcapacity = 2*rule->depcapacity + 1;
    rule->deps = (struct dep*)realloc(rule->deps, sizeof(*rule->deps)*newcapacity);
    rule->depcapacity = newcapacity;
  }
  rule->deps[rule->depsz].name = strdup(can); // Let's copy it to compact it
  rule->deps[rule->depsz].namenodir = strdup(dep);
  rule->deps[rule->depsz].suffix = NULL;
  rule->deps[rule->depsz].percent_special = percent_special;
  rule->deps[rule->depsz].rec = rec;
  rule->deps[rule->depsz].orderonly = orderonly;
  rule->deps[rule->depsz].wait = wait;
  rule->depsz++;
  free(can);
}
static inline void stiryy_main_set_patdep2(struct stiryy_main *stirmain, const char *curprefix, const char **dep, int rec, int orderonly, int wait)
{
  struct stiryyrule *rule = &stirmain->rules[stirmain->rulesz - 1];
  size_t newcapacity;
  size_t sz1 = strlen(curprefix) + 1 + strlen(dep[0]) + 1;
  size_t sz2 = strlen(dep[2]) + 1;
  char *can, *tmp1 = malloc(sz1), *tmp2 = malloc(sz2);
  size_t off = 0;
  if (!rule->ispat || !rule->patfrozen)
  {
    abort();
  }
  if (dep[0][0] == '/')
  {
    if (snprintf(tmp1, sz1, "%s", dep[0]) >= (int)sz1)
    {
      my_abort();
    }
    if (snprintf(tmp2, sz2, "%s", dep[2]) >= (int)sz2)
    {
      my_abort();
    }
  }
  else
  {
    off = strlen(curprefix)+1;
    if (snprintf(tmp1, sz1, "%s/%s", curprefix, dep[0]) >= (int)sz1)
    {
      my_abort();
    }
    if (snprintf(tmp2, sz2, "%s", dep[2]) >= (int)sz2)
    {
      my_abort();
    }
  }
  can = canon(tmp1);
  if (rule->depsz >= rule->depcapacity)
  {
    newcapacity = 2*rule->depcapacity + 1;
    rule->deps = (struct dep*)realloc(rule->deps, sizeof(*rule->deps)*newcapacity);
    rule->depcapacity = newcapacity;
  }
  rule->deps[rule->depsz].name = strdup(can); // Let's copy it to compact it
  rule->deps[rule->depsz].namenodir = strdup(tmp1+off);
  rule->deps[rule->depsz].suffix = strdup(tmp2);
  rule->deps[rule->depsz].percent_special = 0;
  rule->deps[rule->depsz].rec = rec;
  rule->deps[rule->depsz].orderonly = orderonly;
  rule->deps[rule->depsz].wait = wait;
  rule->depsz++;
  free(can);
  free(tmp1);
  free(tmp2);
}

static inline void stiryy_main_set_order(struct stiryy_main *stirmain, const char *curprefix, const char *name)
{
  struct stiryyorder *order = &stirmain->orders[stirmain->ordersz - 1];
  size_t sz = strlen(curprefix) + strlen(name) + 2;
  char *can, *tmp = malloc(sz);
  if (name[0] == '/')
  {
    if (snprintf(tmp, sz, "%s", name) >= (int)sz)
    {
      my_abort();
    }
  }
  else
  {
    if (snprintf(tmp, sz, "%s/%s", curprefix, name) >= (int)sz)
    {
      my_abort();
    }
  }
  can = canon(tmp);
  free(tmp);
  if (order->rulecnt >= 2)
  {
    my_abort();
  }
  order->rules[order->rulecnt] = strdup(can); // Let's copy it to compact it
  order->rulesnodir[order->rulecnt] = strdup(name);
  order->rulecnt++;
  free(can);
}

static inline void stiryy_main_set_dep(struct stiryy_main *stirmain, const char *curprefix, const char *dep, int rec, int orderonly, int wait)
{
  struct stiryyrule *rule = &stirmain->rules[stirmain->rulesz - 1];
  size_t newcapacity;
  size_t sz = strlen(curprefix) + strlen(dep) + 2;
  char *can, *tmp = malloc(sz);
  if (dep[0] == '/')
  {
    if (snprintf(tmp, sz, "%s", dep) >= (int)sz)
    {
      my_abort();
    }
  }
  else
  {
    if (snprintf(tmp, sz, "%s/%s", curprefix, dep) >= (int)sz)
    {
      my_abort();
    }
  }
  can = canon(tmp);
  free(tmp);
  if (rule->depsz >= rule->depcapacity)
  {
    newcapacity = 2*rule->depcapacity + 1;
    rule->deps = (struct dep*)realloc(rule->deps, sizeof(*rule->deps)*newcapacity);
    rule->depcapacity = newcapacity;
  }
  rule->deps[rule->depsz].name = strdup(can); // Let's copy it to compact it
  rule->deps[rule->depsz].namenodir = strdup(dep);
  rule->deps[rule->depsz].suffix = NULL;
  rule->deps[rule->depsz].percent_special = 0;
  rule->deps[rule->depsz].rec = rec;
  rule->deps[rule->depsz].orderonly = orderonly;
  rule->deps[rule->depsz].wait = wait;
  rule->depsz++;
  free(can);
}

static inline void stiryy_set_patdep(struct stiryy *stiryy, const char *dep, int rec, int orderonly, int wait, int percent_special)
{
  stiryy_main_set_patdep(stiryy->main, stiryy->curprefix, dep, rec, orderonly, wait, percent_special);
}
static inline void stiryy_set_patdep2(struct stiryy *stiryy, const char **dep, int rec, int orderonly, int wait)
{
  stiryy_main_set_patdep2(stiryy->main, stiryy->curprefix, dep, rec, orderonly, wait);
}

static inline void stiryy_set_dep(struct stiryy *stiryy, const char *dep, int rec, int orderonly, int wait)
{
  stiryy_main_set_dep(stiryy->main, stiryy->curprefix, dep, rec, orderonly, wait);
}

static inline void stiryy_main_add_order(struct stiryy_main *stirmain)
{
  size_t newcapacity;
  if (stirmain->ordersz >= stirmain->ordercapacity)
  {
    newcapacity = 2*stirmain->ordercapacity + 1;
    stirmain->orders = (struct stiryyorder*)realloc(stirmain->orders, sizeof(*stirmain->orders)*newcapacity);
    stirmain->ordercapacity = newcapacity;
  }
  stirmain->orders[stirmain->ordersz].rulecnt = 0;
  stirmain->orders[stirmain->ordersz].rules[0] = NULL;
  stirmain->orders[stirmain->ordersz].rules[1] = NULL;
  stirmain->ordersz++;
}


static inline void stiryy_add_order(struct stiryy *stiryy)
{
  stiryy_main_add_order(stiryy->main);
}

static inline void stiryy_set_order(struct stiryy *stiryy, const char *name)
{
  stiryy_main_set_order(stiryy->main, stiryy->curprefix, name);
}

static inline void stiryy_main_set_cleanhooktgt(struct stiryy_main *stirmain, const char *curprefix, const char *tgt)
{
  struct stiryyrule *rule = &stirmain->rules[stirmain->rulesz - 1];
  size_t newcapacity;
  size_t sz = strlen(curprefix) + strlen(tgt) + 2;
  char *can, *tmp = malloc(sz);
  char *slashes;
  size_t slashessz;
  char *slashesnodir;
  size_t slashesnodirsz;
  if (snprintf(tmp, sz, "%s/%s", curprefix, tgt) >= (int)sz)
  {
    my_abort();
  }
  can = canon(tmp);
  free(tmp);
  slashessz = strlen(can) + 4;
  slashes = malloc(slashessz);
  if (snprintf(slashes, slashessz, "%s///", can) >= (int)slashessz)
  {
    my_abort();
  }

  slashesnodirsz = strlen(tgt) + 4;
  slashesnodir = malloc(slashesnodirsz);
  if (snprintf(slashesnodir, slashesnodirsz, "%s///", can) >= (int)slashesnodirsz)
  {
    my_abort();
  }
  free(can);

  if (rule->targetsz >= rule->targetcapacity)
  {
    newcapacity = 2*rule->targetcapacity + 1;
    rule->targets = (struct tgt*)realloc(rule->targets, sizeof(*rule->targets)*newcapacity);
    rule->targetcapacity = newcapacity;
  }
  rule->targets[rule->targetsz].name = strdup(slashes);
  rule->targets[rule->targetsz].namenodir = strdup(slashesnodir);
  rule->targets[rule->targetsz].suffix = NULL;
  rule->targets[rule->targetsz].percent_special = 0;
  rule->targetsz++;
  free(slashes);
  free(slashesnodir);
  rule->phony = 1;
  if (strcmp(tgt, "CLEAN") == 0)
  {
    rule->iscleanhook = 1;
  }
  else if (strcmp(tgt, "DISTCLEAN") == 0)
  {
    rule->isdistcleanhook = 1;
  }
  else if (strcmp(tgt, "BOTHCLEAN") == 0)
  {
    rule->isbothcleanhook = 1;
  }
  else
  {
    my_abort();
  }
}

static inline void stiryy_main_set_pattgt(struct stiryy_main *stirmain, const char *curprefix, const char *tgt, int is_dist, int percent_special)
{
  struct stiryyrule *rule = &stirmain->rules[stirmain->rulesz - 1];
  size_t newcapacity;
  size_t sz = strlen(curprefix) + strlen(tgt) + 2;
  char *can, *tmp = malloc(sz);
  if (!rule->ispat)
  {
    printf("rule is not pat\n");
    abort();
  }
  if (rule->patfrozen)
  {
    if (tgt[0] == '/')
    {
      if (snprintf(tmp, sz, "%s", tgt) >= (int)sz)
      {
        my_abort();
      }
    }
#if 0
    else if (tgt[0] == '%')
    {
      if (snprintf(tmp, sz, "%s", tgt) >= (int)sz)
      {
        my_abort();
      }
    }
#endif
    else
    {
      if (snprintf(tmp, sz, "%s/%s", curprefix, tgt) >= (int)sz)
      {
        my_abort();
      }
    }
    can = canon(tmp);
    free(tmp);
    if (rule->targetsz >= rule->targetcapacity)
    {
      newcapacity = 2*rule->targetcapacity + 1;
      rule->targets = (struct tgt*)realloc(rule->targets, sizeof(*rule->targets)*newcapacity);
      rule->targetcapacity = newcapacity;
    }
    rule->targets[rule->targetsz].name = strdup(can);
    rule->targets[rule->targetsz].namenodir = strdup(tgt);
    rule->targets[rule->targetsz].is_dist = !!is_dist;
    rule->targets[rule->targetsz].suffix = NULL;
    rule->targets[rule->targetsz].percent_special = percent_special;
    rule->targetsz++;
    free(can);
  }
  else
  {
    if (is_dist)
    {
      printf("pattern rule bases cannot contain @disttgt\n");
      my_abort();
    }
    if (tgt[0] == '/')
    {
      if (snprintf(tmp, sz, "%s", tgt) >= (int)sz)
      {
        my_abort();
      }
    }
    else
    {
      if (snprintf(tmp, sz, "%s/%s", curprefix, tgt) >= (int)sz)
      {
        my_abort();
      }
    }
    can = canon(tmp);
    free(tmp);
    if (rule->basesz >= rule->basecapacity)
    {
      newcapacity = 2*rule->basecapacity + 1;
      rule->bases = (struct tgt*)realloc(rule->bases, sizeof(*rule->bases)*newcapacity);
      rule->basecapacity = newcapacity;
    }
    rule->bases[rule->basesz].name = strdup(can);
    rule->bases[rule->basesz].namenodir = strdup(tgt);
    rule->basesz++;
    free(can);
  }
}
static inline void stiryy_main_set_pattgt2(struct stiryy_main *stirmain, const char *curprefix, const char **tgt, int is_dist)
{
  struct stiryyrule *rule = &stirmain->rules[stirmain->rulesz - 1];
  size_t newcapacity;
  size_t sz1 = strlen(curprefix) + 1 + strlen(tgt[0]) + 1;
  size_t sz2 = strlen(tgt[2]) + 1;
  char *can, *tmp1 = malloc(sz1), *tmp2 = malloc(sz2);
  size_t off = 0;
  if (!rule->ispat)
  {
    printf("rule is not pat\n");
    abort();
  }
  if (rule->patfrozen)
  {
    if (tgt[0][0] == '/')
    {
      if (snprintf(tmp1, sz1, "%s", tgt[0]) >= (int)sz1)
      {
        my_abort();
      }
      if (snprintf(tmp2, sz2, "%s", tgt[2]) >= (int)sz2)
      {
        my_abort();
      }
    }
    else
    {
      off = strlen(curprefix) + 1;
      if (snprintf(tmp1, sz1, "%s/%s", curprefix, tgt[0]) >= (int)sz1)
      {
        my_abort();
      }
      if (snprintf(tmp2, sz2, "%s", tgt[2]) >= (int)sz2)
      {
        my_abort();
      }
    }
    can = canon(tmp1);
    if (rule->targetsz >= rule->targetcapacity)
    {
      newcapacity = 2*rule->targetcapacity + 1;
      rule->targets = (struct tgt*)realloc(rule->targets, sizeof(*rule->targets)*newcapacity);
      rule->targetcapacity = newcapacity;
    }
    rule->targets[rule->targetsz].name = strdup(can);
    rule->targets[rule->targetsz].namenodir = strdup(tmp1+off);
    rule->targets[rule->targetsz].is_dist = !!is_dist;
    rule->targets[rule->targetsz].suffix = strdup(tmp2);
    rule->targets[rule->targetsz].percent_special = 0;
    rule->targetsz++;
    free(can);
    free(tmp1);
    free(tmp2);
  }
  else
  {
    printf("Pattern rule bases cannot contain @nil lists\n");
    exit(2);
  }
}

static inline void stiryy_main_set_tgt(struct stiryy_main *stirmain, const char *curprefix, const char *tgt, int is_dist)
{
  struct stiryyrule *rule = &stirmain->rules[stirmain->rulesz - 1];
  size_t newcapacity;
  size_t sz = strlen(curprefix) + strlen(tgt) + 2;
  char *can, *tmp = malloc(sz);
  if (tgt[0] == '/')
  {
    if (snprintf(tmp, sz, "%s", tgt) >= (int)sz)
    {
      my_abort();
    }
  }
  else
  {
    if (snprintf(tmp, sz, "%s/%s", curprefix, tgt) >= (int)sz)
    {
      my_abort();
    }
  }
  can = canon(tmp);
  free(tmp);
  if (rule->targetsz >= rule->targetcapacity)
  {
    newcapacity = 2*rule->targetcapacity + 1;
    rule->targets = (struct tgt*)realloc(rule->targets, sizeof(*rule->targets)*newcapacity);
    rule->targetcapacity = newcapacity;
  }
  rule->targets[rule->targetsz].name = strdup(can);
  rule->targets[rule->targetsz].namenodir = strdup(tgt);
  rule->targets[rule->targetsz].is_dist = !!is_dist;
  rule->targets[rule->targetsz].suffix = NULL;
  rule->targets[rule->targetsz].percent_special = 0;
  rule->targetsz++;
  free(can);
}

static inline void stiryy_set_pattgt(struct stiryy *stiryy, const char *tgt, int is_dist, int percent_special)
{
  stiryy_main_set_pattgt(stiryy->main, stiryy->curprefix, tgt, is_dist, percent_special);
}
static inline void stiryy_set_pattgt2(struct stiryy *stiryy, const char **tgt, int is_dist)
{
  stiryy_main_set_pattgt2(stiryy->main, stiryy->curprefix, tgt, is_dist);
}

static inline void stiryy_set_tgt(struct stiryy *stiryy, const char *tgt, int is_dist)
{
  stiryy_main_set_tgt(stiryy->main, stiryy->curprefix, tgt, is_dist);
}

static inline void stiryy_set_cleanhooktgt(struct stiryy *stiryy, const char *tgt)
{
  stiryy_main_set_cleanhooktgt(stiryy->main, stiryy->curprefix, tgt);
}

static inline void stiryy_add_shell(struct stiryy *stiryy, const char *shell)
{
  struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
  size_t newcapacity;
  struct cmdsrcitem *item = &rule->shells.items[rule->shells.itemsz - 1];
  if (rule->shells.itemsz == 0)
  {
    abort();
  }
  if (item->sz >= item->capacity)
  {
    newcapacity = 2*item->capacity + 2;
    item->u.args = realloc(item->u.args, sizeof(*item->u.args)*newcapacity);
    item->capacity = newcapacity;
  }
  item->u.args[item->sz++] = shell ? strdup(shell) : NULL;
  item->u.args[item->sz] = NULL;
}

static inline void stiryy_add_shell_attab(struct stiryy *stiryy, size_t locidx,
                                          int ignore, int noecho, int ismake)
{
  struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
  size_t newcapacity;
  struct cmdsrc *cmdsrc = &rule->shells;
  if (cmdsrc->itemsz >= cmdsrc->itemcapacity)
  {
    newcapacity = 2*cmdsrc->itemcapacity + 2;
    cmdsrc->items = realloc(cmdsrc->items, sizeof(*cmdsrc->items)*newcapacity);
    cmdsrc->itemcapacity = newcapacity;
  }
  //printf("section\n");
  cmdsrc->items[cmdsrc->itemsz].merge = 0;
  cmdsrc->items[cmdsrc->itemsz].iscode = 1;
  cmdsrc->items[cmdsrc->itemsz].isfun = 0;
  cmdsrc->items[cmdsrc->itemsz].sz = 0;
  cmdsrc->items[cmdsrc->itemsz].capacity = 0;
  cmdsrc->items[cmdsrc->itemsz].ignore = !!ignore;
  cmdsrc->items[cmdsrc->itemsz].noecho = !!noecho;
  cmdsrc->items[cmdsrc->itemsz].ismake = !!ismake;
  cmdsrc->items[cmdsrc->itemsz].u.locidx = locidx;
  cmdsrc->itemsz++;
}
static inline void stiryy_add_shell_atattab(struct stiryy *stiryy,
                                            size_t locidx,
                                            int ignore, int noecho, int ismake)
{
  struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
  size_t newcapacity;
  struct cmdsrc *cmdsrc = &rule->shells;
  if (cmdsrc->itemsz >= cmdsrc->itemcapacity)
  {
    newcapacity = 2*cmdsrc->itemcapacity + 2;
    cmdsrc->items = realloc(cmdsrc->items, sizeof(*cmdsrc->items)*newcapacity);
    cmdsrc->itemcapacity = newcapacity;
  }
  //printf("section\n");
  cmdsrc->items[cmdsrc->itemsz].merge = 1;
  cmdsrc->items[cmdsrc->itemsz].iscode = 1;
  cmdsrc->items[cmdsrc->itemsz].isfun = 0;
  cmdsrc->items[cmdsrc->itemsz].sz = 0;
  cmdsrc->items[cmdsrc->itemsz].capacity = 0;
  cmdsrc->items[cmdsrc->itemsz].ignore = !!ignore;
  cmdsrc->items[cmdsrc->itemsz].noecho = !!noecho;
  cmdsrc->items[cmdsrc->itemsz].ismake = !!ismake;
  cmdsrc->items[cmdsrc->itemsz].u.locidx = locidx;
  cmdsrc->itemsz++;
}

static inline void stiryy_add_shell_section(struct stiryy *stiryy)
{
  struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
  size_t newcapacity;
  struct cmdsrc *cmdsrc = &rule->shells;
  if (cmdsrc->itemsz >= cmdsrc->itemcapacity)
  {
    newcapacity = 2*cmdsrc->itemcapacity + 2;
    cmdsrc->items = realloc(cmdsrc->items, sizeof(*cmdsrc->items)*newcapacity);
    cmdsrc->itemcapacity = newcapacity;
  }
  //printf("section\n");
  cmdsrc->items[cmdsrc->itemsz].merge = 0;
  cmdsrc->items[cmdsrc->itemsz].iscode = 0;
  cmdsrc->items[cmdsrc->itemsz].isfun = 0;
  cmdsrc->items[cmdsrc->itemsz].sz = 0;
  cmdsrc->items[cmdsrc->itemsz].capacity = 0;
  cmdsrc->items[cmdsrc->itemsz].ignore = 0;
  cmdsrc->items[cmdsrc->itemsz].noecho = 0;
  cmdsrc->items[cmdsrc->itemsz].ismake = 0;
  cmdsrc->items[cmdsrc->itemsz].u.args = NULL;
  cmdsrc->itemsz++;
}

static inline void stiryy_main_emplace_rule(struct stiryy_main *stirmain, const char *curprefix, size_t scopeidx, int lineno)
{
  size_t newcapacity;
  if (stirmain->rulesz >= stirmain->rulecapacity)
  {
    newcapacity = 2*stirmain->rulecapacity + 1;
    stirmain->rules = (struct stiryyrule*)realloc(stirmain->rules, sizeof(*stirmain->rules)*newcapacity);
    stirmain->rulecapacity = newcapacity;
  }
  stirmain->rule_in_progress = 1;
  stirmain->rules[stirmain->rulesz].basesz = 0;
  stirmain->rules[stirmain->rulesz].basecapacity = 0;
  stirmain->rules[stirmain->rulesz].bases = NULL;
  stirmain->rules[stirmain->rulesz].depsz = 0;
  stirmain->rules[stirmain->rulesz].depcapacity = 0;
  stirmain->rules[stirmain->rulesz].deps = NULL;
  stirmain->rules[stirmain->rulesz].targetsz = 0;
  stirmain->rules[stirmain->rulesz].targetcapacity = 0;
  stirmain->rules[stirmain->rulesz].targets = NULL;
  stirmain->rules[stirmain->rulesz].shells.items = NULL;
  stirmain->rules[stirmain->rulesz].shells.itemsz = 0;
  stirmain->rules[stirmain->rulesz].shells.itemcapacity = 0;
  stirmain->rules[stirmain->rulesz].prefix = strdup(curprefix);
  stirmain->rules[stirmain->rulesz].phony = 0;
  stirmain->rules[stirmain->rulesz].rectgt = 0;
  stirmain->rules[stirmain->rulesz].detouch = 0;
  stirmain->rules[stirmain->rulesz].maybe = 0;
  stirmain->rules[stirmain->rulesz].dist = 0;
  stirmain->rules[stirmain->rulesz].deponly = 0;
  stirmain->rules[stirmain->rulesz].iscleanhook = 0;
  stirmain->rules[stirmain->rulesz].isdistcleanhook = 0;
  stirmain->rules[stirmain->rulesz].isbothcleanhook = 0;
  stirmain->rules[stirmain->rulesz].ispat = 0;
  stirmain->rules[stirmain->rulesz].patfrozen = 0;
  stirmain->rules[stirmain->rulesz].scopeidx = scopeidx;
  stirmain->rules[stirmain->rulesz].lineno = lineno;
  stirmain->rulesz++;
}
static inline void stiryy_main_emplace_patrule(struct stiryy_main *stirmain, const char *curprefix, size_t scopeidx, int lineno)
{
  stiryy_main_emplace_rule(stirmain, curprefix, scopeidx, lineno);
  stirmain->rules[stirmain->rulesz-1].ispat = 1;
}
static inline void stiryy_main_freeze_patrule(struct stiryy_main *stirmain)
{
  if (stirmain->rulesz == 0 || !stirmain->rules[stirmain->rulesz-1].ispat)
  {
    abort();
  }
  stirmain->rules[stirmain->rulesz-1].patfrozen = 1;
}

static inline void stiryy_emplace_rule(struct stiryy *stiryy, size_t scopeidx, int lineno)
{
  stiryy_main_emplace_rule(stiryy->main, stiryy->curprefix, scopeidx, lineno);
}
static inline void stiryy_emplace_patrule(struct stiryy *stiryy, size_t scopeidx, int lineno)
{
  stiryy_main_emplace_patrule(stiryy->main, stiryy->curprefix, scopeidx, lineno);
}
static inline void stiryy_freeze_patrule(struct stiryy *stiryy)
{
  stiryy_main_freeze_patrule(stiryy->main);
}

static inline void stiryy_mark_phony(struct stiryy *stiryy)
{
  stiryy->main->rules[stiryy->main->rulesz-1].phony = 1;
}
static inline void stiryy_mark_dist(struct stiryy *stiryy)
{
  stiryy->main->rules[stiryy->main->rulesz-1].dist = 1;
}
static inline void stiryy_mark_maybe(struct stiryy *stiryy)
{
  stiryy->main->rules[stiryy->main->rulesz-1].maybe = 1;
}
static inline void stiryy_mark_rectgt(struct stiryy *stiryy)
{
  stiryy->main->rules[stiryy->main->rulesz-1].rectgt = 1;
}
static inline void stiryy_mark_detouch(struct stiryy *stiryy)
{
  stiryy->main->rules[stiryy->main->rulesz-1].detouch = 1;
}
static inline int stiryy_check_rule(struct stiryy *stiryy)
{
  struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
  size_t j ;
  if (rule->rectgt || rule->detouch)
  {
    return 0;
  }
  for (j = 0; j < rule->depsz; j++)
  {
    const char *can = rule->deps[j].name;
    if (rule->deps[j].rec)
    {
      size_t i;
      for (i = 0; i < rule->targetsz; i++)
      {
        if (   strncmp(can, rule->targets[i].name, strlen(can)) == 0
            && (   rule->targets[i].name[strlen(can)] == '\0'
                || rule->targets[i].name[strlen(can)] == '/'))
        {
          return -1;
        }
      }
    }
  }
  return 0;
}
static inline void stiryy_main_mark_deponly(struct stiryy_main *stirmain)
{
  stirmain->rules[stirmain->rulesz-1].deponly = 1;
}
static inline void stiryy_mark_deponly(struct stiryy *stiryy)
{
  stiryy_main_mark_deponly(stiryy->main);
}

static inline void stiryy_main_free(struct stiryy_main *stirmain)
{
  size_t i;
  size_t j;
  for (i = 0; i < stirmain->rulesz; i++)
  {
    for (j = 0; j < stirmain->rules[i].depsz; j++)
    {
      free(stirmain->rules[i].deps[j].name);
      free(stirmain->rules[i].deps[j].namenodir);
      free(stirmain->rules[i].deps[j].suffix);
    }
    for (j = 0; j < stirmain->rules[i].targetsz; j++)
    {
      free(stirmain->rules[i].targets[j].name);
      free(stirmain->rules[i].targets[j].namenodir);
      free(stirmain->rules[i].targets[j].suffix);
    }
    for (j = 0; j < stirmain->rules[i].basesz; j++)
    {
      free(stirmain->rules[i].bases[j].name);
      free(stirmain->rules[i].bases[j].namenodir);
    }
    free(stirmain->rules[i].bases);
    free(stirmain->rules[i].deps);
    free(stirmain->rules[i].targets);
    free(stirmain->rules[i].prefix);
    for (j = 0; j < stirmain->rules[i].shells.itemsz; j++)
    {
      if (stirmain->rules[i].shells.items[j].isfun)
      {
        // nop
      }
      else if (stirmain->rules[i].shells.items[j].iscode)
      {
        // nop
      }
      else if (!stirmain->rules[i].shells.items[j].merge)
      {
        size_t k;
        for (k = 0; k < stirmain->rules[i].shells.items[j].sz; k++)
        {
          free(stirmain->rules[i].shells.items[j].u.args[k]);
        }
        free(stirmain->rules[i].shells.items[j].u.args);
      }
      else
      {
        size_t k, l;
        for (k = 0; k < stirmain->rules[i].shells.items[j].sz; k++)
        //for (k = 0; stirmain->rules[i].shells.items[j].u.cmds[k] != NULL; k++)
        {
	  for (l = 0; stirmain->rules[i].shells.items[j].u.cmds[k][l] != NULL; l++)
	  {
            free(stirmain->rules[i].shells.items[j].u.cmds[k][l]);
	  }
	  free(stirmain->rules[i].shells.items[j].u.cmds[k]);
        }
        free(stirmain->rules[i].shells.items[j].u.cmds);
      }
    }
    free(stirmain->rules[i].shells.items);
  }
  free(stirmain->rules);
  for (i = 0; i < stirmain->ordersz; i++)
  {
    free(stirmain->orders[i].rules[0]);
    free(stirmain->orders[i].rules[1]);
  }
  free(stirmain->orders);

  for (i = 0; i < stirmain->cdepincludesz; i++)
  {
    free(stirmain->cdepincludes[i].name);
    free(stirmain->cdepincludes[i].prefix);
  }
  free(stirmain->cdepincludes);
  free(stirmain->realpathname);
  stirmain->realpathname = NULL;
}

static inline void stiryy_free(struct stiryy *stiryy)
{
#if 0
  size_t i;
  size_t j;
  for (i = 0; i < stiryy->main->rulesz; i++)
  {
    for (j = 0; j < stiryy->main->rules[i].depsz; j++)
    {
      free(stiryy->rules[i].deps[j].name);
    }
    for (j = 0; j < stiryy->main->rules[i].targetsz; j++)
    {
      free(stiryy->rules[i].targets[j]);
    }
    free(stiryy->main->rules[i].deps);
    free(stiryy->main->rules[i].targets);
  }
  free(stiryy->rules);
  free(stiryy->bytecode);
#endif
  free(stiryy->curprefix);
  free(stiryy->curprojprefix);
  memset(stiryy, 0, sizeof(*stiryy));
}

int do_dirinclude(struct stiryy *stiryy, int noproj, const char *fname, const char *scopevarname);

int do_fileinclude(struct stiryy *stiryy, const char *fname, int ignore);

#ifdef __cplusplus
};
#endif

#endif
