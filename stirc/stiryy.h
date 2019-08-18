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
#include "abce/abce.h"
#include "canon.h"
#include "abce/abcescopes.h"

#ifdef __cplusplus
extern "C" {
#endif

void my_abort(void);

struct escaped_string {
  size_t sz;
  char *str;
};

struct CSnippet {
  char *data;
  size_t len;
  size_t capacity;
};

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

struct dep {
  char *name;
  char *namenodir;
  int rec;
  int orderonly;
};
struct tgt {
  char *name;
  char *namenodir;
};

struct stiryyrule {
  struct dep *deps;
  size_t depsz;
  size_t depcapacity;
  struct tgt *targets;
  size_t targetsz;
  size_t targetcapacity;
  char ***shells;
  size_t shellsz;
  size_t shellcapacity;
  size_t lastshellsz;
  size_t lastshellcapacity;
  size_t scopeidx;
  char *prefix;
  unsigned phony:1;
  unsigned rectgt:1;
  unsigned maybe:1;
  unsigned dist:1;
  unsigned deponly:1;
};

struct stiryy_main {
  struct stiryyrule *rules;
  size_t rulesz;
  size_t rulecapacity;
  struct abce *abce;
  char *realpathname;
  int subdirseen;
  int subdirseen_sameproject;
  int freeform_token_seen;

  struct cdepinclude *cdepincludes;
  size_t cdepincludesz;
  size_t cdepincludecapacity;
};

struct cdepinclude {
  char *name;
  char *prefix;
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
  const char *dirname;
  const char *filename;
};

static inline void init_main_for_realpath(struct stiryy_main *main, char *cwd)
{
  char buf2[PATH_MAX+16];
  char buf3[PATH_MAX+16];
  memset(main, 0, sizeof(*main));
  if (realpath(cwd, buf2) == NULL)
  {
    my_abort();
  }
  if (snprintf(buf3, sizeof(buf3), "%s/Stirfile", buf2) >= sizeof(buf3))
  {
    my_abort();
  }
  main->realpathname = canon(buf3);
  main->subdirseen = 0;
}

static inline void stiryy_init(struct stiryy *yy, struct stiryy_main *main,
                               char *prefix, char *projprefix,
                               struct abce_mb curscope,
                               const char *dirname, const char *filename)
{
  yy->main = main;
  yy->sameproject = 1;
  //abce_init(&yy->abce);
  yy->ctx = NULL;
  yy->curprefix = strdup(prefix);
  yy->curprojprefix = strdup(projprefix);
  yy->curscopeidx = abce_cache_add(yy->main->abce, &curscope); // avoid GC abort
  yy->curscope = curscope;
  yy->filename = filename;
  yy->dirname = dirname;
}

static inline size_t stiryy_symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen)
{
  return abce_cache_add_str(stiryy->main->abce, symbol, symlen);
}
static inline size_t stiryy_add_fun_sym(struct stiryy *stiryy, const char *symbol, int maybe, size_t loc)
{
  struct abce_mb mb;
  struct abce_mb mbold;
  int ret;
  mb.typ = ABCE_T_F;
  mb.u.d = loc;
  ret = abce_sc_put_val_str_maybe_old(stiryy->main->abce, &stiryy->curscope, symbol, &mb, maybe, &mbold);
  if (ret != 0 && ret != -EEXIST)
  {
    printf("can't add symbol %s\n", symbol);
    exit(1);
  }
  if (mbold.typ == ABCE_T_N)
  {
    return (size_t)-1;
  }
  else
  {
    size_t ret = abce_cache_add(stiryy->main->abce, &mbold);
    abce_mb_refdn(stiryy->main->abce, &mbold);
    return ret;
  }
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

size_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen);

static inline void stiryy_set_cdepinclude(struct stiryy *stiryy, const char *cd)
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
  stiryy->main->cdepincludesz++;
}

static inline void stiryy_set_dep(struct stiryy *stiryy, const char *dep, int rec, int orderonly)
{
  struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
  size_t newcapacity;
  size_t sz = strlen(stiryy->curprefix) + strlen(dep) + 2;
  char *can, *tmp = malloc(sz);
  if (dep[0] == '/')
  {
    if (snprintf(tmp, sz, "%s", dep) >= sz)
    {
      my_abort();
    }
  }
  else
  {
    if (snprintf(tmp, sz, "%s/%s", stiryy->curprefix, dep) >= sz)
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
  rule->deps[rule->depsz].rec = rec;
  rule->deps[rule->depsz].orderonly = orderonly;
  rule->depsz++;
  free(can);
}

static inline void stiryy_set_tgt(struct stiryy *stiryy, const char *tgt)
{
  struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
  size_t newcapacity;
  size_t sz = strlen(stiryy->curprefix) + strlen(tgt) + 2;
  char *can, *tmp = malloc(sz);
  if (tgt[0] == '/')
  {
    if (snprintf(tmp, sz, "%s", tgt) >= sz)
    {
      my_abort();
    }
  }
  else
  {
    if (snprintf(tmp, sz, "%s/%s", stiryy->curprefix, tgt) >= sz)
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
  rule->targetsz++;
  free(can);
}

static inline void stiryy_add_shell(struct stiryy *stiryy, const char *shell)
{
  struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
  size_t newcapacity;
  if (rule->lastshellsz >= rule->lastshellcapacity)
  {
    newcapacity = 2*rule->lastshellcapacity + 1;
    rule->shells[rule->shellsz - 1] = (char**)realloc(rule->shells[rule->shellsz - 1], sizeof(*rule->shells[rule->shellsz - 1])*newcapacity);
    rule->lastshellcapacity = newcapacity;
  }
  rule->shells[rule->shellsz - 1][rule->lastshellsz++] = shell ? strdup(shell) : NULL;
}

static inline void stiryy_add_shell_section(struct stiryy *stiryy)
{
  struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
  size_t newcapacity;
  if (rule->shellsz >= rule->shellcapacity)
  {
    newcapacity = 2*rule->shellcapacity + 1;
    rule->shells = (char***)realloc(rule->shells, sizeof(*rule->shells)*newcapacity);
    rule->shellcapacity = newcapacity;
  }
  //printf("section\n");
  rule->shells[rule->shellsz++] = NULL;
  rule->lastshellsz = 0;
  rule->lastshellcapacity = 0;
}

static inline void stiryy_emplace_rule(struct stiryy *stiryy, size_t scopeidx)
{
  size_t newcapacity;
  if (stiryy->main->rulesz >= stiryy->main->rulecapacity)
  {
    newcapacity = 2*stiryy->main->rulecapacity + 1;
    stiryy->main->rules = (struct stiryyrule*)realloc(stiryy->main->rules, sizeof(*stiryy->main->rules)*newcapacity);
    stiryy->main->rulecapacity = newcapacity;
  }
  stiryy->main->rules[stiryy->main->rulesz].depsz = 0;
  stiryy->main->rules[stiryy->main->rulesz].depcapacity = 0;
  stiryy->main->rules[stiryy->main->rulesz].deps = NULL;
  stiryy->main->rules[stiryy->main->rulesz].targetsz = 0;
  stiryy->main->rules[stiryy->main->rulesz].targetcapacity = 0;
  stiryy->main->rules[stiryy->main->rulesz].targets = NULL;
  stiryy->main->rules[stiryy->main->rulesz].shellsz = 0;
  stiryy->main->rules[stiryy->main->rulesz].shellcapacity = 0;
  stiryy->main->rules[stiryy->main->rulesz].shells = NULL;
  stiryy->main->rules[stiryy->main->rulesz].prefix = strdup(stiryy->curprefix);
  stiryy->main->rules[stiryy->main->rulesz].phony = 0;
  stiryy->main->rules[stiryy->main->rulesz].rectgt = 0;
  stiryy->main->rules[stiryy->main->rulesz].maybe = 0;
  stiryy->main->rules[stiryy->main->rulesz].dist = 0;
  stiryy->main->rules[stiryy->main->rulesz].deponly = 0;
  stiryy->main->rules[stiryy->main->rulesz].scopeidx = scopeidx;
  stiryy->main->rulesz++;
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
static inline void stiryy_mark_deponly(struct stiryy *stiryy)
{
  stiryy->main->rules[stiryy->main->rulesz-1].deponly = 1;
}

static inline void stiryy_main_free(struct stiryy_main *main)
{
  size_t i;
  size_t j;
  for (i = 0; i < main->rulesz; i++)
  {
    for (j = 0; j < main->rules[i].depsz; j++)
    {
      free(main->rules[i].deps[j].name);
      free(main->rules[i].deps[j].namenodir);
    }
    for (j = 0; j < main->rules[i].targetsz; j++)
    {
      free(main->rules[i].targets[j].name);
      free(main->rules[i].targets[j].namenodir);
    }
    free(main->rules[i].deps);
    free(main->rules[i].targets);
  }
  free(main->rules);

  for (i = 0; i < main->cdepincludesz; i++)
  {
    free(main->cdepincludes[i].name);
    free(main->cdepincludes[i].prefix);
  }
  free(main->cdepincludes);
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
  memset(stiryy, 0, sizeof(*stiryy));
}

#ifdef __cplusplus
};
#endif

#endif
