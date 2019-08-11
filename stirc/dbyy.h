#ifndef _DBYY_H_
#define _DBYY_H_

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

struct dbyycmd {
  char **args;
  size_t argssz;
  size_t argscapacity;
};

struct dbyyrule {
  char *dir;
  char *tgt;
  struct dbyycmd *cmds;
  size_t cmdssz;
  size_t cmdscapacity;
};

struct dbyy {
  struct dbyyrule *rules;
  size_t rulesz;
  size_t rulecapacity;
};

static inline void dbyy_add_cmd(struct dbyy *dbyy)
{
  struct dbyyrule *rule = &dbyy->rules[dbyy->rulesz - 1];
  size_t newcapacity;
  if (rule->cmdssz >= rule->cmdscapacity)
  {
    newcapacity = 2*rule->cmdscapacity + 1;
    rule->cmds = (struct dbyycmd*)realloc(rule->cmds, sizeof(*rule->cmds)*newcapacity);
    rule->cmdscapacity = newcapacity;
  }
  rule->cmds[rule->cmdssz].args = NULL;
  rule->cmds[rule->cmdssz].argssz = 0;
  rule->cmds[rule->cmdssz].argscapacity = 0;
  rule->cmdssz++;
}

static inline void dbyy_add_arg(struct dbyy *dbyy, const char *arg)
{
  struct dbyyrule *rule = &dbyy->rules[dbyy->rulesz - 1];
  struct dbyycmd *cmd = &rule->cmds[rule->cmdssz - 1];
  size_t newcapacity;
  if (cmd->argssz >= cmd->argscapacity)
  {
    newcapacity = 2*cmd->argscapacity + 1;
    cmd->args = (char**)realloc(cmd->args, sizeof(*cmd->args)*newcapacity);
    cmd->argscapacity = newcapacity;
  }
  cmd->args[cmd->argssz++] = strdup(arg);
}

static inline void dbyy_emplace_rule(struct dbyy *dbyy, const char *dir, const char *tgt)
{
  size_t newcapacity;
  if (dbyy->rulesz >= dbyy->rulecapacity)
  {
    newcapacity = 2*dbyy->rulecapacity + 1;
    dbyy->rules = (struct dbyyrule*)realloc(dbyy->rules, sizeof(*dbyy->rules)*newcapacity);
    dbyy->rulecapacity = newcapacity;
  }
  dbyy->rules[dbyy->rulesz].cmdssz = 0;
  dbyy->rules[dbyy->rulesz].cmdscapacity = 0;
  dbyy->rules[dbyy->rulesz].cmds = NULL;
  dbyy->rules[dbyy->rulesz].dir = strdup(dir);
  dbyy->rules[dbyy->rulesz].tgt = strdup(tgt);
  dbyy->rulesz++;
}

static inline void dbyy_free(struct dbyy *dbyy)
{
  size_t i;
  size_t j;
  size_t k;
  for (i = 0; i < dbyy->rulesz; i++)
  {
    for (j = 0; j < dbyy->rules[i].cmdssz; j++)
    {
      for (k = 0; k < dbyy->rules[i].cmds[j].argssz; k++)
      {
        free(dbyy->rules[i].cmds[j].args[k]);
      }
      free(dbyy->rules[i].cmds[j].args);
    }
    free(dbyy->rules[i].cmds);
    free(dbyy->rules[i].dir);
    free(dbyy->rules[i].tgt);
  }
  free(dbyy->rules);
  memset(dbyy, 0, sizeof(*dbyy));
}

#ifdef __cplusplus
};
#endif

#endif
