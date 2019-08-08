#include <stdlib.h>
#include <string.h>
#include "canon.h"

char *canon(const char *old)
{
  char *neu = malloc(strlen(old) + 1);
  char *neu2;
  size_t idx = 0;
  const char *old2;
  int is_abspath = 0;
  neu[0] = '\0';
  if (*old == '/')
  {
    neu[0] = '/';
    neu[1] = '\0';
    neu = neu + 1;
    is_abspath = 1;
  }
  while (*old)
  {
    old2 = strchr(old, '/');
    if (old2 == NULL)
    {
      old2 = old + strlen(old);
    }
    if (old2 == old)
    {
      old = (*old2 == '\0') ? old2 : (old2 + 1);
      continue;
    }
    if (old2 == old + 1 && old[0] == '.')
    {
      old = (*old2 == '\0') ? old2 : (old2 + 1);
      continue;
    }
    if (old2 == old + 2 && old[0] == '.' && old[1] == '.')
    {
      neu2 = strrchr(neu, '/');
      if (neu2 == NULL)
      {
        if (idx != 0 && (idx != 2 || neu[idx-1] != '.' || neu[idx-2] != '.'))
        {
          idx = 0;
          old = (*old2 == '\0') ? old2 : (old2 + 1);
          continue;
        }
        if (idx == 0)
        {
          if (is_abspath)
          {
            old = (*old2 == '\0') ? old2 : (old2 + 1);
            continue;
          }
          neu[idx+0] = '.';
          neu[idx+1] = '.';
          neu[idx+2] = '\0';
          idx += 2;
          old = (*old2 == '\0') ? old2 : (old2 + 1);
          continue;
        }
        neu[idx] = '/';
        neu[idx+1] = '.';
        neu[idx+2] = '.';
        neu[idx+3] = '\0';
        idx += 3;
        old = (*old2 == '\0') ? old2 : (old2 + 1);
        continue;
      }
      if ((neu + idx) - neu2 == 2 && neu[idx-1] == '.' && neu[idx-2] == '.')
      {
        neu[idx] = '/';
        neu[idx+1] = '.';
        neu[idx+2] = '.';
        neu[idx+3] = '\0';
        idx += 3;
        old = (*old2 == '\0') ? old2 : (old2 + 1);
        continue;
      }
      idx = neu2 - neu;
      neu[idx] = '\0';
      old = (*old2 == '\0') ? old2 : (old2 + 1);
      continue;
    }
    if (idx != 0)
    {
      neu[idx++] = '/';
    }
    memcpy(neu+idx, old, old2 - old);
    idx += (old2 - old);
    neu[idx] = '\0';
    old = (*old2 == '\0') ? old2 : (old2 + 1);
    continue;
  }
  if (is_abspath)
  {
    return neu - 1;
  }
  return neu;
}
