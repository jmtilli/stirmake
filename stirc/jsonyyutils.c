#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <libgen.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "jsonyy.h"
#include "jsonyyutils.h"

typedef void *yyscan_t;
extern int jsonyyparse(yyscan_t scanner, struct jsonyy *jsonyy);
extern int jsonyylex_init(yyscan_t *scanner);
extern void jsonyyset_in(FILE *in_str, yyscan_t yyscanner);
extern int jsonyylex_destroy(yyscan_t yyscanner);

int jsonyydoparse(FILE *filein, struct jsonyy *jsonyy)
{
  yyscan_t scanner;
  int fd;
  struct stat statbuf;

  fd = fileno(filein);
  if (fstat(fd, &statbuf) != 0)
  {
    return -EBADF;
  }
  if (!S_ISREG(statbuf.st_mode))
  {
    return -EBADF;
  }

  jsonyylex_init(&scanner);
  jsonyyset_in(filein, scanner);
  if (jsonyyparse(scanner, jsonyy) != 0)
  {
    return -EBADMSG;
  }
  jsonyylex_destroy(scanner);
  if (!feof(filein))
  {
    return -EBADMSG;
  }
  return 0;
}

int jsonyydomemparse(char *filedata, size_t filesize, struct jsonyy *jsonyy)
{
  FILE *myfile;
  int ret;
  myfile = fmemopen(filedata, filesize, "r");
  if (myfile == NULL)
  {
    return -ENOMEM;
  }
  ret = jsonyydoparse(myfile, jsonyy);
  if (fclose(myfile) != 0)
  {
    return -EBADF;
  }
  return ret;
}

static void *memdup(const void *mem, size_t sz)
{
  void *result;
  result = malloc(sz);
  if (result == NULL)
  {
    return result;
  }
  memcpy(result, mem, sz);
  return result;
}

struct json_escaped_string jsonyy_escape_string(char *orig)
{
  char *buf = NULL;
  char *result = NULL;
  struct json_escaped_string resultstruct;
  size_t j = 0;
  size_t capacity = 0;
  size_t i = 1;
  while (orig[i] != '"')
  {
    if (j+3 >= capacity)
    {
      char *buf2;
      capacity = 2*capacity+10;
      buf2 = realloc(buf, capacity);
      if (buf2 == NULL)
      {
        free(buf);
        resultstruct.str = NULL;
        return resultstruct;
      }
      buf = buf2;
    }
    if (j+3 >= capacity)
    {
      abort();
    }
    if (orig[i] != '\\')
    {
      buf[j++] = orig[i++];
    }
    else if (orig[i+1] == 'u')
    {
      char hexbuf[5] = {0};
      long l;
      hexbuf[0] = orig[i+2];
      hexbuf[1] = orig[i+3];
      hexbuf[2] = orig[i+4];
      hexbuf[3] = orig[i+5];
      l = strtol(hexbuf, NULL, 16);
      if (l < 0 || l >= 65536) // UTF-8
      {
        abort();
      }
      if (l >= 2048)
      {
        buf[j++] = (l>>12) | 0xE0;
        buf[j++] = ((l>>6)&0x3F) | 0x80;
        buf[j++] = ((l>>0)&0x3F) | 0x80;
      }
      else if (l >= 128)
      {
        buf[j++] = (l>>6) | 0xC0;
        buf[j++] = ((l>>0)&0x3F) | 0x80;
      }
      else
      {
        buf[j++] = l;
      }
      i += 6;
    }
    else if (orig[i+1] == 't')
    {
      buf[j++] = '\t';
      i += 2;
    }
    else if (orig[i+1] == 'r')
    {
      buf[j++] = '\r';
      i += 2;
    }
    else if (orig[i+1] == 'n')
    {
      buf[j++] = '\n';
      i += 2;
    }
    else if (orig[i+1] == 'f')
    {
      buf[j++] = '\f';
      i += 2;
    }
    else if (orig[i+1] == 'b')
    {
      buf[j++] = '\b';
      i += 2;
    }
    else if (orig[i+1] == '/')
    {
      buf[j++] = orig[i+1];
      i += 2;
    }
    else if (orig[i+1] == '\\')
    {
      buf[j++] = orig[i+1];
      i += 2;
    }
    else if (orig[i+1] == '"')
    {
      buf[j++] = orig[i+1];
      i += 2;
    }
    else
    {
      abort();
    }
  }
  if (j >= capacity)
  {
    char *buf2;
    capacity = 2*capacity+10;
    buf2 = realloc(buf, capacity);
    if (buf2 == NULL)
    {
      free(buf);
      resultstruct.str = NULL;
      return resultstruct;
    }
    buf = buf2;
  }
  resultstruct.sz = j;
  buf[j++] = '\0';
  result = memdup(buf, j);
  resultstruct.str = result;
  free(buf);
  return resultstruct;
}

int jsonyynameparse(const char *fname, struct jsonyy *jsonyy)
{
  FILE *jsonyyfile;
  int ret;
  int fd;
  struct stat statbuf;
  if (stat(fname, &statbuf) != 0)
  {
    return -ENOENT;
  }
  if (!S_ISREG(statbuf.st_mode))
  {
    return -EBADF;
  }
  jsonyyfile = fopen(fname, "r");
  fd = fileno(jsonyyfile);
  if (fstat(fd, &statbuf) != 0)
  {
    fclose(jsonyyfile);
    return -EBADF;
  }
  if (!S_ISREG(statbuf.st_mode))
  {
    fclose(jsonyyfile);
    return -EBADF;
  }
  if (jsonyyfile == NULL)
  {
#if 0
    if (require)
    {
      fprintf(stderr, "File %s cannot be opened\n", fname);
      exit(1);
    }
    if (jsonyy_postprocess(jsonyy) != 0)
    {
      exit(1);
    }
#endif
    return -ENOENT;
  }
  ret = jsonyydoparse(jsonyyfile, jsonyy);
#if 0
  if (jsonyy_postprocess(jsonyy) != 0)
  {
    exit(1);
  }
#endif
  fclose(jsonyyfile);
  return ret;
}

int jsonyydirparse(
  const char *argv0, const char *fname, struct jsonyy *jsonyy)
{
  const char *dir;
  char *copy = strdup(argv0);
  char pathbuf[PATH_MAX];
  dir = dirname(copy); // NB: not for multi-threaded operation!
  snprintf(pathbuf, sizeof(pathbuf), "%s/%s", dir, fname);
  free(copy);
  return jsonyynameparse(pathbuf, jsonyy);
}
