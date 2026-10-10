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
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void *my_strdup(const char *str);
void *my_malloc(size_t sz);
void *my_argstrdup(const char *str);
void *my_argmalloc(size_t sz);

struct dbyycmd {
  char **args;
  size_t argssz;
  size_t argscapacity;
};

struct dbyyrule {
  char *dir;
  char *tgt;
  //struct dbyycmd *cmds;
  //size_t cmdssz;
  //size_t cmdscapacity;
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
  struct dbyycmd *cmdsbuf;
  size_t cmdssz;
  size_t cmdsmaxsz;
  size_t cmdscapacity;
  size_t rulesz;
  size_t rulecapacity;
  size_t tssz;
  size_t tscapacity;
};
#define DBYY_EMPTY {.rules = NULL}

static inline void dbyy_add_cmd(struct dbyy *dbyy)
{
  struct dbyyrule *rule = &dbyy->rules[dbyy->rulesz - 1];
  size_t newcapacity;
  if (dbyy->cmdssz >= dbyy->cmdscapacity)
  {
    newcapacity = 2*dbyy->cmdscapacity + 1;
    dbyy->cmdsbuf = (struct dbyycmd*)realloc(dbyy->cmdsbuf, sizeof(*dbyy->cmdsbuf)*newcapacity);
    dbyy->cmdscapacity = newcapacity;
  }
  if (dbyy->cmdssz >= dbyy->cmdsmaxsz)
  {
    dbyy->cmdsbuf[dbyy->cmdssz].args = NULL;
    dbyy->cmdsbuf[dbyy->cmdssz].argscapacity = 0;
    dbyy->cmdsmaxsz++;
  }
  dbyy->cmdsbuf[dbyy->cmdssz].argssz = 0;
#if 0
  rule->cmds[rule->cmdssz].args = NULL;
  rule->cmds[rule->cmdssz].argssz = 0;
  rule->cmds[rule->cmdssz].argscapacity = 0;
#endif
  dbyy->cmdssz++;
}
static inline void dbyy_post_cmd(struct dbyy *dbyy)
{
#if 0
  struct dbyyrule *rule = &dbyy->rules[dbyy->rulesz - 1];
  if (rule->cmdssz > 0)
  {
    void *tmpptr;
    tmpptr = my_argmalloc(sizeof(rule->cmds[rule->cmdssz-1].args[0])*rule->cmds[rule->cmdssz-1].argssz);
    memcpy(tmpptr, rule->cmds[rule->cmdssz-1].args, sizeof(rule->cmds[rule->cmdssz-1].args[0])*rule->cmds[rule->cmdssz-1].argssz);
    free(rule->cmds[rule->cmdssz-1].args);
    rule->cmds[rule->cmdssz-1].args = tmpptr;
  }
#endif
}

typedef void(*dbyy_cmdfn_t)(void *userdata,
                            struct dbyycmd *cmdsbuf, size_t cmdssz,
                            struct dbyyrule *rule);
typedef void(*dbyy_tsfn_t)(void *userdata,
                           struct tsdbentry *e);

static inline void dbyy_post_cmds(struct dbyy *dbyy,
                                  dbyy_cmdfn_t fn,
                                  void *userdata)
{
  struct dbyyrule *rule = &dbyy->rules[dbyy->rulesz - 1];
#if 0
  void *tmpptr;
  tmpptr = my_argmalloc(sizeof(rule->cmds[0])*rule->cmdssz);
  memcpy(tmpptr, rule->cmds, sizeof(rule->cmds[0])*rule->cmdssz);
  free(rule->cmds);
  rule->cmds = tmpptr;
#endif
  fn(userdata, dbyy->cmdsbuf, dbyy->cmdssz, rule);
}

static inline void dbyy_add_arg(struct dbyy *dbyy, const char *arg)
{
  struct dbyyrule *rule = &dbyy->rules[dbyy->rulesz - 1];
  struct dbyycmd *cmd = &dbyy->cmdsbuf[dbyy->cmdssz - 1];
  size_t newcapacity;
  if (cmd->argssz >= cmd->argscapacity)
  {
    newcapacity = 2*cmd->argscapacity + 1;
    cmd->args = (char**)realloc(cmd->args, sizeof(*cmd->args)*newcapacity);
    cmd->argscapacity = newcapacity;
  }
  cmd->args[cmd->argssz++] = my_argstrdup(arg);
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
  dbyy->cmdssz = 0;
  //dbyy->rules[dbyy->rulesz].cmdssz = 0;
  //dbyy->rules[dbyy->rulesz].cmdscapacity = 0;
  //dbyy->rules[dbyy->rulesz].cmds = NULL;
  //dbyy->rules[dbyy->rulesz].dir = my_strdup(dir);
  //dbyy->rules[dbyy->rulesz].tgt = my_strdup(tgt);
  dbyy->rules[0].dir = my_strdup(dir);
  dbyy->rules[0].tgt = my_strdup(tgt);
  dbyy->rulesz = 1;
  //dbyy->rulesz++; // Not needed anymore
}

static inline void dbyy_emplace_tsdb(struct dbyy *dbyy, const char *tgt, off_t filesz, time_t sec, long nsec, dbyy_tsfn_t fn, void *userdata)
{
  size_t newcapacity;
  if (dbyy->tssz >= dbyy->tscapacity)
  {
    newcapacity = 2*dbyy->tscapacity + 1;
    dbyy->tsdb = (struct tsdbentry*)realloc(dbyy->tsdb, sizeof(*dbyy->tsdb)*newcapacity);
    dbyy->tscapacity = newcapacity;
  }
  dbyy->tssz = 0;
  dbyy->tsdb[dbyy->tssz].tgt = my_strdup(tgt);
  dbyy->tsdb[dbyy->tssz].filesz = filesz;
  dbyy->tsdb[dbyy->tssz].ts.tv_sec = sec;
  dbyy->tsdb[dbyy->tssz].ts.tv_nsec = nsec;
  dbyy->tssz++;
  fn(userdata, dbyy->tsdb);
}

#if 0 // No longer valid due to the use of custom non-free-supporting allocator
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
#endif

#ifdef __cplusplus
};
#endif

#endif
