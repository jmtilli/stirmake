#ifndef _INCYY_H_
#define _INCYY_H_

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "canon.h"
#include "stirutils.h"
#include "stiryy.h"

#ifdef __cplusplus
extern "C" {
#endif

struct incyyrule {
  char **deps;
  //char **depsnodir;
  char **targets;
  mysize_t depsz;
  mysize_t targetsz;
};

struct incyy {
  struct incyyrule *rules;
  size_t rulesz;
  size_t rulecapacity;
  char *prefix;
  char *fnamenodir;
  mysize_t depcapacity;
  mysize_t targetcapacity;
  unsigned auto_target:1;
};
#define INCYY_EMPTY {.rules = NULL}

void my_abort(void);

static inline void incyy_set_dep(struct incyy *incyy, const char *dep)
{
  char canbuf[1024];
  char catbuf[1024];
  struct incyyrule *rule = &incyy->rules[incyy->rulesz - 1];
  size_t newcapacity;
  char *can, *tmp;

  if (dep[0] == '/')
  {
    tmp = pathcat1_buf(dep, catbuf, sizeof(catbuf));
  }
  else
  {
    tmp = pathcat2_buf(incyy->prefix, dep, catbuf, sizeof(catbuf));
  }
  can = canon_buf(tmp, canbuf, sizeof(canbuf));
  if (tmp != catbuf)
  {
    free(tmp);
  }

  if (rule->depsz >= incyy->depcapacity)
  {
    newcapacity = 2*incyy->depcapacity + 1;
    rule->deps = (char**)realloc(rule->deps, sizeof(*rule->deps)*newcapacity);
    //rule->depsnodir = (char**)realloc(rule->depsnodir, sizeof(*rule->depsnodir)*newcapacity);
    incyy->depcapacity = newcapacity;
  }
  rule->deps[rule->depsz] = stir_strdup(can);
  //rule->depsnodir[rule->depsz] = stir_strdup(dep);
  rule->depsz++;
  if (can != canbuf)
  {
    free(can);
  }
}

static inline void incyy_set_tgt(struct incyy *incyy, const char *tgt)
{
  char canbuf[1024];
  char catbuf[1024];
  struct incyyrule *rule = &incyy->rules[incyy->rulesz - 1];
  size_t newcapacity;
  char *can, *tmp;

  if (tgt[0] == '/')
  {
    tmp = pathcat1_buf(tgt, catbuf, sizeof(catbuf));
  }
  else
  {
    tmp = pathcat2_buf(incyy->prefix, tgt, catbuf, sizeof(catbuf));
  }
  can = canon_buf(tmp, canbuf, sizeof(canbuf));
  if (tmp != catbuf)
  {
    free(tmp);
  }

  if (rule->targetsz >= incyy->targetcapacity)
  {
    newcapacity = 2*incyy->targetcapacity + 1;
    rule->targets = (char**)realloc(rule->targets, sizeof(*rule->targets)*newcapacity);
    incyy->targetcapacity = newcapacity;
  }
  rule->targets[rule->targetsz++] = stir_strdup(can);
  if (can != canbuf)
  {
    free(can);
  }
}

static inline void incyy_emplace_rule(struct incyy *incyy)
{
  size_t newcapacity;
  if (incyy->rulesz >= incyy->rulecapacity)
  {
    newcapacity = 2*incyy->rulecapacity + 1;
    incyy->rules = (struct incyyrule*)realloc(incyy->rules, sizeof(*incyy->rules)*newcapacity);
    incyy->rulecapacity = newcapacity;
  }
  incyy->rules[incyy->rulesz].depsz = 0;
  incyy->depcapacity = 0;
  incyy->rules[incyy->rulesz].deps = NULL;
  //incyy->rules[incyy->rulesz].depsnodir = NULL;
  incyy->rules[incyy->rulesz].targetsz = 0;
  incyy->targetcapacity = 0;
  incyy->rules[incyy->rulesz].targets = NULL;
  incyy->rulesz++;
  if (incyy->auto_target)
  {
    incyy_set_tgt(incyy, incyy->fnamenodir);
  }
}

static inline void incyy_free(struct incyy *incyy)
{
  size_t i;
  size_t j;
  for (i = 0; i < incyy->rulesz; i++)
  {
    for (j = 0; j < incyy->rules[i].depsz; j++)
    {
      free(incyy->rules[i].deps[j]);
      //free(incyy->rules[i].depsnodir[j]);
    }
    for (j = 0; j < incyy->rules[i].targetsz; j++)
    {
      free(incyy->rules[i].targets[j]);
    }
    free(incyy->rules[i].deps);
    //free(incyy->rules[i].depsnodir);
    free(incyy->rules[i].targets);
  }
  free(incyy->rules);
  memset(incyy, 0, sizeof(*incyy));
}

#ifdef __cplusplus
};
#endif

#endif
