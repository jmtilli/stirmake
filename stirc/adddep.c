#include "adddep.h"
#include "stircommon.h"
#include "mymalloc.h"

struct add_dep_entry_block *add_dep_entry_block_first;
struct add_dep_entry_block *add_dep_entry_block_last;
mysize_t add_dep_entry_block_cnt;

void ins_add_dep(mysize_t tgtidx, mysize_t depidx, mysize_t depidxnodir,
                 int auto_phony, int tgt_phony, int is_inc)
{
  struct add_dep_entry_block *blk;
  if (add_dep_entry_block_first == NULL)
  {
    add_dep_entry_block_first = my_malloc(sizeof(*add_dep_entry_block_first));
    add_dep_entry_block_cnt++;
    add_dep_entry_block_first->cnt = 0;
    add_dep_entry_block_first->next = NULL;
    add_dep_entry_block_last = add_dep_entry_block_first;
  }
  blk = add_dep_entry_block_last;
  while (blk->cnt >= sizeof(blk->e)/sizeof(*blk->e))
  {
    if (blk->next == NULL)
    {
      blk->next = my_malloc(sizeof(*blk->next));
      add_dep_entry_block_cnt++;
      blk->next->cnt = 0;
      blk->next->next = NULL;
      add_dep_entry_block_last = blk;
    }
    //printf("OK\n");
    blk = blk->next;
  }
  blk->e[blk->cnt].tgtidx = tgtidx;
  blk->e[blk->cnt].depidx = depidx;
  //blk->e[blk->cnt].depidxnodir = depidxnodir;
  blk->e[blk->cnt].auto_phony = !!auto_phony;
  blk->e[blk->cnt].tgt_phony = !!tgt_phony;
  blk->e[blk->cnt].is_inc = !!is_inc;
  blk->cnt++;
}
