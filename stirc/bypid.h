#ifndef _BYPID_H_
#define _BYPID_H_

#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "stiryy.h"
#include "const.h"

int ruleid_by_pid_erase(pid_t pid, int *fd);
int ruleid_by_fd(int fd);
void ruleid_by_pid_insert(int ruleid, pid_t pid, int outpiperd);
void kill_all_children(int signum);

struct ruleid_by_pid {
  struct abce_rb_tree_node node;
  struct abce_rb_tree_node fdnode;
  struct linked_list_node llnode;
  pid_t pid;
  int ruleid;
  int fd;
};

static inline int ruleid_by_pid_fd_cmp_asym(const void *fdv, struct abce_rb_tree_node *n2, void *ud)
{
  const int *fd = fdv;
  struct ruleid_by_pid *e = ABCE_CONTAINER_OF(n2, struct ruleid_by_pid, fdnode);
  if (*fd < 0 || e->fd < 0)
  {
    my_abort();
  }
  if (*fd > e->fd)
  {
    return 1;
  }
  if (*fd < e->fd)
  {
    return -1;
  }
  return 0;
}

static inline int ruleid_by_pid_fd_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_pid *e1 = ABCE_CONTAINER_OF(n1, struct ruleid_by_pid, fdnode);
  struct ruleid_by_pid *e2 = ABCE_CONTAINER_OF(n2, struct ruleid_by_pid, fdnode);
  if (e1->fd < 0 || e2->fd < 0)
  {
    my_abort();
  }
  if (e1->fd > e2->fd)
  {
    return 1;
  }
  if (e1->fd < e2->fd)
  {
    return -1;
  }
  return 0;
}

static inline int ruleid_by_pid_cmp_asym(const void *pidv, struct abce_rb_tree_node *n2, void *ud)
{
  const pid_t *pid = pidv;
  struct ruleid_by_pid *e = ABCE_CONTAINER_OF(n2, struct ruleid_by_pid, node);
  if (*pid > e->pid)
  {
    return 1;
  }
  if (*pid < e->pid)
  {
    return -1;
  }
  return 0;
}

static inline int ruleid_by_pid_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct ruleid_by_pid *e1 = ABCE_CONTAINER_OF(n1, struct ruleid_by_pid, node);
  struct ruleid_by_pid *e2 = ABCE_CONTAINER_OF(n2, struct ruleid_by_pid, node);
  if (e1->pid > e2->pid)
  {
    return 1;
  }
  if (e1->pid < e2->pid)
  {
    return -1;
  }
  return 0;
}

extern struct abce_rb_tree_nocmp ruleid_by_pid[RULEID_BY_PID_SIZE];
extern struct abce_rb_tree_nocmp ruleid_by_pid_fd[RULEID_BY_PID_SIZE];
extern struct linked_list_head ruleid_by_pid_list;
extern mysize_t ruleid_by_pid_cnt;

#endif
