#include <stdio.h>
#include "incyyutils.h"
#include "incyy.h"

static inline size_t myspn(const char *str)
{
  size_t len = 0;
  for (;;)
  {
    //printf("Iter: %s\n", str+len);
    len += strspn(str+len, "\t ");
    if (len && str[len] == '\\' && str[len+1] == '\n')
    {
      len += 2;
      continue;
    }
    //printf("Return: %s\n", str+len);
    return len;
  }
}
static inline size_t mycspn(const char *str, int *had_escapes)
{
  size_t len = 0;
  *had_escapes = 0;
  for (;;)
  {
    len += strcspn(&str[len], "\t \n:\\");
    if (str[len] == '\\' && str[len+1])
    {
      *had_escapes = 1;
      len += 2;
      continue;
    }
    return len;
  }
}

int handle_line(char *line, struct incyy *incyy)
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
  incyy_emplace_rule(incyy);
  while (!had_colon)
  {
    len = mycspn(line+start, &had_escapes);
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
      }
      incyy_set_tgt(incyy, &line[start]);
      //printf("TARGET: %s\n", &line[start]);
    }
    //start = start+len+1 + myspn(line+start+len+1);
    line[start+len] = ' ';
    start = start+len + myspn(line+start+len);
  }
  while (!had_end)
  {
    len = mycspn(line+start, &had_escapes);
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
      }
      incyy_set_dep(incyy, &line[start]);
      //printf("DEP: %s\n", &line[start]);
    }
    //start = start+len+1 + myspn(line+start+len+1);
    line[start+len] = ' ';
    start = start+len + myspn(line+start+len);
  }
  return 0;
}

int incyymineparse(FILE *f, struct incyy *incyy)
{
  char *lineptr = NULL;
  char *lineptr2 = NULL;
  size_t n = 0;
  size_t n2 = 0;
  ssize_t nread;
  ssize_t nread2;
  while ((nread = getline(&lineptr, &n, f)) >= 0)
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
        nread2 = getline(&lineptr2, &n2, f);
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
    if (handle_line(lineptr, incyy) != 0)
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
