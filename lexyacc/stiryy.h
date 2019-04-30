#ifndef _STIRYY_H_
#define _STIRYY_H_

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

struct escaped_string {
  size_t sz;
  char *str;
};

struct CSnippet {
  char *data;
  size_t len;
  size_t capacity;
};

static inline void csadd(struct CSnippet *cs, char ch)
{
  if (cs->len + 2 >= cs->capacity)
  {
    size_t new_capacity = cs->capacity * 2 + 2;
    cs->data = realloc(cs->data, new_capacity);
    cs->capacity = new_capacity;
  }
  cs->data[cs->len] = ch;
  cs->data[cs->len + 1] = '\0';
  cs->len++;
}

static inline void csaddstr(struct CSnippet *cs, char *str)
{
  size_t len = strlen(str);
  if (cs->len + len + 1 >= cs->capacity)
  {
    size_t new_capacity = cs->capacity * 2 + 2;
    if (new_capacity < cs->len + len + 1)
    {
      new_capacity = cs->len + len + 1;
    }
    cs->data = realloc(cs->data, new_capacity);
    cs->capacity = new_capacity;
  }
  memcpy(cs->data + cs->len, str, len);
  cs->len += len;
  cs->data[cs->len] = '\0';
}

struct stiryy {
  struct CSnippet cs;
  struct CSnippet hs;
  struct CSnippet si;
  struct CSnippet ii;
};

static inline void stiryy_free(struct stiryy *stiryy)
{
  free(stiryy->cs.data);
  stiryy->cs.data = NULL;
  free(stiryy->hs.data);
  stiryy->hs.data = NULL;
  free(stiryy->si.data);
  stiryy->si.data = NULL;
  free(stiryy->ii.data);
  stiryy->ii.data = NULL;
  memset(stiryy, 0, sizeof(*stiryy));
}

static inline void dump_string_len(FILE *f, const char *str, size_t len)
{
  size_t i;
  putc('"', f);
  for (i = 0; i < len; i++)
  {
    if (str[i] == '"' || str[i] == '\\')
    {
      fprintf(f, "\\");
      putc(str[i], f);
      continue;
    }
    if (str[i] == '\n')
    {
      fprintf(f, "\\n");
      continue;
    }
    if (str[i] == '\t')
    {
      fprintf(f, "\\t");
      continue;
    }
    if (str[i] == '\r')
    {
      fprintf(f, "\\r");
      continue;
    }
    if (isalpha(str[i]) || isdigit(str[i]) || ispunct(str[i]) || str[i] == ' ')
    {
      putc(str[i], f);
      continue;
    }
    fprintf(f, "\\x");
    fprintf(f, "%.2x", (unsigned char)str[i]);
  }
  putc('"', f);
}

static inline void dump_string(FILE *f, const char *str)
{
  size_t len = strlen(str);
  dump_string_len(f, str, len);
}

#endif
