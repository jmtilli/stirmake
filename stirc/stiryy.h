#ifndef _STIRYY_H_
#define _STIRYY_H_

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

struct escaped_string {
  size_t sz;
  char *str;
};

struct stiryyrule {
  char **deps;
  size_t depsz;
  size_t depcapacity;
  char **targets;
  size_t targetsz;
  size_t targetcapacity;
};

struct stiryy {
  struct stiryyrule *rules;
  size_t rulesz;
  size_t rulecapacity;
  char **cdepincludes;
  size_t cdepincludesz;
  size_t cdepincludecapacity;
};

static inline void stiryy_set_cdepinclude(struct stiryy *stiryy, const char *cd)
{
  size_t newcapacity;
  if (stiryy->cdepincludesz >= stiryy->cdepincludecapacity)
  {
    newcapacity = 2*stiryy->cdepincludecapacity + 1;
    stiryy->cdepincludes = (char**)realloc(stiryy->cdepincludes, sizeof(*stiryy->cdepincludes)*newcapacity);
    stiryy->cdepincludecapacity = newcapacity;
  }
  stiryy->cdepincludes[stiryy->cdepincludesz++] = strdup(cd);
}

static inline void stiryy_set_dep(struct stiryy *stiryy, const char *dep)
{
  struct stiryyrule *rule = &stiryy->rules[stiryy->rulesz - 1];
  size_t newcapacity;
  if (rule->depsz >= rule->depcapacity)
  {
    newcapacity = 2*rule->depcapacity + 1;
    rule->deps = (char**)realloc(rule->deps, sizeof(*rule->deps)*newcapacity);
    rule->depcapacity = newcapacity;
  }
  rule->deps[rule->depsz++] = strdup(dep);
}

static inline void stiryy_set_tgt(struct stiryy *stiryy, const char *tgt)
{
  struct stiryyrule *rule = &stiryy->rules[stiryy->rulesz - 1];
  size_t newcapacity;
  if (rule->targetsz >= rule->targetcapacity)
  {
    newcapacity = 2*rule->targetcapacity + 1;
    rule->targets = (char**)realloc(rule->targets, sizeof(*rule->targets)*newcapacity);
    rule->targetcapacity = newcapacity;
  }
  rule->targets[rule->targetsz++] = strdup(tgt);
}

static inline void stiryy_emplace_rule(struct stiryy *stiryy)
{
  size_t newcapacity;
  if (stiryy->rulesz >= stiryy->rulecapacity)
  {
    newcapacity = 2*stiryy->rulecapacity + 1;
    stiryy->rules = (struct stiryyrule*)realloc(stiryy->rules, sizeof(*stiryy->rules)*newcapacity);
    stiryy->rulecapacity = newcapacity;
  }
  stiryy->rules[stiryy->rulesz].depsz = 0;
  stiryy->rules[stiryy->rulesz].depcapacity = 0;
  stiryy->rules[stiryy->rulesz].deps = NULL;
  stiryy->rules[stiryy->rulesz].targetsz = 0;
  stiryy->rules[stiryy->rulesz].targetcapacity = 0;
  stiryy->rules[stiryy->rulesz].targets = NULL;
  stiryy->rulesz++;
}

static inline void stiryy_free(struct stiryy *stiryy)
{
  size_t i;
  size_t j;
  for (i = 0; i < stiryy->rulesz; i++)
  {
    for (j = 0; j < stiryy->rules[i].depsz; j++)
    {
      free(stiryy->rules[i].deps[j]);
    }
    for (j = 0; j < stiryy->rules[i].targetsz; j++)
    {
      free(stiryy->rules[i].targets[j]);
    }
    free(stiryy->rules[i].deps);
    free(stiryy->rules[i].targets);
  }
  for (i = 0; i < stiryy->cdepincludesz; i++)
  {
    free(stiryy->cdepincludes[i]);
  }
  free(stiryy->rules);
  free(stiryy->cdepincludes);
  memset(stiryy, 0, sizeof(*stiryy));
}

#endif
