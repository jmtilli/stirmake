#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <libgen.h>
#include "stiryy.h"
#include "yyutils.h"

typedef void *yyscan_t;
extern int stiryyparse(yyscan_t scanner, struct stiryy *stiryy);
extern int stiryylex_init(yyscan_t *scanner);
extern void stiryyset_in(FILE *in_str, yyscan_t yyscanner);
extern void stiryyset_extra (void *user_defined, yyscan_t yyscanner);
extern int stiryylex_destroy(yyscan_t yyscanner);

int stiryydoparse(FILE *filein, struct stiryy *stiryy)
{
  yyscan_t scanner;
  stiryylex_init(&scanner);
  stiryyset_extra(NULL, scanner);
  stiryyset_in(filein, scanner);
  if (stiryyparse(scanner, stiryy) != 0)
  {
    return -EBADMSG;
  }
  stiryylex_destroy(scanner);
  if (!feof(filein))
  {
    fprintf(stderr, "stirmake: Additional data at end of Stirfile.\n");
    return -EBADMSG;
  }
  return 0;
}

void stiryydomemparse(char *filedata, size_t filesize, struct stiryy *stiryy)
{
  FILE *myfile;
  myfile = fmemopen(filedata, filesize, "r");
  if (myfile == NULL)
  {
    fprintf(stderr, "can't open memory file\n");
    exit(1);
  }
  stiryydoparse(myfile, stiryy);
  if (fclose(myfile) != 0)
  {
    fprintf(stderr, "can't close memory file\n");
    exit(1);
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

struct escaped_string yy_escape_string(char *orig)
{
  char *buf = NULL;
  char *result = NULL;
  struct escaped_string resultstruct;
  size_t j = 0;
  size_t capacity = 0;
  size_t i = 1;
  while (orig[i] != '"')
  {
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
    if (orig[i] != '\\')
    {
      buf[j++] = orig[i++];
    }
    else if (orig[i+1] == 'x')
    {
      char hexbuf[3] = {0};
      hexbuf[0] = orig[i+2];
      hexbuf[1] = orig[i+3];
      buf[j++] = strtol(hexbuf, NULL, 16);
      i += 4;
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

struct escaped_string yy_escape_string_single(char *orig)
{
  char *buf = NULL;
  char *result = NULL;
  struct escaped_string resultstruct;
  size_t j = 0;
  size_t capacity = 0;
  size_t i = 1;
  while (orig[i] != '\'')
  {
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
    if (orig[i] != '\\')
    {
      buf[j++] = orig[i++];
    }
    else if (orig[i+1] == 'x')
    {
      char hexbuf[3] = {0};
      hexbuf[0] = orig[i+2];
      hexbuf[1] = orig[i+3];
      buf[j++] = strtol(hexbuf, NULL, 16);
      i += 4;
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

int stiryynameparse(const char *fname, struct stiryy *stiryy, int require)
{
  FILE *stiryyfile;
  int ret;
  stiryyfile = fopen(fname, "r");
  if (stiryyfile == NULL)
  {
    if (require)
    {
      fprintf(stderr, "File %s cannot be opened\n", fname);
      exit(1);
    }
#if 0
    if (stiryy_postprocess(stiryy) != 0)
    {
      exit(1);
    }
#endif
    return -ENOENT;
  }
  ret = stiryydoparse(stiryyfile, stiryy);
#if 0
  if (stiryy_postprocess(stiryy) != 0)
  {
    exit(1);
  }
#endif
  fclose(stiryyfile);
  return ret;
}

int stiryydirparse(
  const char *argv0, const char *fname, struct stiryy *stiryy, int require)
{
  const char *dir;
  char *copy = strdup(argv0);
  char pathbuf[PATH_MAX];
  dir = dirname(copy); // NB: not for multi-threaded operation!
  snprintf(pathbuf, sizeof(pathbuf), "%s/%s", dir, fname);
  free(copy);
  return stiryynameparse(pathbuf, stiryy, require);
}

int do_dirinclude(struct stiryy *stiryy, int noproj, const char *fname)
{
  struct stiryy stiryy2 = {};
  size_t fsz = strlen(stiryy->curprefix) + strlen(fname) + 8 + 3;
  size_t psz = strlen(stiryy->curprefix) + strlen(fname) + 2;
  size_t ppsz = strlen(stiryy->curprojprefix) + strlen(fname) + 2;
  char *prefix = malloc(psz);
  char *projprefix = malloc(ppsz);
  char *filename = malloc(fsz);
  char realpathname[PATH_MAX];
  char *prefix2, *projprefix2;
  int ret;
  FILE *f;
  struct abce_mb oldscope;
  size_t oldscopeidx;
  if (snprintf(prefix, psz, "%s/%s", stiryy->curprefix, fname) >= psz)
  {
    my_abort();
  }
  if (snprintf(projprefix, ppsz, "%s/%s", stiryy->curprojprefix, fname) >= ppsz)
  {
    my_abort();
  }
  if (snprintf(filename, fsz, "%s/%s/%s", stiryy->curprefix, fname, "Stirfile") >= fsz)
  {
    my_abort();
  }
  if (realpath(filename, realpathname) == NULL)
  {
    printf("path %s does not exist\n", filename);
    return -ENOENT;
  }
  if (strcmp(realpathname, stiryy->main->realpathname) == 0)
  {
    stiryy->main->subdirseen = 1;
    if (noproj && stiryy->sameproject)
    {
      stiryy->main->subdirseen_sameproject = 1;
    }
  }
  prefix2 = canon(prefix);
  projprefix2 = canon(projprefix);
  if (!noproj)
  {
    // replace projprefix2
    free(projprefix2);
    projprefix2 = strdup(".");
  }
  oldscope = stiryy->main->abce->dynscope;
  oldscopeidx = oldscope.u.area->u.sc.locidx;
  stiryy->main->abce->dynscope = abce_mb_create_scope(stiryy->main->abce, ABCE_DEFAULT_SCOPE_SIZE, &oldscope, 0);
  abce_mb_refdn(stiryy->main->abce, &oldscope);
  //printf("projprefix2: %s\n", projprefix2);
  struct scope_ud ud = {
    .prefix = prefix2,
    .prjprefix = projprefix2,
  };
  abce_scope_set_userdata(&stiryy->main->abce->dynscope, &ud);
  if (stiryy->main->abce->dynscope.typ == ABCE_T_N)
  {
    my_abort();
  }
  stiryy_init(&stiryy2, stiryy->main, prefix2, projprefix2, stiryy->main->abce->dynscope, stiryy->dirname, filename, !noproj);
  stiryy2.sameproject = stiryy->sameproject && noproj;

  f = fopen(filename, "r");
  if (!f)
  {
    fprintf(stderr, "stirmake: Can't open substirfile %s.\n", filename);
    return -ENOENT;
  }
  ret = stiryydoparse(f, &stiryy2);
  fclose(f);
  if (ret)
  {
    fprintf(stderr, "stirmake: Can't parse substirfile %s.\n", filename);
    return -EBADMSG;
  }

  stiryy_free(&stiryy2);
  abce_mb_refdn(stiryy->main->abce, &stiryy->main->abce->dynscope);
  stiryy->main->abce->dynscope = abce_mb_refup(stiryy->main->abce, &stiryy->main->abce->cachebase[oldscopeidx]);
  //get_abce(stiryy)->dynscope = oldscope;
  // free(prefix2); // let it leak, FIXME free it someday
  // free(projprefix2); // let it leak, FIXME free it someday
  free(prefix);
  free(projprefix);
  free(filename);
  return 0;
}

int do_fileinclude(struct stiryy *stiryy, const char *fname, int ignore)
{
  struct stiryy stiryy2 = {};
  int ret;
  FILE *f;

  stiryy_init(&stiryy2, stiryy->main, stiryy->curprefix, stiryy->curprojprefix, stiryy->main->abce->dynscope, stiryy->dirname, fname, 0);
  stiryy2.sameproject = stiryy->sameproject;

  f = fopen(fname, "r");
  if (!f)
  {
    if (ignore)
    {
      return 0;
    }
    fprintf(stderr, "stirmake: Can't open substirfile %s.\n", fname);
    return -ENOENT;
  }
  ret = stiryydoparse(f, &stiryy2);
  fclose(f);
  if (ret)
  {
    fprintf(stderr, "stirmake: Can't parse substirfile %s.\n", fname);
    return -EBADMSG;
  }

  stiryy_free(&stiryy2);

  return 0;
}

int
engine_stringlist(struct abce *abce,
                  size_t ip,
                  const char *directive,
                  char ***strs, size_t *strsz)
{
  unsigned char tmpbuf[64] = {};
  size_t tmpsiz = 0;
  size_t i;
  struct abce_mb mb = {};

  *strs = NULL;
  *strsz = 0;

  abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_PUSH_DBL);
  abce_add_double_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ip);
  abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_JMP);

  if (abce->sp != 0)
  {
    abort();
  }
  if (abce_engine(abce, tmpbuf, tmpsiz) != 0)
  {
    printf("Error executing bytecode for %s directive\n", directive);
    printf("error %s\n", abce_err_to_str(abce->err.code));
    printf("Backtrace:\n");
    for (i = 0; i < abce->btsz; i++)
    {
      if (abce->btbase[i].typ == ABCE_T_S)
      {
        printf("%s\n", abce->btbase[i].u.area->u.str.buf);
      }
      else
      {
        printf("(-)\n");
      }
    }
    printf("Additional information:\n");
    abce_mb_dump(&abce->err.mb);
    return -EINVAL;
  }
  if (abce_getmb(&mb, abce, 0) != 0)
  {
    printf("can't get item from stack in %s\n", directive);
    return -EINVAL;
    //printf("expected array, got type %d\n", get_abce(amyplanyy)->err.mb.typ);
  }
  if (mb.typ == ABCE_T_S)
  {
    *strsz = 1;
    *strs = malloc(sizeof(**strs) * (*strsz));
    (*strs)[0] = strdup(mb.u.area->u.str.buf);
  }
  else if (mb.typ == ABCE_T_A)
  {
    for (i = 0; i < mb.u.area->u.ar.size; i++)
    {
      if (mb.u.area->u.ar.mbs[i].typ != ABCE_T_S)
      {
        printf("expected string, got type %d for directive %s\n",
               mb.u.area->u.ar.mbs[i].typ, directive);
        return -EINVAL;
      }
    }
    *strsz = mb.u.area->u.ar.size;
    *strs = malloc(sizeof(**strs) * (*strsz));
    for (i = 0; i < *strsz; i++)
    {
      (*strs)[i] = strdup(mb.u.area->u.ar.mbs[i].u.area->u.str.buf);
    }
  }
  else
  {
    printf("expected str or array, got type %d in %s\n",
           mb.typ, directive);
    return -EINVAL;
  }
  abce_mb_refdn(abce, &mb);
  if (abce->sp != 1)
  {
    abort();
  }
  abce_pop(abce);
  return 0;
}

int add_rule_yy(struct stiryy_main *main, struct tgt *tgts, size_t tgtsz,
                struct dep *deps, size_t depsz,
                struct cmdsrc *shells,
                int phony, int rectgt, int detouch, int maybe, int dist,
                int cleanhook, int distcleanhook, int bothcleanhook,
                int deponly,
                char *prefix, size_t scopeidx)
{
  if (main->rule_in_progress)
  {
    return -EINVAL;
  }
  stiryy_main_emplace_rule(main, prefix, scopeidx);
  main->rules[main->rulesz-1].bases = NULL;
  main->rules[main->rulesz-1].basesz = 0;
  main->rules[main->rulesz-1].basecapacity = 0;
  main->rules[main->rulesz-1].deps = deps;
  main->rules[main->rulesz-1].depsz = depsz;
  main->rules[main->rulesz-1].depcapacity = depsz;
  main->rules[main->rulesz-1].targets = tgts;
  main->rules[main->rulesz-1].targetsz = tgtsz;
  main->rules[main->rulesz-1].targetcapacity = tgtsz;
  main->rules[main->rulesz-1].shells = *shells;
  //main->rules[main->rulesz-1].scopeidx = scopeidx;
  //main->rules[main->rulesz-1].prefix = prefix;
  main->rules[main->rulesz-1].phony = !!phony;
  main->rules[main->rulesz-1].rectgt = !!rectgt;
  main->rules[main->rulesz-1].detouch = !!detouch;
  main->rules[main->rulesz-1].maybe = !!maybe;
  main->rules[main->rulesz-1].dist = !!dist;
  main->rules[main->rulesz-1].iscleanhook = !!cleanhook;
  main->rules[main->rulesz-1].isdistcleanhook = !!distcleanhook;
  main->rules[main->rulesz-1].isbothcleanhook = !!bothcleanhook;
  main->rules[main->rulesz-1].deponly = !!deponly;
  main->rule_in_progress = 0;
  return 0;
}
