#ifndef _DBYY_H_
#define _DBYY_H_

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

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

struct tsdbentry {
  char *dir;
  char *tgt;
  off_t filesz;
  struct timespec ts;
};

struct dbyy {
  struct dbyyrule *rules;
  struct tsdbentry *tsdb;
  size_t rulesz;
  size_t rulecapacity;
  size_t tssz;
  size_t tscapacity;
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

static inline void dbyy_emplace_tsdb(struct dbyy *dbyy, const char *tgt, off_t filesz, time_t sec, long nsec)
{
  size_t newcapacity;
  if (dbyy->tssz >= dbyy->tscapacity)
  {
    newcapacity = 2*dbyy->tscapacity + 1;
    dbyy->tsdb = (struct tsdbentry*)realloc(dbyy->tsdb, sizeof(*dbyy->tsdb)*newcapacity);
    dbyy->tscapacity = newcapacity;
  }
  dbyy->tsdb[dbyy->tssz].tgt = strdup(tgt);
  dbyy->tsdb[dbyy->tssz].filesz = filesz;
  dbyy->tsdb[dbyy->tssz].ts.tv_sec = sec;
  dbyy->tsdb[dbyy->tssz].ts.tv_nsec = nsec;
  dbyy->tssz++;
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
  for (i = 0; i < dbyy->tssz; i++)
  {
    free(dbyy->tsdb[i].dir);
    free(dbyy->tsdb[i].tgt);
  }
  free(dbyy->tsdb);
  memset(dbyy, 0, sizeof(*dbyy));
}

#ifdef __cplusplus
};
#endif

#endif
