#ifndef _STRINGTAB_H_
#define _STRINGTAB_H_

#include "linkedlist.h"
#include "abce/abcemurmur.h"
#include "abce/abcecontainerof.h"
#include "abce/abcerbtree.h"
#include "stiryy.h"
#include "const.h"

struct stringtabentry {
  struct abce_rb_tree_node node;
  char *string;
  mysize_t len;
  mysize_t idx;
};

struct string_plus_len {
  const char *str;
  mysize_t len;
};

static inline int stringtabentry_cmp_asym(const void *stringlenv, struct abce_rb_tree_node *n2, void *ud)
{
  const struct string_plus_len *stringlen = stringlenv;
  struct stringtabentry *e = ABCE_CONTAINER_OF(n2, struct stringtabentry, node);
  int ret;
  const char *str2, *str1;
  size_t len2, len1;
  size_t minlen;
  str1 = stringlen->str;
  len1 = stringlen->len;
  str2 = e->string;
  len2 = e->len;
  minlen = (len1 < len2) ? len1 : len2;
  ret = memcmp(str1, str2, minlen);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 < len2)
  {
    return -1;
  }
  if (len1 > len2)
  {
    return 1;
  }
  return 0;
}

static inline int stringtabentry_cmp_sym(struct abce_rb_tree_node *n1, struct abce_rb_tree_node *n2, void *ud)
{
  struct stringtabentry *e1 = ABCE_CONTAINER_OF(n1, struct stringtabentry, node);
  struct stringtabentry *e2 = ABCE_CONTAINER_OF(n2, struct stringtabentry, node);
  int ret;
  size_t len1 = e1->len;
  size_t len2 = e2->len;
  size_t minlen = (len1 < len2) ? len1 : len2;
  ret = memcmp(e1->string, e2->string, minlen);
  if (ret != 0)
  {
    return ret;
  }
  if (len1 < len2)
  {
    return -1;
  }
  if (len1 > len2)
  {
    return 1;
  }
  return 0;
}

struct sttable_entry {
  char *s;
  unsigned is_remade:1;
  unsigned is_cdepwatch:1;
};

extern struct abce_rb_tree_nocmp st[STRINGTAB_SIZE];
extern struct sttable_entry *sttable;
extern mysize_t st_cap;
extern mysize_t st_cnt;
extern mysize_t stringtab_cnt;


void st_grow(void);
void st_compact(void);
mysize_t stringtab_get(const char *symbol);
mysize_t stringtab_add(const char *symbol);
mysize_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen);

#endif
