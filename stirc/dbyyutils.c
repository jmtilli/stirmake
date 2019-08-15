#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <libgen.h>
#include <arpa/inet.h>
#include "dbyy.h"
#include "dbyyutils.h"

typedef void *yyscan_t;
extern int dbyyparse(yyscan_t scanner, struct dbyy *dbyy);
extern int dbyylex_init(yyscan_t *scanner);
extern void dbyyset_in(FILE *in_str, yyscan_t yyscanner);
extern int dbyylex_destroy(yyscan_t yyscanner);

void dbyydoparse(FILE *filein, struct dbyy *dbyy)
{
  yyscan_t scanner;
  dbyylex_init(&scanner);
  dbyyset_in(filein, scanner);
  if (dbyyparse(scanner, dbyy) != 0)
  {
    fprintf(stderr, "parsing failed\n");
    exit(1);
  }
  dbyylex_destroy(scanner);
  if (!feof(filein))
  {
    fprintf(stderr, "error: additional data at end of dbyy data\n");
    exit(1);
  }
}

void dbyydomemparse(char *filedata, size_t filesize, struct dbyy *dbyy)
{
  FILE *myfile;
  myfile = fmemopen(filedata, filesize, "r");
  if (myfile == NULL)
  {
    fprintf(stderr, "can't open memory file\n");
    exit(1);
  }
  dbyydoparse(myfile, dbyy);
  if (fclose(myfile) != 0)
  {
    fprintf(stderr, "can't close memory file\n");
    exit(1);
  }
}

void dbyynameparse(const char *fname, struct dbyy *dbyy, int require)
{
  FILE *dbyyfile;
  dbyyfile = fopen(fname, "r");
  if (dbyyfile == NULL)
  {
    if (require)
    {
      fprintf(stderr, "File %s cannot be opened\n", fname);
      exit(1);
    }
#if 0
    if (dbyy_postprocess(dbyy) != 0)
    {
      exit(1);
    }
#endif
    return;
  }
  dbyydoparse(dbyyfile, dbyy);
#if 0
  if (dbyy_postprocess(dbyy) != 0)
  {
    exit(1);
  }
#endif
  fclose(dbyyfile);
}

void dbyydirparse(
  const char *argv0, const char *fname, struct dbyy *dbyy, int require)
{
  const char *dir;
  char *copy = strdup(argv0);
  char pathbuf[PATH_MAX];
  dir = dirname(copy); // NB: not for multi-threaded operation!
  snprintf(pathbuf, sizeof(pathbuf), "%s/%s", dir, fname);
  free(copy);
  dbyynameparse(pathbuf, dbyy, require);
}
