#include "stringtab.h"
#include "mymalloc.h"

struct abce_rb_tree_nocmp st[STRINGTAB_SIZE];
struct sttable_entry *sttable = NULL;
/*
 * Linux kernel 22.9.2025, 26084 headers, 35778 c files, 6000 directories
 * This gives a bit more than 128*1024 strings (each .c has .o and .d),
 * so even 128*1024 would not be enough. However, on the other hand,
 * settings st_cap to 256*1024 would on non-overcommit 64-bit systems
 * allocate 4 megabytes of memory immediately.
 */
mysize_t st_cap = 64*1024;
mysize_t st_cnt;

void errxit(const char *fmt, ...);

void st_grow(void)
{
  mysize_t st_newcap = st_cap * 2;
  struct sttable_entry *sttable_new;
  if (st_cnt < st_cap)
  {
    return;
  }
  if (st_newcap < 1024)
  {
    st_newcap = 1024;
  }
  sttable_new = stir_do_mmap_madvise(st_newcap*sizeof(*sttable_new));
  if (sttable_new == NULL)
  {
    return;
  }
  memcpy(sttable_new, sttable, st_cnt*sizeof(*sttable_new));
  stir_do_munmap(sttable, st_cap*sizeof(*sttable));
  sttable = sttable_new;
  st_cap = st_newcap;
}

void st_compact(void)
{
  char *ptr2;
  int errno_save;
  size_t bytes_total, bytes_in_use;
  bytes_total = stir_topages(st_cap * sizeof(*sttable));
  bytes_in_use = stir_topages(st_cnt * sizeof(*sttable));
  ptr2 = (void*)sttable;
  ptr2 += bytes_in_use;
  errno_save = errno;
  stir_do_munmap(ptr2, bytes_total - bytes_in_use);
  errno = errno_save;
  // don't report errors
}

mysize_t stringtab_cnt = 0;

mysize_t stringtab_get(const char *symbol)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  size_t hashloc;
  struct string_plus_len stringlen = {.str = symbol, .len = strlen(symbol)};
  hashval = abce_murmur_buf(HASH_SEED, symbol, stringlen.len);
  hashloc = hashval % (sizeof(st)/sizeof(*st));
  n = ABCE_RB_TREE_NOCMP_FIND(&st[hashloc], stringtabentry_cmp_asym,
NULL, &stringlen);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct stringtabentry, node)->idx;
  }
  return (mysize_t)-1;
}

mysize_t stringtab_add(const char *symbol)
{
  struct abce_rb_tree_node *n;
  uint32_t hashval;
  mysize_t hashloc;
  struct string_plus_len stringlen = {.str = symbol, .len = strlen(symbol)};
  hashval = abce_murmur_buf(HASH_SEED, symbol, stringlen.len);
  hashloc = hashval % (sizeof(st)/sizeof(*st));
  n = ABCE_RB_TREE_NOCMP_FIND(&st[hashloc], stringtabentry_cmp_asym, NULL, &stringlen);
  if (n != NULL)
  {
    return ABCE_CONTAINER_OF(n, struct stringtabentry, node)->idx;
  }
  stringtab_cnt++;
  struct stringtabentry *stringtabentry = my_malloc(sizeof(struct stringtabentry));
  stringtabentry->string = my_strdup_len(symbol, stringlen.len);
  stringtabentry->len = stringlen.len;
  st_grow();
  if (st_cnt >= st_cap)
  {
    errxit("stringtab full");
    exit(2);
  }
  sttable[st_cnt].s = stringtabentry->string;
  sttable[st_cnt].is_remade = 0;
  stringtabentry->idx = st_cnt++;
  if (abce_rb_tree_nocmp_insert_nonexist(&st[hashloc], stringtabentry_cmp_sym, NULL, &stringtabentry->node) != 0)
  {
    printf("23\n");
    my_abort();
  }
  return stringtabentry->idx;
}

mysize_t symbol_add(struct stiryy *stiryy, const char *symbol, size_t symlen)
{
  if (strlen(symbol) != symlen)
  {
    printf("22\n");
    my_abort(); // RFE what to do?
  }
  return stringtab_add(symbol);
}
