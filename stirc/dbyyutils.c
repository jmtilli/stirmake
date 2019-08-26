#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <libgen.h>
#include "dbyy.h"
#include "dbyyutils.h"

typedef void *yyscan_t;
extern int dbyyparse(yyscan_t scanner, struct dbyy *dbyy);
extern int dbyylex_init(yyscan_t *scanner);
extern void dbyyset_in(FILE *in_str, yyscan_t yyscanner);
extern int dbyylex_destroy(yyscan_t yyscanner);

int dbyydoparse(FILE *filein, struct dbyy *dbyy)
{
  yyscan_t scanner;
  dbyylex_init(&scanner);
  dbyyset_in(filein, scanner);
  if (dbyyparse(scanner, dbyy) != 0)
  {
    fprintf(stderr, "parsing failed\n");
    return -EBADMSG;
  }
  dbyylex_destroy(scanner);
  if (!feof(filein))
  {
    fprintf(stderr, "error: additional data at end of dbyy data\n");
    return -EBADMSG;
  }
  return 0;
}

int dbyydomemparse(char *filedata, size_t filesize, struct dbyy *dbyy)
{
  FILE *myfile;
  int ret;
  myfile = fmemopen(filedata, filesize, "r");
  if (myfile == NULL)
  {
    fprintf(stderr, "can't open memory file\n");
    return -ENOENT;
  }
  ret = dbyydoparse(myfile, dbyy);
  if (fclose(myfile) != 0)
  {
    fprintf(stderr, "can't close memory file\n");
    return -EBADF;
  }
  return ret;
}

int dbyynameparse(const char *fname, struct dbyy *dbyy)
{
  FILE *dbyyfile;
  int ret;
  dbyyfile = fopen(fname, "r");
  if (dbyyfile == NULL)
  {
#if 0
    if (dbyy_postprocess(dbyy) != 0)
    {
      exit(1);
    }
#endif
    return -ENOENT;
  }
  ret = dbyydoparse(dbyyfile, dbyy);
#if 0
  if (dbyy_postprocess(dbyy) != 0)
  {
    exit(1);
  }
#endif
  fclose(dbyyfile);
  return ret;
}

int dbyydirparse(
  const char *argv0, const char *fname, struct dbyy *dbyy)
{
  const char *dir;
  char *copy = strdup(argv0);
  char pathbuf[PATH_MAX];
  dir = dirname(copy); // NB: not for multi-threaded operation!
  snprintf(pathbuf, sizeof(pathbuf), "%s/%s", dir, fname);
  free(copy);
  return dbyynameparse(pathbuf, dbyy);
}
