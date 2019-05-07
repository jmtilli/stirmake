#ifndef _STIRYY_H_
#define _STIRYY_H_

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

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

struct stiryyrule {
  char **deps;
  size_t depsz;
  size_t depcapacity;
  char **targets;
  size_t targetsz;
  size_t targetcapacity;
};

struct stiryy {
  void *baton;
  uint8_t *bytecode;
  size_t bytecapacity;
  size_t bytesz;
  struct stiryyrule *rules;
  size_t rulesz;
  size_t rulecapacity;
  char **cdepincludes;
  size_t cdepincludesz;
  size_t cdepincludecapacity;
};

size_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen);
size_t stiryy_add_fun_sym(struct stiryy *stiryy, const char *symbol, int maybe, size_t loc);

static inline void stiryy_add_byte(struct stiryy *stiryy, uint8_t byte)
{
  size_t newcapacity;
  if (stiryy->bytesz >= stiryy->bytecapacity)
  {
    newcapacity = 2*stiryy->bytecapacity + 1;
    stiryy->bytecode = (uint8_t*)realloc(stiryy->bytecode, sizeof(*stiryy->bytecode)*newcapacity);
    stiryy->bytecapacity = newcapacity;
  }
  stiryy->bytecode[stiryy->bytesz++] = byte; 
}

static inline void stiryy_add_double(struct stiryy *stiryy, double dbl)
{
  uint64_t val;
  memcpy(&val, &dbl, 8);
  stiryy_add_byte(stiryy, val>>56);
  stiryy_add_byte(stiryy, val>>48);
  stiryy_add_byte(stiryy, val>>40);
  stiryy_add_byte(stiryy, val>>32);
  stiryy_add_byte(stiryy, val>>24);
  stiryy_add_byte(stiryy, val>>16);
  stiryy_add_byte(stiryy, val>>8);
  stiryy_add_byte(stiryy, val);
}

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
  free(stiryy->bytecode);
  free(stiryy->rules);
  free(stiryy->cdepincludes);
  memset(stiryy, 0, sizeof(*stiryy));
}

#ifdef __cplusplus
};
#endif

#endif
