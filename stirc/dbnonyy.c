#include <stdio.h>
#include "dbyyutils.h"
#include "dbyy.h"
#include "yyutils.h"
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

static struct escaped_string dbyy_escape_string(char *orig, char **strendptr)
{
  char *buf = NULL;
  char *result = NULL;
  struct escaped_string resultstruct;
  size_t j = 0;
  //size_t capacity = 0;
  size_t i = 1;
  if (strendptr)
  {
    *strendptr = NULL;
  }
  buf = orig;
  while (orig[i] != '"' && orig[i])
  {
#if 0
    //if (j+2 >= capacity)
    if (j+7 >= capacity)
    {
      char *buf2;
      capacity = 2*capacity+10;
      if (capacity < 32)
      {
        capacity = 32; // initial size
      }
      buf2 = realloc(buf, capacity);
      if (buf2 == NULL)
      {
        free(buf);
        resultstruct.str = NULL;
        return resultstruct;
      }
      buf = buf2;
    }
#endif
    if (orig[i] != '\\')
    {
      buf[j++] = orig[i++];
      if (orig[i] != '\\' && orig[i] != '"') buf[j++] = orig[i++];
      if (orig[i] != '\\' && orig[i] != '"') buf[j++] = orig[i++];
      if (orig[i] != '\\' && orig[i] != '"') buf[j++] = orig[i++];
      if (orig[i] != '\\' && orig[i] != '"') buf[j++] = orig[i++];
      if (orig[i] != '\\' && orig[i] != '"') buf[j++] = orig[i++];
      if (orig[i] != '\\' && orig[i] != '"') buf[j++] = orig[i++];
      if (orig[i] != '\\' && orig[i] != '"') buf[j++] = orig[i++];
    }
    else if (orig[i+1] == 'x')
    {
      char hexbuf[3] = {0};
      char *endptr;
      hexbuf[0] = orig[i+2];
      hexbuf[1] = orig[i+3];
      buf[j++] = strtol(hexbuf, &endptr, 16);
      if (strlen(hexbuf) != 2 || *endptr != '\0')
      {
        fprintf(stderr, "Invalid string unicode escape: \\x%s\n", hexbuf);
        exit(2);
      }
      i += 4;
    }
    else if (orig[i+1] == 'u')
    {
      char hexbuf[5] = {0};
      uint16_t unicode;
      char *endptr;
      hexbuf[0] = orig[i+2];
      hexbuf[1] = orig[i+3];
      hexbuf[2] = orig[i+4];
      hexbuf[3] = orig[i+5];
      unicode = strtol(hexbuf, &endptr, 16);
      if (strlen(hexbuf) != 4 || *endptr != '\0')
      {
        fprintf(stderr, "Invalid string unicode escape: \\u%s\n", hexbuf);
        exit(2);
      }
      if (unicode <= 0x7F)
      {
        buf[j++] = (char)(uint8_t)unicode;
        i += 6;
        continue;
      }
      if (unicode <= 0x7FF)
      {
        buf[j++] = (char)(uint8_t)(0xc0|(unicode>>6));
        buf[j++] = (char)(uint8_t)(0x80|(unicode&0x3f));
        i += 6;
        continue;
      }
      if (unicode <= 0xFFFF)
      {
        buf[j++] = (char)(uint8_t)(0xe0|(unicode>>12));
        buf[j++] = (char)(uint8_t)(0x80|((unicode>>6)&0x3f));
        buf[j++] = (char)(uint8_t)(0x80|(unicode&0x3f));
        i += 6;
        continue;
      }
      abort();
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
    else
    {
      buf[j++] = orig[i+1];
      i += 2;
    }
  }
#if 0
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
#endif
  if (!orig[i])
  {
    //free(buf);
    resultstruct.str = NULL;
    return resultstruct;
  }
  resultstruct.sz = j;
  buf[j++] = '\0';
  result = buf;
  //result = memdup(buf, j);
  resultstruct.str = result;
  //free(buf);
  if (strendptr)
  {
    *strendptr = &orig[i+1];
  }
  return resultstruct;
}

int dbyymineparse(FILE *f, struct dbyy *dbyy, dbyy_cmdfn_t cmdfn,
                  dbyy_tsfn_t tsfn, void *userdata)
{
  char *lineptr = NULL;
  char *first_string = NULL;
  size_t first_string_len = 0;
  size_t n = 0;
  ssize_t nread;
  int first_line = 1;
  int is_v2 = 0;
  int is_cmd = 0;
  int was_is_cmd = 0;
  size_t len;
  long long nums[3];
  while ((nread = mygetline(&lineptr, &n, f)) >= 0)
  {
    int two_strings_already = 0;
    int colon = 0;
    int numcnt = 0;
    //free(first_string);
    first_string = NULL;
    if (first_line)
    {
      if (strcmp(lineptr, "@v2@\n") == 0)
      {
        is_v2 = 1;
	//printf("V2\n");
	first_line = 0;
	continue;
      }
      else if (strcmp(lineptr, "@v1@\n") == 0)
      {
        is_v2 = 0;
	//printf("V1\n");
	first_line = 0;
	continue;
      }
    }
    if (first_line) first_line = 0;
    is_cmd = (lineptr[0] == '\t');
    if (is_cmd)
    {
      //printf("CMD\n");
      dbyy_add_cmd(dbyy);
    }
    else if (was_is_cmd)
    {
      dbyy_post_cmds(dbyy, cmdfn, userdata);
    }
    was_is_cmd = is_cmd;
    len = whitespacespn(lineptr);
    for (;;)
    {
      if (lineptr[len] == '"')
      {
        char *endptr;
	struct escaped_string es;
	size_t len2;
	if (two_strings_already)
	{
	  free(lineptr);
	  return -1;
	}
        es = dbyy_escape_string(&lineptr[len], &endptr);
	if (es.str == NULL)
	{
	  free(lineptr);
	  return -1;
	}
	len = endptr-lineptr;
	if (lineptr[len] == '\0')
	{
	  break;
	}
	len2 = whitespacespn(&lineptr[len]);
	if (!len2 && lineptr[len] != '\n' && lineptr[len] != '\0' && lineptr[len] != '='
	    && lineptr[len] != ':')
	{
	  free(lineptr);
	  return -1;
	}
	len += len2;
	if (lineptr[len] == '\0')
	{
	  break;
	}
	if (!is_cmd)
	{
	  if (first_string)
	  {
	    //printf("First string: %s\n", first_string);
	    //printf("Second string: %s\n", es.str);
	    dbyy_emplace_rule(dbyy, first_string, first_string_len, es.str, es.sz);
	    //free(es.str);
	    //free(first_string);
	    first_string = NULL;
	    two_strings_already = 1;
	  }
	  else
	  {
	    first_string = es.str;
	    first_string_len = es.sz;
	  }
	}
	else
	{
	  //printf("ARG: %s\n", es.str);
	  dbyy_add_arg(dbyy, es.str);
	  //free(es.str);
	}
      }
      else if (lineptr[len] == ':')
      {
        if (!two_strings_already)
	{
	  free(lineptr);
	  return -1;
	}
	colon = 1;
        //printf("Colon\n");
	len += 1;
	len += whitespacespn(&lineptr[len]);
	if (lineptr[len] == '\0')
	{
	  break;
	}
      }
      else if (lineptr[len] == '=')
      {
        if (!is_v2)
	{
	  free(lineptr);
	  return -1;
	}
        //printf("Equals, first string: %s\n", first_string);
	len += 1;
	len += whitespacespn(&lineptr[len]);
	if (lineptr[len] == '\0')
	{
	  break;
	}
      }
      else if (isdigit(lineptr[len]))
      {
        char *endptr;
	long long x;
        if (!is_v2)
	{
	  free(lineptr);
	  return -1;
	}
	if (numcnt >= 3)
	{
	  free(lineptr);
	  return -1;
	}
        x = strtoll(&lineptr[len], &endptr, 10);
	nums[numcnt++] = x;
        //printf("Digits: %lld\n", x);
	len = endptr-lineptr;
	len += whitespacespn(&lineptr[len]);
	if (lineptr[len] == '\0')
	{
	  break;
	}
      }
      else if (lineptr[len] == '\n' || lineptr[len] == '\0')
      {
        if (is_cmd)
	{
	  dbyy_post_cmd(dbyy);
	}
	else if (numcnt == 3)
	{
          dbyy_emplace_tsdb(dbyy, first_string, first_string_len, nums[0], nums[1], nums[2],
                            tsfn, userdata);
          //free(first_string);
          first_string = NULL;
	}
	else if (first_string == NULL && numcnt == 0)
	{
	  break;
	}
	else if (!two_strings_already || !colon)
	{
	  free(lineptr);
	  return -1;
	}
        break;
      }
      else
      {
        //printf("Stray char: %c\n", lineptr[len]);
        free(lineptr);
        return -1;
      }
    }
  }
  if (was_is_cmd)
  {
    dbyy_post_cmds(dbyy, cmdfn, userdata);
  }
  return 0;
}
