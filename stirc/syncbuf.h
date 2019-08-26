#ifndef _SYNCBUF_H_
#define _SYNCBUF_H_ 1

#include <stdint.h>
#include "linkedlist.h"

// RFE make variable-sized to use more efficiently 8K or 16K pages
struct syncbufpage {
  struct linked_list_node llnode;
  uint16_t meatsz;
  char meat[4078];
};

struct syncbuf {
  struct linked_list_head list;
};

static inline void syncbuf_init(struct syncbuf *buf)
{
  linked_list_head_init(&buf->list);
}

void syncbuf_append(struct syncbuf *buf, const void *data, size_t sz);

void syncbuf_dump(struct syncbuf *buf, int fd);

#endif
