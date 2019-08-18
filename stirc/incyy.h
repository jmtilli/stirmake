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

#ifdef __cplusplus
extern "C" {
#endif

struct incyyrule {
  char **deps;
  size_t depsz;
  size_t depcapacity;
  char **targets;
  size_t targetsz;
  size_t targetcapacity;
};

struct incyy {
  struct incyyrule *rules;
  size_t rulesz;
  size_t rulecapacity;
  char *prefix;
};

void my_abort(void);

static inline void incyy_set_dep(struct incyy *incyy, const char *dep)
{
  struct incyyrule *rule = &incyy->rules[incyy->rulesz - 1];
  size_t newcapacity;
  size_t sz = strlen(incyy->prefix) + strlen(dep) + 2;
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
    if (snprintf(tmp, sz, "%s/%s", incyy->prefix, dep) >= sz)
    {
      my_abort();
    }
  }
  can = canon(tmp);
  free(tmp);

  if (rule->depsz >= rule->depcapacity)
  {
    newcapacity = 2*rule->depcapacity + 1;
    rule->deps = (char**)realloc(rule->deps, sizeof(*rule->deps)*newcapacity);
    rule->depcapacity = newcapacity;
  }
  rule->deps[rule->depsz++] = strdup(can);
  free(can);
}

static inline void incyy_set_tgt(struct incyy *incyy, const char *tgt)
{
  struct incyyrule *rule = &incyy->rules[incyy->rulesz - 1];
  size_t newcapacity;
  size_t sz = strlen(incyy->prefix) + strlen(tgt) + 2;
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
    if (snprintf(tmp, sz, "%s/%s", incyy->prefix, tgt) >= sz)
    {
      my_abort();
    }
  }
  can = canon(tmp);
  free(tmp);

  if (rule->targetsz >= rule->targetcapacity)
  {
    newcapacity = 2*rule->targetcapacity + 1;
    rule->targets = (char**)realloc(rule->targets, sizeof(*rule->targets)*newcapacity);
    rule->targetcapacity = newcapacity;
  }
  rule->targets[rule->targetsz++] = strdup(can);
  free(can);
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
  incyy->rules[incyy->rulesz].depcapacity = 0;
  incyy->rules[incyy->rulesz].deps = NULL;
  incyy->rules[incyy->rulesz].targetsz = 0;
  incyy->rules[incyy->rulesz].targetcapacity = 0;
  incyy->rules[incyy->rulesz].targets = NULL;
  incyy->rulesz++;
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
    }
    for (j = 0; j < incyy->rules[i].targetsz; j++)
    {
      free(incyy->rules[i].targets[j]);
    }
    free(incyy->rules[i].deps);
    free(incyy->rules[i].targets);
  }
  free(incyy->rules);
  memset(incyy, 0, sizeof(*incyy));
}

#ifdef __cplusplus
};
#endif

#endif
