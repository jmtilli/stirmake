#include <stdio.h>
#include "incyyutils.h"
#include "incyy.h"
#include "mygetline.h"

static inline size_t whitespacespn(const char *str)
{
  size_t len = 0;
  for (;;)
  {
    if (str[len] == '\0' || (str[len] != ' ' && str[len] != '\t')) return len;
    len++;
  }
}

static inline int inset_ascii(char ch)
{
  unsigned char uch = (unsigned char)ch;
  uint64_t set = 0x400000100000601ULL;
  if (uch == '\\') return 1;
  if (uch >= 64) return 0;
  return (set>>uch)&1;
}
static inline int inset_set(char ch, const uint64_t set[4])
{
  unsigned char uch = (unsigned char)ch;
  return (set[uch/64]>>(uch%64))&1;
}

static inline size_t setcspn_ascii(const char *str)
{
  size_t len = 0;
  for (;;)
  {
    if (inset_ascii(str[len])) return len;
    if (inset_ascii(str[len+1])) return len+1;
    if (inset_ascii(str[len+2])) return len+2;
    if (inset_ascii(str[len+3])) return len+3;
    if (inset_ascii(str[len+4])) return len+4;
    if (inset_ascii(str[len+5])) return len+5;
    if (inset_ascii(str[len+6])) return len+6;
    if (inset_ascii(str[len+7])) return len+7;
    len += 8;
  }
}
static inline size_t setcspn_set(const char *str, const uint64_t set[4])
{
  size_t len = 0;
  for (;;)
  {
    if (inset_set(str[len], set)) return len;
    if (inset_set(str[len+1], set)) return len+1;
    if (inset_set(str[len+2], set)) return len+2;
    if (inset_set(str[len+3], set)) return len+3;
    if (inset_set(str[len+4], set)) return len+4;
    if (inset_set(str[len+5], set)) return len+5;
    if (inset_set(str[len+6], set)) return len+6;
    if (inset_set(str[len+7], set)) return len+7;
    len += 8;
  }
}

static inline size_t myspn(const char *str)
{
  size_t len = 0;
  for (;;)
  {
    //printf("Iter: %s\n", str+len);
    len += whitespacespn(str+len);
    if (len && str[len] == '\\' && str[len+1] == '\n')
    {
      len += 2;
      continue;
    }
    //printf("Return: %s\n", str+len);
    return len;
  }
}
static inline size_t mycspn_ascii(const char *str, int *had_escapes)
{
  size_t len = 0;
  *had_escapes = 0;
  for (;;)
  {
    len += setcspn_ascii(&str[len]);
    if (str[len] == '\\' && str[len+1])
    {
      *had_escapes = 1;
      len += 2;
      continue;
    }
    return len;
  }
}
static inline size_t mycspn_set(const char *str, int *had_escapes,
                                const uint64_t set[4])
{
  size_t len = 0;
  *had_escapes = 0;
  for (;;)
  {
    len += setcspn_set(&str[len], set);
    if (str[len] == '\\' && str[len+1])
    {
      *had_escapes = 1;
      len += 2;
      continue;
    }
    return len;
  }
}

static inline int is_ascii(void)
{
  return ('\t' == 9) && (' ' == 32) && ('\n' == 10) && (':' == 58);
}

int handle_line(char *line, struct incyy *incyy, const uint64_t set[4], incyy_fn_t fnrule, incyy_fn_t fntarget, incyy_fn_t fndep, void *userdata)
{
  size_t start;
  size_t len;
  int had_colon = 0;
  int had_end = 0;
  int had_escapes;
  //printf("LINE: %s\n", line);
  start = myspn(line);
  if (line[start] == '\0' || line[start] == '\n')
  {
    return 0;
  }
  incyy_emplace_rule(incyy, fnrule, fntarget, userdata);
  while (!had_colon)
  {
    if (is_ascii())
    {
      len = mycspn_ascii(line+start, &had_escapes);
    }
    else
    {
      len = mycspn_set(line+start, &had_escapes, set);
    }
    if (line[start+len] == ':')
    {
      had_colon = 1;
    }
    if (line[start+len] == '\0' || line[start+len] == '\n')
    {
      return -1;
    }
    line[start+len] = '\0';
    if (len != 0)
    {
      size_t len2 = len;
      if (had_escapes)
      {
        size_t idx;
        char *cp = &line[start];
        for (idx = 0; idx < len; idx++)
        {
          if (line[start+idx] == '\\')
          {
            if (idx+1 >= len)
            {
              return -1;
            }
            *cp++ = line[start + (++idx)];
          }
          else
          {
            *cp++ = line[start + idx];
          }
        }
        *cp = '\0';
        len2 = cp-(line+start);
      }
      incyy_set_tgt(incyy, &line[start], len2, fntarget, userdata);
      //printf("TARGET: %s\n", &line[start]);
    }
    //start = start+len+1 + myspn(line+start+len+1);
    line[start+len] = ' ';
    start = start+len + myspn(line+start+len);
  }
  while (!had_end)
  {
    if (is_ascii())
    {
      len = mycspn_ascii(line+start, &had_escapes);
    }
    else
    {
      len = mycspn_set(line+start, &had_escapes, set);
    }
    if (line[start+len] == ':')
    {
      return -1;
    }
    if (line[start+len] == '\n' || line[start+len] == '\0')
    {
      had_end = 1;
    }
    line[start+len] = '\0';
    if (len != 0)
    {
      size_t len2 = len;
      if (had_escapes)
      {
        size_t idx;
        char *cp = &line[start];
        for (idx = 0; idx < len; idx++)
        {
          if (line[start+idx] == '\\')
          {
            if (idx+1 >= len)
            {
              return -1;
            }
            *cp++ = line[start + (++idx)];
          }
          else
          {
            *cp++ = line[start + idx];
          }
        }
        *cp = '\0';
        len2 = cp-(line+start);
      }
      incyy_set_dep(incyy, &line[start], len2, fndep, userdata);
      //printf("DEP: %s\n", &line[start]);
    }
    //start = start+len+1 + myspn(line+start+len+1);
    line[start+len] = ' ';
    start = start+len + myspn(line+start+len);
  }
  return 0;
}

int incyymineparse(FILE *f, struct incyy *incyy, incyy_fn_t fnrule, incyy_fn_t fntarget, incyy_fn_t fndep, void *userdata)
{
  char *lineptr = NULL;
  char *lineptr2 = NULL;
  size_t n = 0;
  size_t n2 = 0;
  ssize_t nread;
  ssize_t nread2;
  uint64_t set[4] = {0};
  set['\\'/64] |= 1ULL<<('\\'%64);
  set['\t'/64] |= 1ULL<<('\t'%64);
  set['\n'/64] |= 1ULL<<('\n'%64);
  set[' '/64] |= 1ULL<<(' '%64);
  set[':'/64] |= 1ULL<<(':'%64);
  set[0] |= 1;
  while ((nread = mygetline(&lineptr, &n, f)) >= 0)
  {
#if 0
    if (nread && lineptr[nread-1] == '\n')
    {
      nread -= 1;
    }
#endif
    if (nread>=2 && lineptr[nread-1] == '\n' && lineptr[nread-2] == '\\')
    {
      size_t i;
      int escape = 0;
      for (i = 0; i < (size_t)nread-1; i++)
      {
        if (!escape && lineptr[i] == '\\')
        {
          escape = 1;
          continue;
        }
        if (escape)
        {
          escape = 0;
        }
      }
      while (escape)
      {
        escape = 0;
        nread2 = mygetline(&lineptr2, &n2, f);
	//printf("READ LINEPTR2: %s\n", lineptr2);
        if (nread2 < 0)
        {
          free(lineptr);
          free(lineptr2);
          return -1;
        }
        if ((size_t)nread+(size_t)nread2+1 >= n)
        {
          size_t newcap = (size_t)nread + (size_t)nread2 + 1;
          char *lineptrtmp;
          if (newcap < 2*n)
          {
            newcap = 2*n;
          }
          lineptrtmp = realloc(lineptr, newcap);
          if (lineptrtmp == NULL)
          {
            free(lineptr);
            free(lineptr2);
            return -1;
          }
          lineptr = lineptrtmp;
          n = newcap;
        }
        memcpy(&lineptr[nread], lineptr2, (size_t)nread2+1);
	nread += nread2;
#if 0
        if (nread2 && lineptr2[nread2-1] == '\n')
        {
          nread2 -= 1;
        }
#endif
        if (nread2 >= 2 && lineptr2[nread2-1] == '\n' && lineptr2[nread2-2] == '\\')
        {
	  //printf("CHECKING FOR LINEPTR2\n");
          for (i = 0; i < (size_t)nread2-1; i++)
          {
            if (!escape && lineptr2[i] == '\\')
            {
              escape = 1;
              continue;
            }
            if (escape)
            {
              escape = 0;
            }
          }
        }
      }
    }
    if (handle_line(lineptr, incyy, set, fnrule, fntarget, fndep, userdata) != 0)
    {
      free(lineptr);
      free(lineptr2);
      return -1;
    }
  }
  free(lineptr);
  free(lineptr2);
  return 0;
}
