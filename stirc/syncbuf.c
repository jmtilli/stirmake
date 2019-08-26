#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "linkedlist.h"
#include "syncbuf.h"
#include "abce/abce.h"

void *stir_do_mmap_madvise(size_t bytes);

void stir_do_munmap(void *ptr, size_t bytes);

struct syncbufpage *getbuf(void)
{
  struct syncbufpage *page;
  page = stir_do_mmap_madvise(sizeof(page));
  page->meatsz = 0;
  return page;
}

void putbuf(struct syncbufpage *page)
{
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
    total_written += bytes_written;
    cdata += bytes_written;
    sz -= bytes_written;
  }
  return total_written;
}

void syncbuf_dump(struct syncbuf *buf, int fd)
{
  struct syncbufpage *page;
  while (!linked_list_is_empty(&buf->list))
  {
    page = ABCE_CONTAINER_OF(buf->list.node.next, struct syncbufpage, llnode);
    writeall(fd, page->meat, page->meatsz);
    linked_list_delete(&page->llnode);
    putbuf(page);
  }
}
