#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <libgen.h>
#include "incyy.h"
#include "incyyutils.h"

typedef void *yyscan_t;
extern int incyyparse(yyscan_t scanner, struct incyy *incyy);
extern int incyylex_init(yyscan_t *scanner);
extern void incyyset_in(FILE *in_str, yyscan_t yyscanner);
extern int incyylex_destroy(yyscan_t yyscanner);

void errxit(const char *fmt, ...);

void incyydoparse(FILE *filein, struct incyy *incyy)
{
  yyscan_t scanner;
  incyylex_init(&scanner);
  incyyset_in(filein, scanner);
  if (incyyparse(scanner, incyy) != 0)
  {
    if (strcmp(incyy->prefix, ".") == 0)
    {
      errxit("parsing failed %s", incyy->fnamenodir);
    }
    else
    {
      errxit("parsing failed %s/%s", incyy->prefix, incyy->fnamenodir);
    }
    exit(1);
  }
  incyylex_destroy(scanner);
  if (!feof(filein))
  {
    if (strcmp(incyy->prefix, ".") == 0)
    {
      errxit("error: additional data at end of incyy data %s", incyy->fnamenodir);
    }
    else
    {
      errxit("error: additional data at end of incyy data %s/%s", incyy->prefix, incyy->fnamenodir);
    }
    exit(1);
  }
}

void incyydomemparse(char *filedata, size_t filesize, struct incyy *incyy)
{
  FILE *myfile;
  myfile = fmemopen(filedata, filesize, "r");
  if (myfile == NULL)
  {
    fprintf(stderr, "can't open memory file\n");
    exit(1);
  }
  incyydoparse(myfile, incyy);
  if (fclose(myfile) != 0)
  {
    fprintf(stderr, "can't close memory file\n");
    exit(1);
  }
}

void incyynameparse(const char *fname, struct incyy *incyy, int require)
{
  FILE *incyyfile;
  incyyfile = fopen(fname, "r");
  if (incyyfile == NULL)
  {
    if (require)
    {
      fprintf(stderr, "File %s cannot be opened\n", fname);
      exit(1);
    }
#if 0
    if (incyy_postprocess(incyy) != 0)
    {
      exit(1);
    }
#endif
    return;
  }
  incyydoparse(incyyfile, incyy);
#if 0
  if (incyy_postprocess(incyy) != 0)
  {
    exit(1);
  }
#endif
  fclose(incyyfile);
}

void incyydirparse(
  const char *argv0, const char *fname, struct incyy *incyy, int require)
{
  const char *dir;
  char *copy = strdup(argv0);
  char pathbuf[PATH_MAX];
  dir = dirname(copy); // NB: not for multi-threaded operation!
  snprintf(pathbuf, sizeof(pathbuf), "%s/%s", dir, fname);
  free(copy);
  incyynameparse(pathbuf, incyy, require);
}
