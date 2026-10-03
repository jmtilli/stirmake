#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "canon.h"
#include "stirutils.h"

void my_abort(void);

char *pathcat1_buf(const char *old, char *buf, size_t bufsz)
{
  size_t strlen_old = strlen(old);
  char *neu = NULL;
  if (buf == NULL)
  {
    neu = malloc(strlen_old + 1);
  }
  else
  {
    neu = buf;
    if (bufsz < strlen_old + 1)
    {
      neu = malloc(strlen_old + 1);
    }
  }
  snprintf(neu, strlen_old + 1, "%s", old);
  return neu;
}
char *pathcat2_buf(const char *old, const char *old2, char *buf, size_t bufsz)
{
  size_t strlen_old = strlen(old);
  size_t strlen_old2 = strlen(old2);
  size_t bufneed = strlen_old + strlen_old2 + 2;
  char *neu = NULL;
  if (buf == NULL)
  {
    neu = malloc(bufneed);
  }
  else
  {
    neu = buf;
    if (bufsz < bufneed)
    {
      neu = malloc(bufneed);
    }
  }
  snprintf(neu, bufneed, "%s/%s", old, old2);
  return neu;
}

char *canon_buf(const char *old, char *buf, size_t bufsz)
{
  char *neu = NULL;
  char *neu2;
  size_t idx = 0;
  const char *old2;
  size_t strlen_old = strlen(old);
  int is_abspath = 0;
  if (buf == NULL)
  {
    neu = malloc(strlen_old + 1);
  }
  else
  {
    neu = buf;
    if (bufsz < strlen_old + 1)
    {
      neu = malloc(strlen_old + 1);
    }
  }
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
      idx = (size_t)(neu2 - neu);
      neu[idx] = '\0';
      old = (*old2 == '\0') ? old2 : (old2 + 1);
      continue;
    }
    if (idx != 0)
    {
      neu[idx++] = '/';
    }
    memcpy(neu+idx, old, (size_t)(old2 - old));
    idx += (size_t)(old2 - old);
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
char *canon(const char *old)
{
  return canon_buf(old, NULL, 0);
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
  const char *pathslash, *fileslash;
  size_t pathlen;
  if (file[0] == '/' || strcmp(path, ".") == 0)
  {
    return stir_strdup(file);
  }
  pathlen = strlen(path);
  if (strncmp(path, file, pathlen) == 0 && file[pathlen] == '/')
  {
    return stir_strdup(file+pathlen+1);
  }
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
    if (pathslash == NULL)
    {
      pathslash = path + strlen(path);
    }
    if (fileslash == NULL)
    {
      fileslash = file + strlen(file);
    }
    if (   pathslash - path != fileslash - file
        || pathslash - path == 0
        || memcmp(path, file, (size_t)(pathslash - path)) != 0)
    {
      char *bp = construct_backpath((*path) ? path : ".");
      size_t bufsiz;
      char *buf, *result;
      if (bp == NULL)
      {
        free(pathcanon);
        free(filecanon);
        return NULL;
      }
      if (*file == '\0')
      {
        file = ".";
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
      if (snprintf(buf, bufsiz, "%s/%s", bp, file) >= (int)bufsiz)
      {
        abort();
      }
      free(bp);
      free(pathcanon);
      free(filecanon);
      result = canon(buf);
      free(buf);
      return result;
    }
    file = (*fileslash) ? (fileslash + 1) : fileslash;
    path = (*pathslash) ? (pathslash + 1) : pathslash;
  }
}
