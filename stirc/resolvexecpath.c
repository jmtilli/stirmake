#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "resolvexecpath.h"

char *resolv_exec_path(const char *progname)
{
  char *path, *pathbase;
  char *attemptbuf;
  size_t attemptbufsz;
  int last = 0;
  if (strchr(progname, '/') != NULL)
  {
    return strdup(progname);
  }
  path = getenv("PATH");
  if (path == NULL)
  {
    path = strdup("/bin:/usr/bin");
  }
  else
  {
    path = strdup(path);
  }
  attemptbufsz = strlen(path) + strlen(progname) + 2;
  attemptbuf = malloc(attemptbufsz);
  pathbase = path;
  while (!last)
  {
    char *colon = strchr(path, ':');
    if (colon == NULL)
    {
      last = 1;
    }
    else
    {
      *colon = '\0';
    }
    if (snprintf(attemptbuf, attemptbufsz, "%s/%s", path, progname) >= attemptbufsz)
    {
      abort();
    }
    if (access(attemptbuf, X_OK) == 0)
    {
      char *res = strdup(attemptbuf);
      free(attemptbuf);
      free(pathbase);
      return res;
    }
    path = colon + 1;
  }
  free(attemptbuf);
  free(pathbase);
  return NULL;
}
