#ifndef _INCYY_H_
#define _INCYY_H_

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

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
};

static inline void incyy_set_dep(struct incyy *incyy, const char *dep)
{
  struct incyyrule *rule = &incyy->rules[incyy->rulesz - 1];
  size_t newcapacity;
  if (rule->depsz >= rule->depcapacity)
  {
    newcapacity = 2*rule->depcapacity + 1;
    rule->deps = (char**)realloc(rule->deps, sizeof(*rule->deps)*newcapacity);
    rule->depcapacity = newcapacity;
  }
  rule->deps[rule->depsz++] = strdup(dep);
}

static inline void incyy_set_tgt(struct incyy *incyy, const char *tgt)
{
  struct incyyrule *rule = &incyy->rules[incyy->rulesz - 1];
  size_t newcapacity;
  if (rule->targetsz >= rule->targetcapacity)
  {
    newcapacity = 2*rule->targetcapacity + 1;
    rule->targets = (char**)realloc(rule->targets, sizeof(*rule->targets)*newcapacity);
    rule->targetcapacity = newcapacity;
  }
  rule->targets[rule->targetsz++] = strdup(tgt);
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

#endif
