#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "canon.h"

void my_abort(void);

char *canon(const char *old)
{
  char *neu = malloc(strlen(old) + 1);
  char *neu2;
  size_t idx = 0;
  const char *old2;
  int is_abspath = 0;
  if (old[0] == '\0')
  {
    my_abort(); // Must give some path
  }
  if (old[0] == '.' && old[1] == '\0')
  {
    neu[0] = '.';
    neu[1] = '\0';
    return neu;
  }
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
          neu[idx] = '\0';
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
  if (idx == 0)
  {
    neu[0] = '.';
    neu[1] = '\0';
  }
  return neu;
}

size_t strcnt(const char *haystack, char needle)
{
  size_t ret = 0;
  while (*haystack)
  {
    ret += ((*haystack) == needle);
    haystack++;
  }
  return ret;
}

/*
 * Given a relative path of form a/b/c, constructs the path ../../.. where
 * the count of .. elements is the same as the count of path elements. ".."
 * elements in input are handled correctly, except the path may not start
 * with .. after canonicalization. "." elements in niput are handled correctly,
 * too.
 */
char *construct_backpath(const char *frontpath)
{
  char *can;
  size_t cnt;
  size_t sz;
  char *ret, *ptr;
  can = canon(frontpath);
  if (can == NULL)
  {
    return NULL;
  }
  if (can[0] == '.' && can[1] == '\0')
  {
    return can;
  }
  if (can[0] == '/')
  {
    my_abort(); // we don't support this use case
  }
  if (can[0] == '.' && can[1] == '.' && (can[2] == '\0' || can[2] == '/'))
  {
    my_abort(); // we don't support this use case
  }
  cnt = strcnt(can, '/') + 1;
  free(can);
  sz = 3*cnt + 1;
  ret = malloc(sz);
  if (ret == NULL)
  {
    return NULL;
  }
  ptr = ret;
  while (cnt > 1)
  {
    *ptr++ = '.';
    *ptr++ = '.';
    *ptr++ = '/';
    cnt--;
  }
  if (cnt == 1)
  {
    *ptr++ = '.';
    *ptr++ = '.';
  }
  *ptr++ = '\0';
  return ret;
}

char *neighpath(const char *path, const char *file)
{
  char *pathcanon, *filecanon;
  char *pathslash, *fileslash;
  filecanon = canon(file);
  if (filecanon == NULL)
  {
    return NULL;
  }
  pathcanon = canon(path);
  if (pathcanon == NULL)
  {
    free(filecanon);
    return NULL;
  }
  file = filecanon;
  path = pathcanon;
  for (;;)
  {
    pathslash = strchr(path, '/');
    fileslash = strchr(file, '/');
    if (   pathslash == NULL || fileslash == NULL
        || pathslash - path != fileslash - file)
    {
      char *bp = construct_backpath(path);
      size_t bufsiz;
      char *buf;
      if (bp == NULL)
      {
        free(pathcanon);
        free(filecanon);
        return NULL;
      }
      bufsiz = strlen(bp)+strlen(file)+2;
      buf = malloc(bufsiz);
      if (buf == NULL)
      {
        free(bp);
        free(pathcanon);
        free(filecanon);
        return NULL;
      }
      if (snprintf(buf, bufsiz, "%s/%s", bp, file) >= bufsiz)
      {
        abort();
      }
      free(bp);
      free(pathcanon);
      free(filecanon);
      return buf;
    }
    file = fileslash + 1;
    path = pathslash + 1;
  }
}
