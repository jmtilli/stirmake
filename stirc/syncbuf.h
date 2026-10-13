#ifndef _SYNCBUF_H_
#define _SYNCBUF_H_ 1

#include <stdint.h>
#include "linkedlist.h"
#include "stirutils.h"

// RFE make variable-sized to use more efficiently 8K or 16K pages
struct syncbufpage {
  struct linked_list_node llnode;
  uint16_t meatsz;
  char meat[4078];
};

struct syncbuf {
  struct linked_list_head list;
  unsigned has_escapes:1;
};

static inline void syncbuf_init(struct syncbuf *buf)
{
  linked_list_head_init(&buf->list);
  buf->has_escapes = 0;
}

void syncbuf_append(struct syncbuf *buf, const void *data, size_t sz);

void syncbuf_dump(struct syncbuf *buf, int fd);

size_t writeall(int fd, const void *buf, size_t sz);

extern mysize_t max_syncbufs_cached;
extern mysize_t cur_syncbufs_cached;

#endif
