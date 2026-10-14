#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include "linkedlist.h"
#include "syncbuf.h"
#include "yyutils.h"
#include "abce/abce.h"

mysize_t max_syncbufs_cached = 0;
mysize_t cur_syncbufs_cached = 0;
int was_recursive = 0;
int was_recursive_outsync = 0;
int output_is_pipe = 0;

struct syncbufpage *cache_head = NULL;

void *stir_do_mmap_madvise(size_t bytes);

void stir_do_munmap(void *ptr, size_t bytes);

struct syncbufpage *getbuf(void)
{
  struct syncbufpage *page;
  if (cache_head && cur_syncbufs_cached)
  {
    struct syncbufpage *new_cache_head;
    memcpy(&new_cache_head, cache_head, sizeof(new_cache_head));
    page = cache_head;
    page->meatsz = 0;
    cache_head = new_cache_head;
    cur_syncbufs_cached--;
    return page;
  }
  page = stir_do_mmap_madvise(sizeof(*page));
  page->meatsz = 0;
  return page;
}

void putbuf(struct syncbufpage *page)
{
  if (cur_syncbufs_cached < max_syncbufs_cached)
  {
    memcpy(page, &cache_head, sizeof(cache_head));
    cache_head = page;
    cur_syncbufs_cached++;
    return;
  }
  stir_do_munmap(page, sizeof(*page));
}

void syncbuf_append(struct syncbuf *buf, const void *data, size_t sz)
{
  struct syncbufpage *page;
  const char *cdata = data;
  size_t tocopy;
  if (sz == 0)
  {
    return;
  }
  if (is_ascii_full() && !buf->has_escapes && memchr(data, 27, sz))
  {
    buf->has_escapes = 1;
  }
  if (linked_list_is_empty(&buf->list))
  {
    linked_list_add_tail(&getbuf()->llnode, &buf->list);
  }
  while (sz > 0)
  {
    page = ABCE_CONTAINER_OF(buf->list.node.prev, struct syncbufpage, llnode);
    if (page->meatsz == sizeof(page->meat))
    {
      linked_list_add_tail(&getbuf()->llnode, &buf->list);
      page = ABCE_CONTAINER_OF(buf->list.node.prev, struct syncbufpage, llnode);
    }
    tocopy = sizeof(page->meat) - page->meatsz;
    if (sz < tocopy)
    {
      tocopy = sz;
    }
    memcpy(&page->meat[page->meatsz], cdata, tocopy);
    page->meatsz += tocopy;
    cdata += tocopy;
    sz -= tocopy;
  }
}

size_t writeall(int fd, const void *buf, size_t sz)
{
  const char *cdata = buf;
  size_t total_written = 0;
  ssize_t bytes_written;
  while (sz > 0)
  {
    bytes_written = write(fd, cdata, sz);
    if (bytes_written < 0 && errno == EINTR)
    {
      continue;
    }
    if (bytes_written < 0)
    {
      return total_written;
    }
    total_written += (size_t)bytes_written;
    cdata += bytes_written;
    sz -= (size_t)bytes_written;
  }
  return total_written;
}

void syncbuf_dump(struct syncbuf *buf, int fd)
{
  struct syncbufpage *page;
  // If isatty(fd), we don't strip, since a terminal supports ANSI color.
  // If was_recursive && was_recursive_outsync && isapipe(fd) we don't strip
  // ...since then the parent stirmake strips.
  // However, output_is_pipe is defined only for fd == 1, which the caller
  // always fortunately happens to use, so fd != 1, we have to assume that
  // !output_is_pipe and to strip.
  if (is_ascii_full() && buf->has_escapes && !isatty(fd) && (!was_recursive || !was_recursive_outsync || (fd != 1 || !output_is_pipe)))
  {
    int escapemode = 0;
    int toskip = 0;
    struct linked_list_node *node;
    LINKED_LIST_FOR_EACH(node, &buf->list)
    {
      size_t off;
      size_t i;
      size_t j = 0;
      size_t start = 0;
      char *ptr;
      page = ABCE_CONTAINER_OF(node, struct syncbufpage, llnode);
      if (toskip)
      {
        start = 1;
        toskip = 0;
      }
      for (i = start; i < page->meatsz; i++)
      {
        if (page->meat[i] == 27)
        {
          if (i+1 == page->meatsz)
          {
            struct syncbufpage *nxtpage = NULL;
            if (node->next != &buf->list.node)
            {
              nxtpage = ABCE_CONTAINER_OF(node->next, struct syncbufpage, llnode);
            }
            if (nxtpage && nxtpage->meatsz && nxtpage->meat[0] == '[')
            {
              toskip = 1;
              escapemode = 1;
              continue;
            }
          }
          else if (page->meat[i+1] == '[')
          {
            i++;
            escapemode = 1;
            continue;
          }
        }
        if (escapemode)
        {
          if (page->meat[i] >= '\x40' && page->meat[i] <= '\x7E')
          {
            escapemode = 0;
          }
          continue;
        }
        page->meat[j++] = page->meat[i];
      }
      page->meatsz = j;
    }
  }
  while (!linked_list_is_empty(&buf->list))
  {
    page = ABCE_CONTAINER_OF(buf->list.node.next, struct syncbufpage, llnode);
    writeall(fd, page->meat, page->meatsz);
    linked_list_delete(&page->llnode);
    putbuf(page);
  }
}
