#include "bypid.h"
#include "mymalloc.h"
#include <sys/types.h>
#include <signal.h>

struct abce_rb_tree_nocmp ruleid_by_pid[RULEID_BY_PID_SIZE];
struct abce_rb_tree_nocmp ruleid_by_pid_fd[RULEID_BY_PID_SIZE];
struct linked_list_head ruleid_by_pid_list =
  STIR_LINKED_LIST_HEAD_INITER(ruleid_by_pid_list);

int ruleid_by_fd(int fd)
{
  struct abce_rb_tree_node *n;
  uint32_t hashvalfd;
  size_t hashlocfd;
  struct ruleid_by_pid *bypid;
  if (fd < 0)
  {
    abort();
  }
  hashvalfd = abce_murmur32(HASH_SEED, (uint32_t)fd);
  hashlocfd = hashvalfd % (sizeof(ruleid_by_pid_fd)/sizeof(*ruleid_by_pid_fd));
  n = ABCE_RB_TREE_NOCMP_FIND(&ruleid_by_pid_fd[hashlocfd], ruleid_by_pid_fd_cmp_asym, NULL, &fd);
  if (n == NULL)
  {
    return -ENOENT;
  }
  bypid = ABCE_CONTAINER_OF(n, struct ruleid_by_pid, fdnode);
  return bypid->ruleid;
}

int ruleid_by_pid_erase(pid_t pid, int *fd)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval, hashvalfd;
  size_t hashloc, hashlocfd;
  int ruleid;
  struct ruleid_by_pid *bypid;
  hashval = abce_murmur32(HASH_SEED, (uint32_t)pid);
  hashloc = hashval % (sizeof(ruleid_by_pid)/sizeof(*ruleid_by_pid));
  n = ABCE_RB_TREE_NOCMP_FIND(&ruleid_by_pid[hashloc], ruleid_by_pid_cmp_asym, NULL, &pid);
  if (n == NULL)
  {
    return -ENOENT;
  }
  bypid = ABCE_CONTAINER_OF(n, struct ruleid_by_pid, node);
  abce_rb_tree_nocmp_delete(&ruleid_by_pid[hashloc], &bypid->node);
  if (bypid->fd >= 0)
  {
    hashvalfd = abce_murmur32(HASH_SEED, (uint32_t)bypid->fd);
    hashlocfd = hashvalfd % (sizeof(ruleid_by_pid_fd)/sizeof(*ruleid_by_pid_fd));
    abce_rb_tree_nocmp_delete(&ruleid_by_pid_fd[hashlocfd], &bypid->fdnode);
  }
  ruleid = bypid->ruleid;
  linked_list_delete(&bypid->llnode);
  if (fd)
  {
    *fd = bypid->fd;
  }
  my_free(bypid);
  return ruleid;
}

int ignore_by_pid(pid_t pid)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  struct ruleid_by_pid *bypid;
  hashval = abce_murmur32(HASH_SEED, (uint32_t)pid);
  hashloc = hashval % (sizeof(ruleid_by_pid)/sizeof(*ruleid_by_pid));
  n = ABCE_RB_TREE_NOCMP_FIND(&ruleid_by_pid[hashloc], ruleid_by_pid_cmp_asym, NULL, &pid);
  if (n == NULL)
  {
    return -ENOENT;
  }
  bypid = ABCE_CONTAINER_OF(n, struct ruleid_by_pid, node);
  return !!bypid->ignore;
}

mysize_t ruleid_by_pid_cnt;

void ruleid_by_pid_insert(int ruleid, pid_t pid, int outpiperd, int ignore)
{
  struct ruleid_by_pid *bypid = my_malloc(sizeof(*bypid)); // RFE use malloc() instead?
  uint32_t hashval;
  size_t hashloc;
  uint32_t hashvalfd;
  size_t hashlocfd;
  ruleid_by_pid_cnt++;
  bypid->pid = pid;
  bypid->ruleid = ruleid;
  bypid->fd = outpiperd;
  bypid->ignore = ignore;
  hashval = abce_murmur32(HASH_SEED, (uint32_t)pid);
  hashloc = hashval % (sizeof(ruleid_by_pid)/sizeof(*ruleid_by_pid));
  if (abce_rb_tree_nocmp_insert_nonexist(&ruleid_by_pid[hashloc], ruleid_by_pid_cmp_sym, NULL, &bypid->node) != 0)
  {
    printf("12\n");
    my_abort();
  }
  if (bypid->fd >= 0)
  {
    hashvalfd = abce_murmur32(HASH_SEED, (uint32_t)bypid->fd);
    hashlocfd = hashvalfd % (sizeof(ruleid_by_pid_fd)/sizeof(*ruleid_by_pid_fd));
    if (abce_rb_tree_nocmp_insert_nonexist(&ruleid_by_pid_fd[hashlocfd], ruleid_by_pid_fd_cmp_sym, NULL, &bypid->fdnode) != 0)
    {
      printf("12.5\n");
      my_abort();
    }
  }
  linked_list_add_tail(&bypid->llnode, &ruleid_by_pid_list);
}
void kill_all_children(int signum)
{
  struct linked_list_node *node;
  LINKED_LIST_FOR_EACH(node, &ruleid_by_pid_list)
  {
    struct ruleid_by_pid *bypid =
      ABCE_CONTAINER_OF(node, struct ruleid_by_pid, llnode);
    kill(bypid->pid, signum);
  }
}
