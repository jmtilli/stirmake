/*
%code requires {
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void *yyscan_t;
#endif
#include "stiryy.h"
#include <sys/types.h>
}

%define api.prefix {stiryy}
*/

%{

/*
#define YYSTYPE STIRYYSTYPE
#define YYLTYPE STIRYYLTYPE
*/

#include "stiryy.h"
#include "yyutils.h"
#include "stiryy.tab.h"
#include "stiryy.lex.h"
#include "stiropcodes.h"
#include "stirtrap.h"
#if 0
#include "abce/stiryy.h"
#include "abce/stiryyutils.h"
#endif
#include "abce/abce.h"
#include "abce/amyplan.h"
#include "abce/abceopcodes.h"
#include "abce/amyplanlocvarctx.h"
#include "git.h"
#include <arpa/inet.h>

void my_abort(void);

int shell_dummy;

void stiryyerror(/*YYLTYPE *yylloc,*/ yyscan_t scanner, struct stiryy *stiryy, const char *str)
{
  //fprintf(stderr, "error: %s at line %d col %d\n",str, yylloc->first_line, yylloc->first_column);
  // FIXME we need better location info!
  if (stiryy->dirname == NULL)
  {
    fprintf(stderr, "stirmake: %s at file %s line %d col %d.\n", str, stiryy->filename, stiryyget_lineno(scanner), stiryyget_column(scanner));
  }
  else
  {
    fprintf(stderr, "stirmake: %s at file %s/%s line %d col %d\n", str, stiryy->dirname, stiryy->filename, stiryyget_lineno(scanner), stiryyget_column(scanner));
  }
}

void recommend(/*YYLTYPE *yylloc,*/ yyscan_t scanner, struct stiryy *stiryy, const char *str)
{
  // FIXME we need better location info!
  if (stiryy->dirname == NULL)
  {
    fprintf(stderr, "stirmake: %s at file %s line %d col %d.\n", str, stiryy->filename, stiryyget_lineno(scanner), stiryyget_column(scanner));
  }
  else
  {
    fprintf(stderr, "stirmake: %s at file %s/%s line %d col %d\n", str, stiryy->dirname, stiryy->filename, stiryyget_lineno(scanner), stiryyget_column(scanner));
  }
}

int stiryywrap(yyscan_t scanner)
{
        return 1;
}

static inline int amyplanyy_do_emit(struct stiryy *amyplanyy)
{
  return amyplanyy->do_emit;
}

static inline int is_autocall(struct stiryy *amyplanyy)
{
  return 0;
}
void add_corresponding_get(struct stiryy *stiryy, double get)
{
  stiryy_add_byte(stiryy, (uint16_t)get);
}

void add_corresponding_set(struct stiryy *stiryy, double get)
{
  uint16_t uset = (uint16_t)get_corresponding_set((uint16_t)get);
  stiryy_add_byte(stiryy, uset);
}

#define get_abce(stiryy) ((stiryy)->main->abce)

int handle_tgt_tgtdepref(yyscan_t scanner, struct stiryy *stiryy, size_t bcodeoff, int is_dist)
{
  if (amyplanyy_do_emit(stiryy))
  {
    size_t strsz;
    size_t i;
    int ret;
    char **strs;

    stiryy_add_byte(stiryy, ABCE_OPCODE_EXIT);

    ret = engine_stringlist(get_abce(stiryy), bcodeoff, "target", &strs, &strsz);
    if (ret)
    {
      stiryyerror(scanner, stiryy, "error in targets");
      return -EINVAL;
    }

    for (i = 0; i < strsz; i++)
    {
      stiryy_set_tgt(stiryy, strs[i], is_dist);
      free(strs[i]);
    }
    free(strs);
  }
  return 0;
}
void handle_tgt_freeform_token(yyscan_t scanner, struct stiryy *stiryy, const char *tkn, int is_dist)
{
  if (amyplanyy_do_emit(stiryy))
  {
    if (!stiryy->main->freeform_token_seen)
    {
      recommend(scanner, stiryy, "Recommend using string literals instead of free-form tokens; recommend also using @strict mode");
      stiryy->main->freeform_token_seen=1;
    }
    stiryy_set_tgt(stiryy, tkn, is_dist);
  }
}

#define amyplanyy stiryy
#define amyplanyy_add_byte stiryy_add_byte
#define amyplanyy_add_double stiryy_add_double
#define amyplanyy_set_double stiryy_set_double
#define amyplanyy_add_fun_sym stiryy_add_fun_sym
#define amyplan_symbol_add stiryy_symbol_add
#define amyplanyyerror stiryyerror

%}

%pure-parser
%lex-param {yyscan_t scanner}
%parse-param {yyscan_t scanner}
%parse-param {struct stiryy *stiryy}
/* %locations */

%union {
  int i;
  double d;
  char *s;
  struct escaped_string str;
  struct {
    int i;
    char *s;
  } both;
  struct {
    uint8_t has_i:1;
    uint8_t has_prio:1;
    int prio;
  } tokenoptstmp;
  struct {
    uint8_t i:1;
    int prio;
  } tokenopts;
}

/*
%destructor { free ($$.str); } STRING_LITERAL
%destructor { free ($$); } FREEFORM_TOKEN
%destructor { free ($$); } VARREF_LITERAL
%destructor { free ($$); } SHELL_COMMAND
%destructor { free ($$); } PERCENTLUA_LITERAL
*/


%token <s> PERCENTLUA_LITERAL
%token OPEN_BRACKET
%token CLOSE_BRACKET
%token OPEN_BRACE
%token CLOSE_BRACE
%token OPEN_PAREN
%token CLOSE_PAREN
%token DUMMY_TOK1
%token DUMMY_TOK2

%token <s> SHELL_COMMAND
%token ATTAB ATATTAB

%token EIGHT SIXTEENBE THIRTYTWOBE SIXTEENLE THIRTYTWOLE

%token NEWLINE
%token DYNLUACALL LEXLUACALL

%token CLEANHOOK DISTCLEANHOOK BOTHCLEANHOOK

%token BEGINSCOPE BEGINHOLEYSCOPE ENDSCOPE
%token FORDICT FORDICTPREV

%token FOPEN FCLOSE FREAD FSEEK FFLUSH FWRITE

%token ONCE ENDONCE STDOUT STDERR ERROR DUMP EXIT
%token EQUALS
%token PLUSEQUALS
%token QMEQUALS
%token COLONEQUALS
%token PLUSCOLONEQUALS
%token QMCOLONEQUALS
%token SHELLEQUALS
%token SHELLSHELLEQUALS
%token COLON
%token COMMA
%token SCOPENAME
%token <str> STRING_LITERAL
%token <d> NUMBER
%token <s> VARREF_LITERAL
%token <s> FREEFORM_TOKEN
%token MAYBE_CALL
%token CALL
%token LT
%token GT
%token LE
%token GE
%token AT
%token FUNCTION
%token ENDFUNCTION
%token LOCVAR
%token RECDEP
%token ORDER
%token WAIT
%token ORDERONLY
%token DEPONLY

%token DELAYVAR
%token DELAYEXPR
%token DELAYLISTEXPAND
%token SUFFILTER
%token SUFFILTEROUT
%token PATHSUFFIX
%token PATHBASENAME
%token PATHSIMPLIFY
%token PATHDIR
%token PATHNOTDIR
%token PATHSUFFIXALL
%token PATHBASENAMEALL
%token PATHSIMPLIFYALL
%token PATHDIRALL
%token PATHNOTDIRALL
%token GLOB
%token JSONIN
%token SUFSUBONE
%token STRAPPEND
%token SUFSUBALL
%token PHONYRULE
%token MAYBERULE
%token DISTRULE
%token DISTTGT
%token PATRULE
%token RECTGTRULE
%token DETOUCHRULE
%token FILEINCLUDE
%token DIRINCLUDE PROJDIRINCLUDE
%token CDEPINCLUDES
%token AUTOPHONY
%token AUTOTARGET
%token IGNORE
%token NOECHO
%token ISMAKE
%token DYNO
%token LEXO
%token IMMO
%token DYN
%token LEX
%token IMM
%token D
%token L
%token I
%token DO
%token LO
%token IO
%token DP LP IP DPO LPO IPO
%token LOC
%token SC SCO
%token APPEND
%token APPEND_LIST
%token RETURN
%token ADD_RULE
%token ADD_DEPS
%token RULE_DIST
%token RULE_PHONY
%token RULE_ORDINARY
%token PRINT
%token PERIOD

%token DICTNEXT DICTPREV
%token ATQM SCOPE TRUE TYPE FALSE NIL STR_FROMCHR STR_LOWER STR_UPPER
%token STR_REVERSE STRCMP STRSTR STRREP STRLISTJOIN STRSTRIP STRSUB
%token STRGSUB STRSET STRWORD STRWORDCNT STRWORDLIST ABS ACOS ASIN ATAN
%token CEIL COS SIN TAN EXP LOG SQRT DUP_NONRECURSIVE PB_NEW TOSTRING
%token TONUMBER SCOPE_PARENT SCOPE_NEW GETSCOPE_DYN GETSCOPE_LEX
%token GETENV CHOMP
%token STR2PB PB2STR
%token FLOOR TRUNC ROUND
%token SPLICE


%token DIRUP DIRDOWN

%token IF
%token ELSE
%token ELSEIF
%token ENDIF
%token FOR
%token ENDFOR
%token WHILE
%token ENDWHILE
%token BREAK
%token CONTINUE

%token DIV MUL ADD SUB SHL SHR NE EQ LOGICAL_AND LOGICAL_OR LOGICAL_NOT MOD BITWISE_AND BITWISE_OR BITWISE_NOT BITWISE_XOR

%token TOPLEVEL SUBFILE
%token VERSION

%token ERROR_TOK

%type<d> value
%type<d> tgtdepref
%type<d> lvalue
%type<d> fordict
%type<d> arglist
%type<d> valuelistentry
%type<d> maybe_arglist
%type<d> maybe_atqm
%type<d> maybe_distrule
%type<d> maybe_disttgt
%type<d> dynstart
%type<d> scopstart
%type<d> lexstart
%type<d> varref_tail
%type<d> varref

%type<d> maybeqmequals
%type<d> maybe_rec
%type<d> maybe_maybe_call
%type<d> shellequals
%type<d> dirinclude
%type<d> cdepincludes
%type<d> cdepspecifiers
%type<d> cdepspecifier
%type<d> maybeignore
%type<d> rulespecifiers

%type<d> scopetype
%type<d> beginscope
%type<s> maybe_name
%type<s> maybe_includescopevar

%start st

%%

st: stirrules;

stirrules:
  newlines topmarker NEWLINE amyplanrules;

newlines:
| newlines NEWLINE
;

topmarker:
  TOPLEVEL
{
  if (!stiryy->expect_toplevel)
  {
    if (!stiryy->main->trial)
    {
      recommend(scanner, stiryy, "Expected @subfile, got @toplevel. Exiting.\n");
    }
    YYABORT;
  }
}
| SUBFILE
{
  if (stiryy->expect_toplevel)
  {
    if (!stiryy->main->trial)
    {
      recommend(scanner, stiryy, "Expected @toplevel, got @subfile. Exiting.\n");
    }
    YYABORT;
  }
}
;

shellequals: SHELLEQUALS {$$ = 0;} | SHELLSHELLEQUALS {$$ = 1;} ;

custom_assign:
  VARREF_LITERAL shellequals
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t funloc = get_abce(amyplanyy)->bytecodesz;
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FUN_HEADER);
    amyplanyy_add_double(amyplanyy, 0);
    $<d>$ = funloc;
  }
}
  expr NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    unsigned char tmpbuf[256] = {};
    size_t tmpsiz = 0;
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_RET);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FUN_TRAILER);
    amyplanyy_add_double(amyplanyy, amyplan_symbol_add(amyplanyy, $1, strlen($1)));
    if (get_abce(amyplanyy)->sp != 0)
    {
      abort();
    }
    abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_PUSH_DBL);
    abce_add_double_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), $<d>3);
      //abce_sc_get_rec_str_fun(&get_abce(amyplanyy)->dynscope, $1, 1));
    abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_FUNIFY);
    abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_CALL_IF_FUN);
    abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_EXIT);

    get_abce(amyplanyy)->ip = -tmpsiz-ABCE_GUARD;
    if (abce_engine(get_abce(amyplanyy), tmpbuf, tmpsiz) != 0)
    {
      size_t i;
      printf("Error executing bytecode for var %s\n", $1);
      printf("error %s\n", abce_err_to_str(get_abce(amyplanyy)->err.code));
      printf("Backtrace:\n");
      for (i = 0; i < get_abce(amyplanyy)->btsz; i++)
      {
        if (get_abce(amyplanyy)->btbase[i].typ == ABCE_T_S)
        {
          printf("%s\n", get_abce(amyplanyy)->btbase[i].u.area->u.str.buf);
        }
        else
        {
          printf("(-)\n");
        }
      }
      printf("Additional information:\n");
      abce_mb_dump(&get_abce(amyplanyy)->err.mb);
      amyplanyyerror(scanner, amyplanyy, "error in assignment");
      YYABORT;
    }
    if (get_abce(amyplanyy)->sp != 1)
    {
      abort();
    }
    if (get_abce(amyplanyy)->stackbase[0].typ != ABCE_T_A)
    {
      amyplanyyerror(scanner, amyplanyy, "shell-assign supported only from non-empty array of strings");
      YYABORT;
    }
    if (get_abce(amyplanyy)->stackbase[0].u.area->u.ar.size == 0)
    {
      amyplanyyerror(scanner, amyplanyy, "shell-assign supported only from non-empty array of strings");
      YYABORT;
    }
    char **argv;
    argv = malloc(sizeof(*argv) * (get_abce(amyplanyy)->stackbase[0].u.area->u.ar.size + 1));
    size_t idx;
    for (idx = 0; idx < get_abce(amyplanyy)->stackbase[0].u.area->u.ar.size; idx++)
    {
      if (get_abce(amyplanyy)->stackbase[0].u.area->u.ar.mbs[idx].typ != ABCE_T_S)
      {
        amyplanyyerror(scanner, amyplanyy, "shell-assign supported only from non-empty array of strings");
        YYABORT;
      }
      argv[idx] = get_abce(amyplanyy)->stackbase[0].u.area->u.ar.mbs[idx].u.area->u.str.buf;
    }
    argv[get_abce(amyplanyy)->stackbase[0].u.area->u.ar.size] = NULL;
    char *res = eval_cmd(argv);
    if (res == NULL)
    {
      amyplanyyerror(scanner, amyplanyy, "shell-assign failed");
      YYABORT;
    }

    if ($2)
    {
      size_t it = 0;
      it = strspn(res, " \t\r\n");
      struct abce_mb *mbar = abce_mb_cpush_create_array(get_abce(amyplanyy));
      // FIXME what if it's null?
      abce_push_mb(get_abce(amyplanyy), mbar);
      abce_cpop(get_abce(amyplanyy));
      mbar = NULL;
      //abce_mb_refdn(get_abce(amyplanyy), &mbar);

      while (res[it] != '\0')
      {
        size_t it2;
        it2 = it + strcspn(res + it, " \t\r\n");
        struct abce_mb *mbstr = abce_mb_cpush_create_string(get_abce(amyplanyy), res + it, it2 - it);
        // FIXME what if it's null?
        if (abce_mb_array_append(get_abce(amyplanyy), &get_abce(amyplanyy)->stackbase[1], mbstr) != 0)
        {
          amyplanyyerror(scanner, amyplanyy, "shell-assign failed in abce (OOM)");
          YYABORT;
        }
        abce_cpop(get_abce(amyplanyy));
        mbstr = NULL;
        //abce_mb_refdn(get_abce(amyplanyy), &mbstr);
        it = it2 + strspn(res + it2, " \t\r\n");
      }

      struct abce_mb *key = abce_mb_cpush_create_string(get_abce(amyplanyy), $1, strlen($1));
      // FIXME what if it's null?
      abce_sc_replace_val_mb(get_abce(amyplanyy), &get_abce(amyplanyy)->dynscope, key, &get_abce(amyplanyy)->stackbase[1]);
      abce_cpop(get_abce(amyplanyy));
      key = NULL;
      //abce_mb_refdn(get_abce(amyplanyy), &key);
      abce_pop(get_abce(amyplanyy));
    }
    else
    {
      struct abce_mb *mbstr = abce_mb_cpush_create_string(get_abce(amyplanyy), res, strlen(res));
      // FIXME what if it's null?
      abce_push_mb(get_abce(amyplanyy), mbstr);
      abce_cpop(get_abce(amyplanyy));
      //abce_mb_refdn(get_abce(amyplanyy), &mbstr);

      struct abce_mb *key = abce_mb_cpush_create_string(get_abce(amyplanyy), $1, strlen($1));
      // FIXME what if it's null?
      abce_sc_replace_val_mb(get_abce(amyplanyy), &get_abce(amyplanyy)->dynscope, key, &get_abce(amyplanyy)->stackbase[1]);
      abce_cpop(get_abce(amyplanyy));
      //abce_mb_refdn(get_abce(amyplanyy), &key);
      abce_pop(get_abce(amyplanyy));
    }

    abce_pop(get_abce(amyplanyy));
  }

  free($1);
}
;


custom_stmt:
  ADD_DEPS OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, STIR_OPCODE_DEP_ADD);
  }
}
| ADD_RULE OPEN_PAREN expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, STIR_OPCODE_RULE_ADD);
  }
}
;

custom_expr0:
  DIRUP
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_TOP_DIR);
}
| DIRDOWN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_CUR_DIR_FROM_TOP);
}
| SUFSUBONE OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_SUFSUBONE);
}
| SUFFILTER OPEN_PAREN expr COMMA expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_SUFFILTER);
}
| SUFFILTEROUT OPEN_PAREN expr COMMA expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_SUFFILTEROUT);
}
| SUFSUBALL OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_SUFSUBALL);
}
| PATHSUFFIX OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_PATHSUFFIX);
}
| PATHBASENAME OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_PATHBASENAME);
}
| PATHDIR OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_PATHDIR);
}
| PATHNOTDIR OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_PATHNOTDIR);
}
| PATHSIMPLIFY OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_PATHSIMPLIFY);
}
| PATHSUFFIXALL OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_PATHSUFFIXALL);
}
| PATHBASENAMEALL OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_PATHBASENAMEALL);
}
| PATHDIRALL OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_PATHDIRALL);
}
| PATHNOTDIRALL OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_PATHNOTDIRALL);
}
| PATHSIMPLIFYALL OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_PATHSIMPLIFYALL);
}
| GLOB OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_GLOB);
}
| JSONIN OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, STIR_OPCODE_JSON_IN);
}
;

custom_callable:
  STDOUT OPEN_PAREN expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, 0);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_OUT);
  }
}
| STDERR OPEN_PAREN expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, 1);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_OUT);
  }
}
| ERROR OPEN_PAREN expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ERROR);
  }
}
| DUMP OPEN_PAREN expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DUMP);
  }
}
| custom_stmt
| expr
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
  }
}
;

ordertoken:
  FREEFORM_TOKEN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_set_order(stiryy, $1);
  }
  free($1);
}
| tgtdepref
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    int ret;
    char **strs;
    size_t strsz;

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);

    ret = engine_stringlist(get_abce(amyplanyy), $1, "order spec", &strs, &strsz);

    if (ret)
    {
      stiryyerror(scanner, stiryy, "error in @order");
      YYABORT;
    }

    if (strsz != 1)
    {
      stiryyerror(scanner, stiryy, "error in @order: stringlist size not 1");
      YYABORT;
    }

    stiryy_set_order(stiryy, strs[0]);
    free(strs[0]);
    free(strs);
  }
}
;

custom_rule:
  ORDER
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_add_order(stiryy);
  }
}
  ordertoken ordertoken NEWLINE
| stirrule
| PERCENTLUA_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
#ifdef WITH_LUA
    if (abce_ensure_lua(get_abce(amyplanyy)->dynscope.u.area, get_abce(amyplanyy)) != 0)
    {
      stiryyerror(scanner, stiryy, "Not enough memory for Lua");
      YYABORT;
    }
    if (luaL_dostring(get_abce(amyplanyy)->dynscope.u.area->u.sc.lua, $1) != 0)
    {
      stiryyerror(scanner, stiryy, "Lua error");
      stiryyerror(scanner, stiryy,
                  lua_tostring(get_abce(amyplanyy)->dynscope.u.area->u.sc.lua,
                               -1));
      YYABORT;
    }
#else
    stiryyerror(scanner, stiryy, "Lua not compiled in");
    YYABORT;
#endif
  }
}
| PRINT NUMBER NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    printf("%g\n", $2);
  }
}
| CALL
{
  $<d>$ = get_abce(amyplanyy)->bytecodesz;
}
  custom_callable NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    unsigned char tmpbuf[256] = {};
    size_t tmpsiz = 0;

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);

    abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_PUSH_DBL);
    abce_add_double_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), $<d>2);
    abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_JMP);

    if (abce_engine(get_abce(amyplanyy), tmpbuf, tmpsiz) != 0)
    {
      size_t i;
      printf("Error executing bytecode for call directive\n");
      printf("error %s\n", abce_err_to_str(get_abce(amyplanyy)->err.code));
      printf("Backtrace:\n");
      for (i = 0; i < get_abce(amyplanyy)->btsz; i++)
      {
        if (get_abce(amyplanyy)->btbase[i].typ == ABCE_T_S)
        {
          printf("%s\n", get_abce(amyplanyy)->btbase[i].u.area->u.str.buf);
        }
        else
        {
          printf("(-)\n");
        }
      }
      printf("Additional information:\n");
      abce_mb_dump(&get_abce(amyplanyy)->err.mb);
      stiryyerror(scanner, stiryy, "error in @call");
      YYABORT;
    }
    if (get_abce(amyplanyy)->sp != 0)
    {
      abort();
    }
  }
}
/*
| FILEINCLUDE STRING_LITERAL NEWLINE
{
  free($2.str);
}
*/
| FILEINCLUDE maybeignore
{
  $<d>$ = get_abce(amyplanyy)->bytecodesz;
}
  expr NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t strsz, i;
    int ret;
    char **strs;

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);

    ret = engine_stringlist(get_abce(amyplanyy), $<d>3, "fileinclude", &strs, &strsz);
    if (ret)
    {
      stiryyerror(scanner, stiryy, "error in @fileinclude");
      YYABORT;
    }

    for (i = 0; i < strsz; i++)
    {
      if (do_fileinclude(stiryy, strs[i], $2) != 0)
      {
        YYABORT;
      }
      free(strs[i]);
    }
    free(strs);
  }
}
/* // shift-reduce conflict!
| dirinclude STRING_LITERAL NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (do_dirinclude(stiryy, $1, $2.str) != 0)
    {
      YYABORT;
    }
  }
  free($2.str);
}
*/
| dirinclude
{
  $<d>$ = get_abce(amyplanyy)->bytecodesz;
}
  maybe_includescopevar
  expr NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t strsz, i;
    int ret;
    char **strs;

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);

    ret = engine_stringlist(get_abce(amyplanyy), $<d>2, "dirinclude", &strs, &strsz);
    if (ret)
    {
      stiryyerror(scanner, stiryy, "error in @dirinclude");
      YYABORT;
    }
    if ($3 != NULL && strsz != 1)
    {
      printf("If setting include scope, must have 1 include, has %zu includes\n", strsz);
      stiryyerror(scanner, stiryy, "error in @dirinclude");
      YYABORT;
    }

    for (i = 0; i < strsz; i++)
    {
      if (do_dirinclude(stiryy, $1, strs[i], $3) != 0)
      {
        YYABORT;
      }
      free(strs[i]);
    }
    free(strs);
  }
}
/*
| CDEPINCLUDESCURDIR STRING_LITERAL NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_set_cdepinclude(stiryy, $2.str);
  }
  free($2.str);
}
*/
| cdepincludes
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if ($1 != 7)
    {
      recommend(scanner, stiryy, "Recommend @autophony, @autotarget and @ignore with @cdepincludes");
    }
  }
  $<d>$ = get_abce(amyplanyy)->bytecodesz;
}
  expr NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t strsz;
    size_t i;
    int ret;
    char **strs;

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);

    ret = engine_stringlist(get_abce(amyplanyy), $<d>2, "cdepincludes", &strs, &strsz);
    if (ret)
    {
      stiryyerror(scanner, stiryy, "error in @cdepincludes");
      YYABORT;
    }

    for (i = 0; i < strsz; i++)
    {
      unsigned u1 = (unsigned)$1;
      stiryy_set_cdepinclude(stiryy, strs[i],
                             !!(u1 & 1), !!(u1 & 2), !!(u1 & 4));
      free(strs[i]);
    }
    free(strs);
  }
}
| IF
{
  size_t exprloc = get_abce(amyplanyy)->bytecodesz;
  $<d>$ = exprloc;
}
  OPEN_PAREN expr CLOSE_PAREN NEWLINE
{
  double oldval = amyplanyy->do_emit;
  $<d>$ = oldval;
}
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    unsigned char tmpbuf[64] = {};
    size_t tmpsiz = 0;

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);
    abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_PUSH_DBL);
    abce_add_double_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), $<d>2);
    abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_JMP);
    //get_abce(amyplanyy)->ip = $<d>2;
    //printf("ip: %d\n", (int)get_abce(amyplanyy)->ip);
    if (get_abce(amyplanyy)->sp != 0)
    {
      abort();
    }
    if (abce_engine(get_abce(amyplanyy), tmpbuf, tmpsiz) != 0)
    {
      size_t i;
      printf("Error executing bytecode for @if directive\n");
      printf("error %s\n", abce_err_to_str(get_abce(amyplanyy)->err.code));
      printf("Backtrace:\n");
      for (i = 0; i < get_abce(amyplanyy)->btsz; i++)
      {
        if (get_abce(amyplanyy)->btbase[i].typ == ABCE_T_S)
        {
          printf("%s\n", get_abce(amyplanyy)->btbase[i].u.area->u.str.buf);
        }
        else
        {
          printf("(-)\n");
        }
      }
      printf("Additional information:\n");
      abce_mb_dump(&get_abce(amyplanyy)->err.mb);
      stiryyerror(scanner, stiryy, "error in @if");
      YYABORT;
    }
    if (get_abce(amyplanyy)->sp != 1)
    {
      abort();
    }
    int b;
    if (abce_getboolean(&b, get_abce(amyplanyy), 0) != 0)
    {
      printf("expected boolean, got type %d\n", get_abce(amyplanyy)->err.mb.typ);
      stiryyerror(scanner, stiryy, "error in @if");
      YYABORT;
    }
    if (!b)
    {
      amyplanyy->do_emit = 0;
    }
    abce_pop(get_abce(amyplanyy));
    $<d>$ = !!b;
  }
  else
  {
    $<d>$ = 1;
  }
}
  amyplanrules
{
  $<d>$ = $<d>8;
  amyplanyy->do_emit = (int)$<d>7;
}
  maybe_stirelseifs
  maybe_stirelse
  ENDIF NEWLINE
{
  amyplanyy->do_emit = (int)$<d>7;
}
;

maybe_includescopevar:
{
  $$ = NULL;
}
| SCOPENAME VARREF_LITERAL
{
  $$ = $2;
}
;

maybe_stirelseifs:
{
  $<d>$ = $<d>0;
}
| maybe_stirelseifs ELSEIF
{
  size_t exprloc = get_abce(amyplanyy)->bytecodesz;
  $<d>$ = exprloc;
  if (!(int)$<d>1)
  {
    amyplanyy->do_emit = 1;
  }
}
OPEN_PAREN expr CLOSE_PAREN NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if ((int)$<d>1)
    {
      $<d>$ = 1;
      amyplanyy->do_emit = 0;
    }
    else
    {
      unsigned char tmpbuf[64] = {};
      size_t tmpsiz = 0;

      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);
      abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_PUSH_DBL);
      abce_add_double_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), $<d>3);
      abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_JMP);
      //get_abce(amyplanyy)->ip = $<d>2;
      //printf("ip: %d\n", (int)get_abce(amyplanyy)->ip);
      if (get_abce(amyplanyy)->sp != 0)
      {
        abort();
      }
      if (abce_engine(get_abce(amyplanyy), tmpbuf, tmpsiz) != 0)
      {
        size_t i;
        printf("Error executing bytecode for @if directive\n");
        printf("error %s\n", abce_err_to_str(get_abce(amyplanyy)->err.code));
        printf("Backtrace:\n");
        for (i = 0; i < get_abce(amyplanyy)->btsz; i++)
        {
          if (get_abce(amyplanyy)->btbase[i].typ == ABCE_T_S)
          {
            printf("%s\n", get_abce(amyplanyy)->btbase[i].u.area->u.str.buf);
          }
          else
          {
            printf("(-)\n");
          }
        }
        printf("Additional information:\n");
        abce_mb_dump(&get_abce(amyplanyy)->err.mb);
        stiryyerror(scanner, stiryy, "error in @if");
        YYABORT;
      }
      if (get_abce(amyplanyy)->sp != 1)
      {
        abort();
      }
      int b;
      if (abce_getboolean(&b, get_abce(amyplanyy), 0) != 0)
      {
        printf("expected boolean, got type %d\n", get_abce(amyplanyy)->err.mb.typ);
        stiryyerror(scanner, stiryy, "error in @if");
        YYABORT;
      }
      if (!b)
      {
        amyplanyy->do_emit = 0;
      }
      abce_pop(get_abce(amyplanyy));
      $<d>$ = !!b;
    }
  }
  else
  {
    $<d>$ = 1;
  }
}
  amyplanrules
{
  $<d>$ = $<d>8;
}
;

maybe_stirelse:
| ELSE NEWLINE
{
  if (!(int)$<d>0)
  {
    amyplanyy->do_emit = 1;
  }
  if (amyplanyy_do_emit(amyplanyy))
  {
    if ((int)$<d>0)
    {
      amyplanyy->do_emit = 0;
    }
  }
}
  amyplanrules
;

maybeignore:
  { $$ = 0; }
| IGNORE { $$ = 1; }
;

/* Start of copypaste */

amyplanrules:
| amyplanrules NEWLINE
| amyplanrules assignrule
| amyplanrules VERSION OPEN_PAREN STRING_LITERAL CLOSE_PAREN NEWLINE
{
  int i;
  int found = 0;
  for (i = 0; i < sizeof(gitshas)/sizeof(*gitshas); i++)
  {
    if (strcmp($4.str, gitshas[i]) == 0 && strlen($4.str) == $4.sz)
    {
      found = 1;
    }
  }
  if (!found)
  {
    fprintf(stderr, "Incompatible version of stirmake installed, expected to contain git SHA1:\n");
    fprintf(stderr, "%s\n", $4.str);
    YYABORT;
  }
}
| amyplanrules FUNCTION VARREF_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy->ctx = amyplan_locvarctx_alloc(NULL, 2, (size_t)-1, (size_t)-1);
  }
}
OPEN_PAREN maybe_parlist CLOSE_PAREN NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t funloc = get_abce(amyplanyy)->bytecodesz;
    amyplanyy_add_fun_sym(amyplanyy, $3, 0, funloc);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FUN_HEADER);
    amyplanyy_add_double(amyplanyy, amyplanyy->ctx->args);
  }
}
  funlines
  ENDFUNCTION NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NIL); // retval
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, amyplanyy->ctx->args); // argcnt
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, amyplanyy->ctx->sz - amyplanyy->ctx->args); // locvarcnt
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_RETEX2);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FUN_TRAILER);
    amyplanyy_add_double(amyplanyy, amyplan_symbol_add(amyplanyy, $3, strlen($3)));
    amyplan_locvarctx_free(amyplanyy->ctx);
    amyplanyy->ctx = NULL;
  }
  free($3);
}
| amyplanrules
  beginscope maybe_name NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t oldscopeidx = get_abce(amyplanyy)->dynscope.u.area->u.sc.locidx;
    struct abce_mb oldscope;
    struct abce_mb *newscope;
    struct abce_mb *key;
    void *ud = abce_scope_get_userdata(&get_abce(amyplanyy)->dynscope);

    if ($3)
    {
      key = abce_mb_cpush_create_string(get_abce(amyplanyy), $3, strlen($3));
      if (key == NULL)
      {
        fprintf(stderr, "out of memory\n");
        YYABORT;
      }
    }
    else
    {
      if (abce_cpush_nil(get_abce(amyplanyy)) != 0)
      {
        fprintf(stderr, "out of memory\n");
        YYABORT;
      }
      key = &get_abce(amyplanyy)->cstackbase[get_abce(amyplanyy)->csp-1];
    }
    //abce_push_c(get_abce(amyplanyy)); // FIXME required? only for GC?

    oldscope = get_abce(amyplanyy)->dynscope;
    if (abce_cpush_mb(get_abce(amyplanyy), &oldscope) != 0)
    {
      fprintf(stderr, "out of memory\n");
      YYABORT;
    }
    abce_mb_refdn(get_abce(amyplanyy), &get_abce(amyplanyy)->dynscope);
    oldscopeidx = oldscope.u.area->u.sc.locidx;
    newscope = abce_mb_cpush_create_scope(get_abce(amyplanyy), ABCE_DEFAULT_SCOPE_SIZE, &oldscope, (int)$2);
    if (newscope == NULL)
    {
      fprintf(stderr, "out of memory\n");
      YYABORT;
    }
    get_abce(amyplanyy)->dynscope = abce_mb_refup(get_abce(amyplanyy), newscope);
    abce_cpop(get_abce(amyplanyy));

    if ($3)
    {
      abce_sc_replace_val_mb(get_abce(amyplanyy), &oldscope, key, &get_abce(amyplanyy)->dynscope);
    }
    abce_scope_set_userdata(&get_abce(amyplanyy)->dynscope, ud);
    //abce_pop(get_abce(amyplanyy)); // FIXME required? only for GC?
    abce_cpop(get_abce(amyplanyy));
    abce_cpop(get_abce(amyplanyy));
    $<d>$ = oldscopeidx;
  }
  free($3);
}
  amyplanrules
  ENDSCOPE NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    struct abce_mb creatscope;
    creatscope = get_abce(amyplanyy)->dynscope;
    get_abce(amyplanyy)->dynscope = abce_mb_refup(get_abce(amyplanyy), &get_abce(amyplanyy)->cachebase[(size_t)$<d>5]);
    abce_mb_refdn(get_abce(amyplanyy), &creatscope);
  }
}
| amyplanrules custom_rule
;

beginscope: BEGINSCOPE {$$ = 0;} | BEGINHOLEYSCOPE {$$ = 1;} ;
maybe_name: {$$ = NULL;} | VARREF_LITERAL {$$ = $1; } ;

maybeqmequals: EQUALS {$$ = 0;} | QMEQUALS {$$ = 1;} ;
maybe_maybe_call: {$$ = 0;} | MAYBE_CALL {$$ = 1;};

assignrule:
  custom_assign
| VARREF_LITERAL maybe_maybe_call maybeqmequals
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t funloc = get_abce(amyplanyy)->bytecodesz;
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FUN_HEADER);
    amyplanyy_add_double(amyplanyy, 0);
    $<d>$ = funloc;
  }
}
expr NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    unsigned char tmpbuf[256] = {};
    size_t tmpsiz = 0;
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_RET);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FUN_TRAILER);
    amyplanyy_add_double(amyplanyy, amyplan_symbol_add(amyplanyy, $1, strlen($1)));

    if ($2)
    {
      amyplanyy_add_fun_sym(amyplanyy, $1, $3, $<d>4);
    }
    else if (!$3 || abce_sc_get_rec_str_area(get_abce(amyplanyy)->dynscope.u.area, $1, 1) == NULL)
    {
      if (get_abce(amyplanyy)->sp != 0)
      {
        abort();
      }
      abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_PUSH_DBL);
      abce_add_double_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), $<d>4);
        //abce_sc_get_rec_str_fun(&get_abce(amyplanyy)->dynscope, $1, 1));
      abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_FUNIFY);
      abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_CALL_IF_FUN);
      abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_EXIT);

      get_abce(amyplanyy)->ip = -tmpsiz-ABCE_GUARD;
      if (abce_engine(get_abce(amyplanyy), tmpbuf, tmpsiz) != 0)
      {
        size_t i;
        printf("Error executing bytecode for var %s\n", $1);
        printf("error %s\n", abce_err_to_str(get_abce(amyplanyy)->err.code));
        printf("Backtrace:\n");
        for (i = 0; i < get_abce(amyplanyy)->btsz; i++)
        {
          if (get_abce(amyplanyy)->btbase[i].typ == ABCE_T_S)
          {
            printf("%s\n", get_abce(amyplanyy)->btbase[i].u.area->u.str.buf);
          }
          else
          {
            printf("(-)\n");
          }
        }
        printf("Additional information:\n");
        abce_mb_dump(&get_abce(amyplanyy)->err.mb);
        amyplanyyerror(scanner, amyplanyy, "error in assignment");
        YYABORT;
      }
      if (get_abce(amyplanyy)->sp != 1)
      {
        abort();
      }
      struct abce_mb *key = abce_mb_cpush_create_string(get_abce(amyplanyy), $1, strlen($1));
      abce_sc_replace_val_mb(get_abce(amyplanyy), &get_abce(amyplanyy)->dynscope, key, &get_abce(amyplanyy)->stackbase[0]);
      abce_cpop(get_abce(amyplanyy));
      abce_pop(get_abce(amyplanyy));
    }
  }

  free($1);
}
| VARREF_LITERAL maybe_maybe_call PLUSEQUALS
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t funloc = get_abce(amyplanyy)->bytecodesz;
    size_t oldloc = amyplanyy_add_fun_sym(amyplanyy, $1, 0, funloc); // FIXME move later
    if (oldloc == (size_t)-1)
    {
      printf("Can't find old symbol function for %s\n", $1);
      YYABORT;
    }
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FUN_HEADER);
    amyplanyy_add_double(amyplanyy, 0);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, oldloc);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL_IF_FUN);
    // FIXME what if it's not a list?
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DUP_NONRECURSIVE);
  }
}
expr NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    unsigned char tmpbuf[256] = {};
    size_t tmpsiz = 0;
    size_t symidx;
    // FIXME what if it's not a list?
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPENDALL_MAINTAIN);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_RET);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FUN_TRAILER);
    symidx = amyplan_symbol_add(amyplanyy, $1, strlen($1));
    amyplanyy_add_double(amyplanyy, symidx);

    if (!$2)
    {
      if (get_abce(amyplanyy)->sp != 0)
      {
        abort();
      }
      abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_PUSH_DBL);
      abce_add_double_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf),
        abce_sc_get_rec_str_fun(&get_abce(amyplanyy)->dynscope, $1, 1));
      abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_FUNIFY);
      abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_CALL_IF_FUN);
      abce_add_ins_alt(tmpbuf, &tmpsiz, sizeof(tmpbuf), ABCE_OPCODE_EXIT);

      get_abce(amyplanyy)->ip = -tmpsiz-ABCE_GUARD;
      if (abce_engine(get_abce(amyplanyy), tmpbuf, tmpsiz) != 0)
      {
        size_t i;
        printf("Error executing bytecode for var %s\n", $1);
        printf("error %s\n", abce_err_to_str(get_abce(amyplanyy)->err.code));
        printf("Backtrace:\n");
        for (i = 0; i < get_abce(amyplanyy)->btsz; i++)
        {
          if (get_abce(amyplanyy)->btbase[i].typ == ABCE_T_S)
          {
            printf("%s\n", get_abce(amyplanyy)->btbase[i].u.area->u.str.buf);
          }
          else
          {
            printf("(-)\n");
          }
        }
        printf("Additional information:\n");
        abce_mb_dump(&get_abce(amyplanyy)->err.mb);
        amyplanyyerror(scanner, amyplanyy, "error in += assignment");
        YYABORT;
      }
      if (get_abce(amyplanyy)->sp != 1)
      {
        abort();
      }
      struct abce_mb *key = abce_mb_cpush_create_string(get_abce(amyplanyy), $1, strlen($1));
      abce_sc_replace_val_mb(get_abce(amyplanyy), &get_abce(amyplanyy)->dynscope, key, &get_abce(amyplanyy)->stackbase[0]);
      abce_cpop(get_abce(amyplanyy));
      abce_pop(get_abce(amyplanyy));
    }
  }

  free($1);
}
;

maybe_parlist:
| parlist
;

parlist:
VARREF_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplan_locvarctx_add_param(amyplanyy->ctx, $1);
  }
  free($1);
}
| parlist COMMA VARREF_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplan_locvarctx_add_param(amyplanyy->ctx, $3);
  }
  free($3);
}
;

funlines:
  locvarlines
  bodylines
;

locvarlines:
| locvarlines LOCVAR VARREF_LITERAL EQUALS expr NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    int ret = amyplan_locvarctx_add(amyplanyy->ctx, $3);
    if (ret == -EEXIST)
    {
      printf("Local variable %s exists already\n", $3);
      amyplanyyerror(scanner, amyplanyy, "can't have two same names");
      YYABORT;
    }
    else if (ret == -ENOMEM)
    {
      printf("Not enough memory for local variable %s\n", $3);
      amyplanyyerror(scanner, amyplanyy, "not enough memory");
      YYABORT;
    }
    else if (ret != 0)
    {
      printf("Can't create local variable %s\n", $3);
      amyplanyyerror(scanner, amyplanyy, "unknown error");
      YYABORT;
    }
  }
  free($3);
}
| locvarlines NEWLINE
;

bodylines:
| statement NEWLINE bodylinescont
;

bodylinescont:
| bodylinescont statement NEWLINE
| bodylinescont NEWLINE
;

statement:
  lvalue EQUALS SUB
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if ($1 != ABCE_OPCODE_DICTSET_MAINTAIN)
    {
      printf("Can remove only from dict\n");
      YYABORT;
    }
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DICTDEL);
  }
}
| lvalue EQUALS expr
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if ($1 == ABCE_OPCODE_STRGET)
    {
      printf("Can't assign to string\n");
      YYABORT;
    }
    if ($1 == ABCE_OPCODE_LISTPOP)
    {
      printf("Can't assign to pop query\n");
      YYABORT;
    }
    if ($1 == ABCE_OPCODE_DICTHAS)
    {
      printf("Can't assign to dictionary query\n");
      YYABORT;
    }
    if ($1 == ABCE_OPCODE_SCOPE_HAS)
    {
      printf("Can't assign to scope query\n");
      YYABORT;
    }
    if ($1 == ABCE_OPCODE_PUSH_FROM_CACHE)
    {
      printf("Can't assign to immediate varref\n");
      YYABORT;
    }
    if (   $1 == ABCE_OPCODE_STRLEN || $1 == ABCE_OPCODE_LISTLEN
        || $1 == ABCE_OPCODE_DICTLEN)
    {
      printf("Can't assign to length query (except for PB)\n");
      YYABORT;
    }
    add_corresponding_set(amyplanyy, $1);
    if ($1 == ABCE_OPCODE_DICTGET) // prev. changes from GET to SET_MAINTAIN
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
    }
  }
}
| RETURN expr
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, amyplan_locvarctx_arg_sz(amyplanyy->ctx)); // arg
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, amyplan_locvarctx_recursive_sz(amyplanyy->ctx)); // loc
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_RETEX2);
  }
}
| BREAK
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    int64_t loc = amyplan_locvarctx_break(amyplanyy->ctx, 1);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FALSE);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, loc);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
  }
}
| CONTINUE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    int64_t loc = amyplan_locvarctx_continue(amyplanyy->ctx, 1);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, loc);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
  }
}
| BREAK NUMBER
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t sz = $2;
    int64_t loc;
    if ((double)sz != $2 || sz == 0)
    {
      printf("Break count not positive integer\n");
      YYABORT;
    }
    loc = amyplan_locvarctx_break(amyplanyy->ctx, sz);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FALSE);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, loc);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
  }
}
| CONTINUE NUMBER
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t sz = $2;
    int64_t loc;
    if ((double)sz != $2 || sz == 0)
    {
      printf("Continue count not positive integer\n");
      YYABORT;
    }
    loc = amyplan_locvarctx_continue(amyplanyy->ctx, sz);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, loc);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
  }
}
| expr
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP); // called for side effects only
  }
}
| IF OPEN_PAREN expr CLOSE_PAREN NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    $<d>$ = get_abce(amyplanyy)->bytecodesz;
    amyplanyy_add_double(amyplanyy, -50); // to be overwritten
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_IF_NOT_JMP);
  }
}
  bodylinescont
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_set_double(amyplanyy, $<d>6, get_abce(amyplanyy)->bytecodesz);
    $<d>$ = $<d>6; // For overwrite by maybe_else
  }
}
  maybe_elseifs
  maybe_else
  ENDIF
| fordict VARREF_LITERAL COMMA VARREF_LITERAL OPEN_PAREN expr CLOSE_PAREN NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    struct amyplan_locvarctx *ctx;
    int64_t locvarkey;
    int64_t locvarval;
    locvarkey = amyplan_locvarctx_search_rec(amyplanyy->ctx, $2);
    if (locvarkey < 0)
    {
      printf("var %s not found\n", $2);
      YYABORT;
    }
    locvarval = amyplan_locvarctx_search_rec(amyplanyy->ctx, $4);
    if (locvarval < 0)
    {
      printf("var %s not found\n", $4);
      YYABORT;
    }

    //amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    //amyplanyy_add_double(amyplanyy, -50); // to be overwritten
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NIL); // key
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NIL); // val

    size_t addressof_iter = get_abce(amyplanyy)->bytecodesz;
  //iter:
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP); // pop val
    if ($1 != 0)
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_TRUE);
    }
    else
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FALSE);
    }

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DICTNEXT_SAFE);

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, -2);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_STACK);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TYPE);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, ABCE_T_N);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_NE);
    size_t addressof_break = get_abce(amyplanyy)->bytecodesz;
  //midpoint:
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    size_t tomodify = get_abce(amyplanyy)->bytecodesz;
    amyplanyy_add_double(amyplanyy, -50); // to be overwritten
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_IF_NOT_JMP);

    // set key and val to the named variables
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, locvarval);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, -2);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_STACK);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SET_STACK);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, locvarkey);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, -3);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_STACK);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SET_STACK);

    ctx =
      amyplan_locvarctx_alloc(amyplanyy->ctx, 0, addressof_break, addressof_iter);
    ctx->sz = 3;
    if (ctx == NULL)
    {
      printf("Out of memory\n");
      YYABORT;
    }
    amyplanyy->ctx = ctx;
    $<d>$ = tomodify;
  }
}
  bodylinescont
  ENDFOR
{
  struct amyplan_locvarctx *ctx = amyplanyy->ctx->parent;
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, amyplanyy->ctx->jmpaddr_continue);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
//iterend:
  amyplanyy_set_double(amyplanyy, $<d>9, get_abce(amyplanyy)->bytecodesz);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP); // pop val
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP); // pop key
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP); // pop dict ref

  free(amyplanyy->ctx);
  amyplanyy->ctx = ctx;
}
| FOR OPEN_PAREN statement COMMA
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    $<d>$ = get_abce(amyplanyy)->bytecodesz; // midpoint, $5
  }
}
  expr
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    $<d>$ = get_abce(amyplanyy)->bytecodesz; // addressof_breakpoint, $7
    amyplanyy_add_double(amyplanyy, -50); // to be overwritten
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
  }
}
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    $<d>$ = get_abce(amyplanyy)->bytecodesz; // startpoint, $8
  }
}
  COMMA statement CLOSE_PAREN NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    struct amyplan_locvarctx *ctx;
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, $<d>5); // addressof_midpoint
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
    ctx =
      amyplan_locvarctx_alloc(amyplanyy->ctx, 0, get_abce(amyplanyy)->bytecodesz, $<d>8);
    if (ctx == NULL)
    {
      printf("Out of memory\n");
      YYABORT;
    }
    amyplanyy->ctx = ctx;
    amyplanyy_set_double(amyplanyy, $<d>7, get_abce(amyplanyy)->bytecodesz);
    $<d>$ = get_abce(amyplanyy)->bytecodesz; // breakpoint
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  }
}
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    $<d>$ = get_abce(amyplanyy)->bytecodesz;
    amyplanyy_add_double(amyplanyy, -50); // addressof_endpoint, $14
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_IF_NOT_JMP);
  }
}
  bodylinescont
  ENDFOR
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    struct amyplan_locvarctx *ctx = amyplanyy->ctx->parent;
    free(amyplanyy->ctx);
    amyplanyy->ctx = ctx;

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, $<d>8); // addressof_startpoint
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
    amyplanyy_set_double(amyplanyy, $<d>14, get_abce(amyplanyy)->bytecodesz);
  }
}
| WHILE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    $<d>$ = get_abce(amyplanyy)->bytecodesz; // startpoint
  }
}
  OPEN_PAREN expr CLOSE_PAREN NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    struct amyplan_locvarctx *ctx =
      amyplan_locvarctx_alloc(amyplanyy->ctx, 0, get_abce(amyplanyy)->bytecodesz, $<d>2);
    if (ctx == NULL)
    {
      printf("Out of memory\n");
      YYABORT;
    }
    amyplanyy->ctx = ctx;
    $<d>$ = get_abce(amyplanyy)->bytecodesz; // breakpoint
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, -50); // to be overwritten
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_IF_NOT_JMP);
  }
}
  bodylinescont
  ENDWHILE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    struct amyplan_locvarctx *ctx = amyplanyy->ctx->parent;
    free(amyplanyy->ctx);
    amyplanyy->ctx = ctx;
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, $<d>2);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
    amyplanyy_set_double(amyplanyy, $<d>7 + 1, get_abce(amyplanyy)->bytecodesz);
  }
}
| ONCE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    $<d>$ = get_abce(amyplanyy)->bytecodesz; // startpoint
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_TRUE);
  }
}
  NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    struct amyplan_locvarctx *ctx =
      amyplan_locvarctx_alloc(amyplanyy->ctx, 0, get_abce(amyplanyy)->bytecodesz, $<d>2);
    if (ctx == NULL)
    {
      printf("Out of memory\n");
      YYABORT;
    }
    amyplanyy->ctx = ctx;
    $<d>$ = get_abce(amyplanyy)->bytecodesz; // breakpoint
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, -50); // to be overwritten
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_IF_NOT_JMP);
  }
}
  bodylinescont
  ENDONCE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    struct amyplan_locvarctx *ctx = amyplanyy->ctx->parent;
    free(amyplanyy->ctx);
    amyplanyy->ctx = ctx;
    amyplanyy_set_double(amyplanyy, $<d>4 + 1, get_abce(amyplanyy)->bytecodesz);
  }
}
| APPEND OPEN_PAREN expr COMMA expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPEND_MAINTAIN);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
  }
}
| APPEND_LIST OPEN_PAREN expr COMMA expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPENDALL_MAINTAIN);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
  }
}
| STDOUT OPEN_PAREN expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, 0);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_OUT);
  }
}
| STDERR OPEN_PAREN expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, 1);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_OUT);
  }
}
| ERROR OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ERROR); }
| DUMP OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DUMP); }
| EXIT OPEN_PAREN CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT); }
| EXIT OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT); }
| custom_stmt
;

maybe_elseifs:
{
  $<d>$ = $<d>0;
}
| maybe_elseifs ELSEIF
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    $<d>$ = get_abce(amyplanyy)->bytecodesz;
    amyplanyy_add_double(amyplanyy, -50); // to be overwritten
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
    amyplanyy_set_double(amyplanyy, $<d>1, get_abce(amyplanyy)->bytecodesz); // Overwrite, mid1
  }
}
  OPEN_PAREN expr CLOSE_PAREN NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    $<d>$ = get_abce(amyplanyy)->bytecodesz;
    amyplanyy_add_double(amyplanyy, -50); // to be overwritten, FIXME!
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_IF_NOT_JMP);
  }
}
  bodylinescont
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_set_double(amyplanyy, $<d>8, get_abce(amyplanyy)->bytecodesz);
    amyplanyy_set_double(amyplanyy, $<d>3, get_abce(amyplanyy)->bytecodesz);
  }
  $<d>$ = $<d>8; // for overwrite by maybe_else
}
;

maybe_else:
| ELSE NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    $<d>$ = get_abce(amyplanyy)->bytecodesz;
    amyplanyy_add_double(amyplanyy, -50); // to be overwritten
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
    amyplanyy_set_double(amyplanyy, $<d>0, get_abce(amyplanyy)->bytecodesz); // Overwrite
  }
}
bodylinescont
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_set_double(amyplanyy, $<d>3, get_abce(amyplanyy)->bytecodesz);
  }
}
;

varref_tail:
  OPEN_BRACKET expr CLOSE_BRACKET
{
  $$ = ABCE_OPCODE_LISTGET;
}
| OPEN_BRACKET SUB CLOSE_BRACKET
{
  $$ = ABCE_OPCODE_LISTPOP; // This is special. Can't assign to pop query.
}
| OPEN_BRACKET CLOSE_BRACKET
{
  $$ = ABCE_OPCODE_LISTLEN; // This is special. Can't assign to length query.
}
| OPEN_BRACE expr CLOSE_BRACE
{
  $$ = ABCE_OPCODE_DICTGET;
}
| OPEN_BRACE CLOSE_BRACE
{
  $$ = ABCE_OPCODE_DICTLEN; // This is special. Can't assign to length query.
}
| OPEN_BRACE ATQM expr CLOSE_BRACE
{
  $$ = ABCE_OPCODE_DICTHAS; // This is special. Can't assign to "has" query.
}
| OPEN_BRACKET AT expr CLOSE_BRACKET
{
  $$ = ABCE_OPCODE_STRGET; // This is special. Can't assign to string.
}
| OPEN_BRACKET AT CLOSE_BRACKET
{
  $$ = ABCE_OPCODE_STRLEN; // This is special. Can't assign to length query.
}
| OPEN_BRACE AT expr COMMA expr CLOSE_BRACE
{
  $$ = ABCE_OPCODE_PBGET; // offset comma transfer size (0, 1, 2, -1, -2)
}
| OPEN_BRACE AT CLOSE_BRACE
{
  $$ = ABCE_OPCODE_PBLEN; // This is very special: CAN assign to length query
}
;

fordict:
  FORDICT
{
  $$ = 0;
}
| FORDICTPREV
{
  $$ = 1;
}
;

lvalue:
  varref
{
  $$ = $1;
}
| varref
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, $1);
  }
}
  maybe_bracketexprlist varref_tail
{
  $$ = $4;
}
| dynstart
{
  $$ = $1;
}
| dynstart
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, $1);
  }
}
  maybe_bracketexprlist varref_tail
{
  $$ = $4;
}
| scopstart
{
  $$ = $1;
}
| scopstart
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, $1);
  }
}
  maybe_bracketexprlist varref_tail
{
  $$ = $4;
}
| lexstart
{
  $$ = $1;
}
| lexstart
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, $1);
  }
}
  maybe_bracketexprlist varref_tail
{
  $$ = $4;
}
| OPEN_PAREN expr CLOSE_PAREN maybe_bracketexprlist varref_tail
{
  $$ = $5;
}
;

maybe_atqm:
{
  $$ = ABCE_OPCODE_SCOPEVAR;
}
| ATQM
{
  $$ = ABCE_OPCODE_SCOPE_HAS;
}
;

lexstart:
  LEX
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, get_abce(amyplanyy)->dynscope.u.area->u.sc.locidx);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  }
}
  OPEN_BRACKET maybe_atqm expr CLOSE_BRACKET
{
  $$ = $4;
}
;

dynstart:
  DYN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN);
  }
}
  OPEN_BRACKET maybe_atqm expr CLOSE_BRACKET
{
  $$ = $4;
}
;

scopstart:
  SCOPE OPEN_BRACKET expr COMMA maybe_atqm expr CLOSE_BRACKET
{
  $$ = $5;
}
;

maybe_bracketexprlist:
| maybe_bracketexprlist varref_tail
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if ($2 == ABCE_OPCODE_LISTSET)
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LISTGET);
    }
    else
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DICTGET);
    }
  }
}
;

value:
  AT expr
{
  $$ = 1;
}
| expr
{
  $$ = 0;
}
;

varref:
  VARREF_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    int64_t locvar;
    if (amyplanyy->ctx == NULL)
    {
      // Outside of function, search for dynamic symbol
      // Doesn't really happen with standard syntax, but somebody may embed it
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN);

      int64_t idx = abce_cache_add_str(get_abce(amyplanyy), $1, strlen($1));
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
      amyplanyy_add_double(amyplanyy, idx);
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
      $$ = ABCE_OPCODE_SCOPEVAR;
    }
    else
    {
      locvar = amyplan_locvarctx_search_rec(amyplanyy->ctx, $1);
      if (locvar >= 0)
      {
        amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
        amyplanyy_add_double(amyplanyy, locvar);
      }
      else
      {
        printf("var %s not found\n", $1);
        YYABORT;
      }
      $$ = ABCE_OPCODE_PUSH_STACK;
    }
  }
  free($1);
}
| IO VARREF_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    const struct abce_mb *mb2 =
      abce_sc_get_rec_str(&get_abce(amyplanyy)->dynscope, $2, 0);
    if (mb2 == NULL)
    {
      printf("Variable %s not found\n", $2);
      YYABORT;
    }
    int64_t idx = abce_cache_add(get_abce(amyplanyy), mb2);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, idx);
  }
  free($2);
  $$ = ABCE_OPCODE_PUSH_FROM_CACHE;
}
| I VARREF_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    const struct abce_mb *mb2 =
      abce_sc_get_rec_str(&get_abce(amyplanyy)->dynscope, $2, 1);
    if (mb2 == NULL)
    {
      printf("Variable %s not found\n", $2);
      YYABORT;
    }
    int64_t idx = abce_cache_add(get_abce(amyplanyy), mb2);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, idx);
  }
  free($2);
  $$ = ABCE_OPCODE_PUSH_FROM_CACHE;
}
| IP VARREF_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (get_abce(amyplanyy)->dynscope.u.area->u.sc.parent == NULL)
    {
      printf("No parent scope, can't use immediate parent reference\n");
      YYABORT;
    }
    struct abce_mb mb1 = {.typ = ABCE_T_SC, .u = {.area = get_abce(amyplanyy)->dynscope.u.area->u.sc.parent}};
    const struct abce_mb *mb2 = abce_sc_get_rec_str(&mb1, $2, 1);
    if (mb2 == NULL)
    {
      printf("Variable %s not found\n", $2);
      YYABORT;
    }
    int64_t idx = abce_cache_add(get_abce(amyplanyy), mb2);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, idx);
  }
  free($2);
  $$ = ABCE_OPCODE_PUSH_FROM_CACHE;
}
| IPO VARREF_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (get_abce(amyplanyy)->dynscope.u.area->u.sc.parent == NULL)
    {
      printf("No parent scope, can't use immediate parent reference\n");
      YYABORT;
    }
    struct abce_mb mb1 = {.typ = ABCE_T_SC, .u = {.area = get_abce(amyplanyy)->dynscope.u.area->u.sc.parent}};
    const struct abce_mb *mb2 = abce_sc_get_rec_str(&mb1, $2, 0);
    if (mb2 == NULL)
    {
      printf("Variable %s not found\n", $2);
      YYABORT;
    }
    int64_t idx = abce_cache_add(get_abce(amyplanyy), mb2);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, idx);
  }
  free($2);
  $$ = ABCE_OPCODE_PUSH_FROM_CACHE;
}
| scopetype VARREF_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    int64_t idx = abce_cache_add_str(get_abce(amyplanyy), $2, strlen($2));
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, idx);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  }
  free($2);
  if ($1)
  {
    $$ = ABCE_OPCODE_SCOPEVAR_NONRECURSIVE;
  }
  else
  {
    $$ = ABCE_OPCODE_SCOPEVAR;
  }
}
;

scopetype:
  LP
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    if (get_abce(amyplanyy)->dynscope.u.area->u.sc.parent == NULL)
    {
      printf("No parent scope, can't use lexical parent reference\n");
      YYABORT;
    }
    amyplanyy_add_double(amyplanyy, get_abce(amyplanyy)->dynscope.u.area->u.sc.parent->u.sc.locidx);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  }
  $$ = 0;
}
| LPO
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    if (get_abce(amyplanyy)->dynscope.u.area->u.sc.parent == NULL)
    {
      printf("No parent scope, can't use lexical parent reference\n");
      YYABORT;
    }
    amyplanyy_add_double(amyplanyy, get_abce(amyplanyy)->dynscope.u.area->u.sc.parent->u.sc.locidx);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  }
  $$ = 1;
}
| L
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, get_abce(amyplanyy)->dynscope.u.area->u.sc.locidx);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  }
  $$ = 0;
}
| LO
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, get_abce(amyplanyy)->dynscope.u.area->u.sc.locidx);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  }
  $$ = 1;
}
| DP
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SCOPE_PARENT);
  }
  $$ = 0;
}
| D
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN);
  }
  $$ = 0;
}
| DO
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN);
  }
  $$ = 1;
}
| DPO
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SCOPE_PARENT);
  }
  $$ = 1;
}
| SC OPEN_PAREN expr CLOSE_PAREN
{
  $$ = 0;
}
| SCO OPEN_PAREN expr CLOSE_PAREN
{
  $$ = 1;
}
;

expr: expr11;

expr1:
  expr0_or_string
| LOGICAL_NOT expr1
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LOGICAL_NOT);
  }
}
| BITWISE_NOT expr1
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_BITWISE_NOT);
  }
}
| ADD expr1
| SUB expr1
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_UNARY_MINUS);
  }
}
;

expr2:
  expr1
| expr2 MUL expr1
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_MUL);
  }
}
| expr2 DIV expr1
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DIV);
  }
}
| expr2 MOD expr1
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_MOD);
  }
}
;

expr3:
  expr2
| expr3 ADD expr2
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ADD);
  }
}
| expr3 SUB expr2
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SUB);
  }
}
| expr3 PERIOD expr2
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRAPPEND);
  }
}
;

expr4:
  expr3
| expr4 SHL expr3
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SHL);
  }
}
| expr4 SHR expr3
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SHR);
  }
}
;

expr5:
  expr4
| expr5 LT expr4
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LT);
  }
}
| expr5 LE expr4
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LE);
  }
}
| expr5 GT expr4
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GT);
  }
}
| expr5 GE expr4
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GE);
  }
}
;

expr6:
  expr5
| expr6 EQ expr5
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EQ);
  }
}
| expr6 NE expr5
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_NE);
  }
}
;

expr7:
  expr6
| expr7 BITWISE_AND expr6
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_BITWISE_AND);
  }
}
;

expr8:
  expr7
| expr8 BITWISE_XOR expr7
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_BITWISE_XOR);
  }
}
;

expr9:
  expr8
| expr9 BITWISE_OR expr8
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_BITWISE_OR);
  }
}
;

expr10:
  expr9
| expr10 LOGICAL_AND
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TOP);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    $<d>$ = get_abce(amyplanyy)->bytecodesz;
    amyplanyy_add_double(amyplanyy, 0);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_IF_NOT_JMP);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
  }
}
  expr9
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_set_double(amyplanyy, $<d>3, get_abce(amyplanyy)->bytecodesz);
  }
}
;

expr11:
  expr10
| expr11 LOGICAL_OR
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TOP);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LOGICAL_NOT);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    $<d>$ = get_abce(amyplanyy)->bytecodesz;
    amyplanyy_add_double(amyplanyy, 0);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_IF_NOT_JMP);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
  }
}
  expr10
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_set_double(amyplanyy, $<d>3, get_abce(amyplanyy)->bytecodesz);
  }
}
;

expr0_or_string:
  expr0_without_string
| STRING_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    int64_t idx = abce_cache_add_str(get_abce(amyplanyy), $1.str, $1.sz);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, idx);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  }
  free($1.str);
}
;

expr0_without_string:
  OPEN_PAREN expr CLOSE_PAREN
| OPEN_PAREN expr CLOSE_PAREN OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, $5);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL);
  }
}
| OPEN_PAREN expr CLOSE_PAREN MAYBE_CALL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL_IF_FUN);
  }
}
| dict maybe_bracketexprlist
| list maybe_bracketexprlist
| NUMBER
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, $1);
  }
}
| TRUE { if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_TRUE); }
| TYPE OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TYPE); }
| FALSE { if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FALSE); }
| NIL { if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NIL); }
| EIGHT
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, 0);
  }
}
| SIXTEENBE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, 1);
  }
}
| THIRTYTWOBE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, 2);
  }
}
| SIXTEENLE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, -1);
  }
}
| THIRTYTWOLE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, -2);
  }
}
| STR_FROMCHR OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STR_FROMCHR); }
| STR_LOWER OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STR_LOWER); }
| STR_UPPER OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STR_UPPER); }
| STR_REVERSE OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STR_REVERSE); }
| STRCMP OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STR_CMP); }
| STRSTR OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRSTR); }
| STRREP OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRREP); }
| DICTPREV OPEN_PAREN expr COMMA expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_TRUE);
    // Stack: dict | key | bool
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DICTNEXT_SAFE);
    // Stack: dict | nextkey | nextval
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
    // Stack: dict | nextkey
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, -2);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXCHANGE_TOP);
    // Stack: nextkey | dict
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
    // Stack: nextkey
  }
}
| DICTNEXT OPEN_PAREN expr COMMA expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FALSE);
    // Stack: dict | key | bool
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DICTNEXT_SAFE);
    // Stack: dict | nextkey | nextval
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
    // Stack: dict | nextkey
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, -2);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXCHANGE_TOP);
    // Stack: nextkey | dict
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
    // Stack: nextkey
  }
}
| STRLISTJOIN OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRLISTJOIN); }
| STRAPPEND OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRAPPEND); }
| STRSTRIP OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRSTRIP); }
| FOPEN OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FILE_OPEN); }
| FCLOSE OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FILE_CLOSE); }
| FFLUSH OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FILE_FLUSH); }
| FREAD OPEN_PAREN expr COMMA expr COMMA expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FILE_GET); }
| FSEEK OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FILE_SEEK_TELL); }
| FWRITE OPEN_PAREN expr COMMA expr COMMA expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FILE_WRITE); }
| SPLICE OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LISTSPLICE); }
| STRSUB OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRSUB); }
| STRGSUB OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRGSUB); }
| STRSET OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRSET); }
| STRWORD OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRWORD); }
| STRWORDCNT OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRWORDCNT); }
| STRWORDLIST OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRWORDLIST); }
| ABS OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ABS); }
| ACOS OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ACOS); }
| ASIN OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ASIN); }
| ATAN OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ATAN); }
| ROUND OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ROUND); }
| TRUNC OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TRUNC); }
| FLOOR OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FLOOR); }
| CEIL OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CEIL); }
| COS OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_COS); }
| SIN OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SIN); }
| TAN OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TAN); }
| EXP OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXP); }
| LOG OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LOG); }
| SQRT OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SQRT); }
| GETENV OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETENV); }
| CHOMP OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CHOMP); }
| STR2PB OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STR2PB); }
| PB2STR OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PB2STR); }
| DUP_NONRECURSIVE OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DUP_NONRECURSIVE); }
| PB_NEW OPEN_PAREN CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NEW_PB); }
| TOSTRING OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TOSTRING); }
| TONUMBER OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TONUMBER); }
| SCOPE_PARENT OPEN_PAREN expr CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SCOPE_PARENT); }
| SCOPE_NEW OPEN_PAREN expr COMMA expr CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SCOPE_NEW);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  }
}
| GETSCOPE_DYN OPEN_PAREN CLOSE_PAREN
{ if (amyplanyy_do_emit(amyplanyy)) amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN); }
| GETSCOPE_LEX OPEN_PAREN CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, get_abce(amyplanyy)->dynscope.u.area->u.sc.locidx);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  }
}
| lvalue
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    add_corresponding_get(amyplanyy, $1);
  }
}
| lvalue
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    add_corresponding_get(amyplanyy, $1);
  }
}
  OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, $4);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL);
  }
}
| DYNLUACALL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN);
  }
}
  OPEN_PAREN expr COMMA maybe_arglist CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, $6);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LUACALL);
  }
}
| LEXLUACALL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, get_abce(amyplanyy)->dynscope.u.area->u.sc.locidx);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  }
}
  OPEN_PAREN expr COMMA maybe_arglist CLOSE_PAREN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, $6);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LUACALL);
  }
}
| lvalue MAYBE_CALL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    add_corresponding_get(amyplanyy, $1);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL_IF_FUN);
  }
}
| IMM OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| IMM OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| IMM OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist MAYBE_CALL
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| DYNO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| DYNO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| DYNO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist MAYBE_CALL
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| LEXO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| LEXO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| LEXO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist MAYBE_CALL
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| IMMO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| IMMO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| IMMO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist MAYBE_CALL
{
  fprintf(stderr, "unsupported syntax\n");
  YYABORT;
}
| LOC OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET maybe_bracketexprlist
{
  fprintf(stderr, "unsupported syntax\n");
  free($3.str);
  YYABORT;
}
| LOC OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET maybe_bracketexprlist OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  fprintf(stderr, "unsupported syntax\n");
  free($3.str);
  YYABORT;
}
| LOC OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET maybe_bracketexprlist MAYBE_CALL
{
  fprintf(stderr, "unsupported syntax\n");
  free($3.str);
  YYABORT;
}
| custom_expr0
;

maybe_arglist:
{
  $$ = 0;
}
| arglist
{
  $$ = $1;
}
;

arglist:
expr
{
  $$ = 1;
}
| arglist COMMA expr
{
  $$ = $1 + 1;
}
;

list:
OPEN_BRACKET
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NEW_ARRAY);
  }
}
maybe_valuelist CLOSE_BRACKET
;

dict:
OPEN_BRACE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NEW_DICT);
  }
}
maybe_dictlist CLOSE_BRACE
;

maybe_dictlist:
| dictlist
;

dictlist:
  dictentry
| dictlist COMMA dictentry
;

dictentry:
  value COLON
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (is_autocall(amyplanyy))
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL_IF_FUN);
    }
  }
}
  value
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (is_autocall(amyplanyy))
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL_IF_FUN);
    }
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DICTSET_MAINTAIN);
  }
}
;

maybe_valuelist:
| valuelist
;

valuelist:
  valuelistentry
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (is_autocall(amyplanyy))
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL_IF_FUN);
    }
    if ($1)
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPENDALL_MAINTAIN);
    }
    else
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPEND_MAINTAIN);
    }
  }
}
| valuelist COMMA valuelistentry
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (is_autocall(amyplanyy))
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL_IF_FUN);
    }
    if ($3)
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPENDALL_MAINTAIN);
    }
    else
    {
      amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPEND_MAINTAIN);
    }
  }
}
;

valuelistentry:
  value
{
  $$ = $1;
};

/* End of copypaste */

stirrule:
  targetspec COLON depspec
{
  stiryyset_extra(stiryyget_extra(scanner) | 1, scanner);
}
  NEWLINE shell_commands
{
  stiryyset_extra(stiryyget_extra(scanner) & (~1), scanner);
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (stiryy_check_rule(stiryy) != 0)
    {
      char buf[2048] = {0};
      snprintf(buf, sizeof(buf), "Recommend setting rule for %s to @rectgtrule, @detouchrule or @mayberule",
               stiryy->main->rules[stiryy->main->rulesz - 1].targets[0].name);
      recommend(scanner, stiryy, buf);
    }
    stiryy->main->rule_in_progress = 0;
  }
}
| CLEANHOOK COLON
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    //printf("target1 %s\n", $1);
    stiryy_emplace_rule(stiryy, get_abce(stiryy)->dynscope.u.area->u.sc.locidx);
    stiryy_set_cleanhooktgt(stiryy, "CLEAN");
  }
}
  depspec
{
  stiryyset_extra(stiryyget_extra(scanner) | 1, scanner);
}
  NEWLINE shell_commands
{
  stiryyset_extra(stiryyget_extra(scanner) & (~1), scanner);
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy->main->rule_in_progress = 0;
  }
}
| DISTCLEANHOOK COLON
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    //printf("target1 %s\n", $1);
    stiryy_emplace_rule(stiryy, get_abce(stiryy)->dynscope.u.area->u.sc.locidx);
    stiryy_set_cleanhooktgt(stiryy, "DISTCLEAN");
  }
}
  depspec
{
  stiryyset_extra(stiryyget_extra(scanner) | 1, scanner);
}
  NEWLINE shell_commands
{
  stiryyset_extra(stiryyget_extra(scanner) & (~1), scanner);
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy->main->rule_in_progress = 0;
  }
}
| BOTHCLEANHOOK COLON
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    //printf("target1 %s\n", $1);
    stiryy_emplace_rule(stiryy, get_abce(stiryy)->dynscope.u.area->u.sc.locidx);
    stiryy_set_cleanhooktgt(stiryy, "BOTHCLEAN");
  }
}
  depspec
{
  stiryyset_extra(stiryyget_extra(scanner) | 1, scanner);
}
  NEWLINE shell_commands
{
  stiryyset_extra(stiryyget_extra(scanner) & (~1), scanner);
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy->main->rule_in_progress = 0;
  }
}
| RECTGTRULE maybe_distrule COLON targetspec COLON depspec
{
  stiryyset_extra(stiryyget_extra(scanner) | 1, scanner);
}
  NEWLINE shell_commands
{
  stiryyset_extra(stiryyget_extra(scanner) & (~1), scanner);
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_mark_rectgt(stiryy);
    if ($2)
    {
      stiryy_mark_dist(stiryy);
    }
    stiryy->main->rule_in_progress = 0;
  }
}
| DETOUCHRULE maybe_distrule COLON targetspec COLON depspec
{
  stiryyset_extra(stiryyget_extra(scanner) | 1, scanner);
}
  NEWLINE shell_commands
{
  stiryyset_extra(stiryyget_extra(scanner) & (~1), scanner);
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_mark_detouch(stiryy);
    if ($2)
    {
      stiryy_mark_dist(stiryy);
    }
    stiryy->main->rule_in_progress = 0;
  }
}
| PHONYRULE COLON targetspec COLON depspec
{
  stiryyset_extra(stiryyget_extra(scanner) | 1, scanner);
}
  NEWLINE shell_commands
{
  stiryyset_extra(stiryyget_extra(scanner) & (~1), scanner);
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_mark_phony(stiryy);
    stiryy->main->rule_in_progress = 0;
  }
}
| MAYBERULE COLON targetspec COLON depspec
{
  stiryyset_extra(stiryyget_extra(scanner) | 1, scanner);
}
  NEWLINE shell_commands
{
  stiryyset_extra(stiryyget_extra(scanner) & (~1), scanner);
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_mark_maybe(stiryy);
    stiryy->main->rule_in_progress = 0;
  }
}
| DISTRULE COLON targetspec COLON depspec
{
  stiryyset_extra(stiryyget_extra(scanner) | 1, scanner);
}
  NEWLINE shell_commands
{
  stiryyset_extra(stiryyget_extra(scanner) & (~1), scanner);
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_mark_dist(stiryy);
    if (stiryy_check_rule(stiryy) != 0)
    {
      char buf[2048] = {0};
      snprintf(buf, sizeof(buf), "Recommend setting rule for %s to @rectgtrule, @detouchrule or @mayberule",
               stiryy->main->rules[stiryy->main->rulesz - 1].targets[0].name);
      recommend(scanner, stiryy, buf);
    }
    stiryy->main->rule_in_progress = 0;
  }
}
| DEPONLY COLON targetspec COLON depspec NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_mark_deponly(stiryy);
    stiryy->main->rule_in_progress = 0;
  }
}
| PATRULE maybe_distrule COLON
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_emplace_patrule(stiryy, get_abce(stiryy)->dynscope.u.area->u.sc.locidx);
  }
}
  pattargetspec COLON
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_freeze_patrule(stiryy);
  }
}
  pattargetspec COLON patdepspec
{
  stiryyset_extra(stiryyget_extra(scanner) | 1, scanner);
}
  NEWLINE shell_commands
{
  stiryyset_extra(stiryyget_extra(scanner) & (~1), scanner);
  if (amyplanyy_do_emit(amyplanyy))
  {
    if ($2)
    {
      stiryy_mark_dist(stiryy);
    }
    stiryy->main->rule_in_progress = 0;
  }
}
;

shell_commands:
| shell_commands shell_command
;

shell_command:
  SHELL_COMMAND NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    char *outbuf = NULL;
    size_t cidx;
    size_t outcap = 0;
    size_t outsz = 0;
    size_t len = strlen($1);
    size_t i;
    size_t codeloc = amyplanyy->main->abce->bytecodesz;
    size_t shidx;
    size_t dashcidx;
    size_t emptyidx;
    int ismake = 0;
    int noecho = 0;
    int ignore = 0;
    shidx = abce_cache_add_str(amyplanyy->main->abce, "sh", 2);
    dashcidx = abce_cache_add_str(amyplanyy->main->abce, "-c", 2);
    abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_NEW_ARRAY);
    abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_DBL);
    abce_add_double(amyplanyy->main->abce, shidx);
    abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_FROM_CACHE);
    abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_APPEND_MAINTAIN);
    abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_DBL);
    abce_add_double(amyplanyy->main->abce, dashcidx);
    abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_FROM_CACHE);
    abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_APPEND_MAINTAIN);
    emptyidx = abce_cache_add_str(amyplanyy->main->abce, "", 0);
    abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_DBL);
    abce_add_double(amyplanyy->main->abce, emptyidx);
    abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_FROM_CACHE);
    i = 0;
    if ($1[i] == '-')
    {
      ignore = 1;
      i++;
    }
    else if ($1[i] == '+')
    {
      ismake = 1;
      i++;
    }
    else if ($1[i] == '@')
    {
      noecho = 1;
      i++;
    }
    for (; i < len; i++)
    {
#if 1
      if ($1[i] == '\\')
      {
        if (i+1 >= len)
        {
          my_abort();
        }
        if (outsz >= outcap)
        {
          outcap = 2*outcap + 16;
          outbuf = realloc(outbuf, outcap);
        }
        outbuf[outsz++] = $1[i+1];
        i++;
        continue;
      }
#endif
      if ($1[i] == '$')
      {
        if (outsz)
        {
          size_t cidx;
          /*
          if (i > 0 && $1[i] != ' ')
          {
            recommend(scanner, stiryy, "Recommend putting space before variable name, because new argument is created");
          }
          */
          cidx = abce_cache_add_str(amyplanyy->main->abce, outbuf, outsz);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_DBL);
          abce_add_double(amyplanyy->main->abce, cidx);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_FROM_CACHE);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_STRAPPEND);
          outsz = 0;
        }
        if (i+1 < len && ($1[i+1] == '<' || $1[i+1] == '@'))
        {
          size_t cidx;
          cidx = abce_cache_add_str(amyplanyy->main->abce, &$1[i+1], 1);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_GETSCOPE_DYN);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_DBL);
          abce_add_double(amyplanyy->main->abce, cidx);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_FROM_CACHE);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_SCOPEVAR);
          abce_add_ins(amyplanyy->main->abce, STIR_OPCODE_SHELL_ESCAPE);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_STRAPPEND);
          i++;
          continue;
        }
        if (i+1 < len && ($1[i+1] == '^' || $1[i+1] == '+' || $1[i+1] == '|'))
        {
          size_t cidx;
          cidx = abce_cache_add_str(amyplanyy->main->abce, " ", 1);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_DBL);
          abce_add_double(amyplanyy->main->abce, cidx);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_FROM_CACHE);
          cidx = abce_cache_add_str(amyplanyy->main->abce, &$1[i+1], 1);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_GETSCOPE_DYN);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_DBL);
          abce_add_double(amyplanyy->main->abce, cidx);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_FROM_CACHE);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_SCOPEVAR);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_STRLISTJOIN);
          abce_add_ins(amyplanyy->main->abce, STIR_OPCODE_SHELL_ESCAPE);
          abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_STRAPPEND);
          i++;
          continue;
        }
        if (i+1 < len && $1[i+1] == '(')
        {
          size_t end;
          size_t cidxvar, cidx;
          int isarray = 0;
          if (i+2 < len && $1[i+2] == '@')
          {
            isarray = 1;
          }
          for (end = i+1; end < len && $1[end] != ')'; end++)
          {
            // nop
          }
          if (end == len)
          {
            stiryyerror(scanner, stiryy, "Old-style shell command varref literal not terminated");
            YYABORT;
          }
          cidxvar = abce_cache_add_str(amyplanyy->main->abce, $1 + i + 2 + isarray, end - (i+2+isarray));
          if (isarray)
          {
            cidx = abce_cache_add_str(amyplanyy->main->abce, " ", 1);
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_DBL);
            abce_add_double(amyplanyy->main->abce, cidx);
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_FROM_CACHE);
            cidx = cidxvar;
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_GETSCOPE_DYN);
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_DBL);
            abce_add_double(amyplanyy->main->abce, cidx);
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_FROM_CACHE);
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_SCOPEVAR);
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_STRLISTJOIN);
            abce_add_ins(amyplanyy->main->abce, STIR_OPCODE_SHELL_ESCAPE);
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_STRAPPEND);
          }
          else
          {
            cidx = cidxvar;
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_GETSCOPE_DYN);
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_DBL);
            abce_add_double(amyplanyy->main->abce, cidx);
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_FROM_CACHE);
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_SCOPEVAR);
            abce_add_ins(amyplanyy->main->abce, STIR_OPCODE_SHELL_ESCAPE);
            abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_STRAPPEND);
          }
          i = end;
          continue;
        }
        stiryyerror(scanner, stiryy, "Old-style shell commands support only $@, $<, $^, $+, $|, $(VARNAME) and $(@VARNAME) as variables");
        YYABORT;
      }
      if (outsz >= outcap)
      {
        outcap = 2*outcap + 16;
        outbuf = realloc(outbuf, outcap);
      }
      outbuf[outsz++] = $1[i];
    }
    if (outsz)
    {
      cidx = abce_cache_add_str(amyplanyy->main->abce, outbuf, outsz);
      abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_DBL);
      abce_add_double(amyplanyy->main->abce, cidx);
      abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_PUSH_FROM_CACHE);
      abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_STRAPPEND);
      outsz = 0;
    }

    abce_add_ins(amyplanyy->main->abce, ABCE_OPCODE_APPEND_MAINTAIN);

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);
    stiryy_add_shell_attab(stiryy, codeloc, ignore, noecho, ismake);
  }

  free($1);
}
| ATTAB rulespecifiers
{
  $<d>$ = get_abce(amyplanyy)->bytecodesz;
}
  expr NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);
    stiryy_add_shell_attab(stiryy, $<d>3, ((int)$2)&1, ((int)$2)&2, ((int)$2)&4);
  }
}
| ATATTAB rulespecifiers
{
  $<d>$ = get_abce(amyplanyy)->bytecodesz;
}
  expr NEWLINE
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);
    stiryy_add_shell_atattab(stiryy, $<d>3, ((int)$2)&1, ((int)$2)&2, ((int)$2)&4);
  }
}
;

rulespecifiers:
{
  $$ = 0;
}
| rulespecifiers IGNORE
{
  $$ = ((int)$1) | 1;
}
| rulespecifiers NOECHO
{
  $$ = ((int)$1) | 2;
}
| rulespecifiers ISMAKE
{
  $$ = ((int)$1) | 4;
}
;

pattargetspec:
  pattargets
;

patdepspec:
  patdeps
;

targetspec:
  targets
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (stiryy->main->rules[stiryy->main->rulesz - 1].targetsz == 0)
    {
      printf("empty target list\n");
      YYABORT;
    }
  }
}
;
  
depspec:
  deps
;

pattargets:
  maybe_disttgt FREEFORM_TOKEN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if ($1 && !stiryy->main->rules[stiryy->main->rulesz-1].patfrozen)
    {
      stiryyerror(scanner, stiryy, "pattern rules can't contain @disttgt in bases");
      YYABORT;
    }
    if (!stiryy->main->freeform_token_seen)
    {
      recommend(scanner, stiryy, "Recommend using string literals instead of free-form tokens; recommend also using @strict mode");
      stiryy->main->freeform_token_seen=1;
    }
    stiryy_set_pattgt(amyplanyy, $2, $1);
  }
}
| maybe_disttgt tgtdepref
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    int ret;
    char **strs;
    size_t i, strsz;

    if ($1 && !stiryy->main->rules[stiryy->main->rulesz-1].patfrozen)
    {
      stiryyerror(scanner, stiryy, "pattern rules can't contain @disttgt in bases");
      YYABORT;
    }

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);

    ret = engine_stringlist(get_abce(amyplanyy), $2, "target", &strs, &strsz);

    if (ret)
    {
      stiryyerror(scanner, stiryy, "error in targets");
      YYABORT;
    }

    for (i = 0; i < strsz; i++)
    {
      stiryy_set_pattgt(stiryy, strs[i], $1);
      free(strs[i]);
    }
    free(strs);
  }
}
| pattargets maybe_disttgt FREEFORM_TOKEN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if ($2 && !stiryy->main->rules[stiryy->main->rulesz-1].patfrozen)
    {
      stiryyerror(scanner, stiryy, "pattern rules can't contain @disttgt in bases");
      YYABORT;
    }
    if (!stiryy->main->freeform_token_seen)
    {
      recommend(scanner, stiryy, "Recommend using string literals instead of free-form tokens; recommend also using @strict mode");
      stiryy->main->freeform_token_seen=1;
    }
    stiryy_set_pattgt(amyplanyy, $3, $2);
  }
}
| pattargets maybe_disttgt tgtdepref
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    int ret;
    char **strs;
    size_t i, strsz;

    if ($2 && !stiryy->main->rules[stiryy->main->rulesz-1].patfrozen)
    {
      stiryyerror(scanner, stiryy, "pattern rules can't contain @disttgt in bases");
      YYABORT;
    }

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);

    ret = engine_stringlist(get_abce(amyplanyy), $3, "target", &strs, &strsz);

    if (ret)
    {
      stiryyerror(scanner, stiryy, "error in targets");
      YYABORT;
    }

    for (i = 0; i < strsz; i++)
    {
      stiryy_set_pattgt(stiryy, strs[i], $2);
      free(strs[i]);
    }
    free(strs);
  }
}
;

tgtdepref:
  VARREF_LITERAL
{
  $$ = get_abce(amyplanyy)->bytecodesz;
  if (amyplanyy_do_emit(amyplanyy))
  {
    // Outside of function, search for dynamic symbol
    // Doesn't really happen with standard syntax, but somebody may embed it
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN);

    int64_t idx = abce_cache_add_str(get_abce(amyplanyy), $1, strlen($1));
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, idx);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SCOPEVAR);
  }
  free($1);
}
| STRING_LITERAL
{
  $$ = get_abce(amyplanyy)->bytecodesz;
  if (amyplanyy_do_emit(amyplanyy))
  {
    int64_t idx = abce_cache_add_str(get_abce(amyplanyy), $1.str, $1.sz);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
    amyplanyy_add_double(amyplanyy, idx);
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  }
  free($1.str);
}
| OPEN_PAREN
{
  $$ = get_abce(amyplanyy)->bytecodesz;
}
  expr CLOSE_PAREN
{
  $$ = $<d>2;
}
;

targets:
  FREEFORM_TOKEN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_emplace_rule(stiryy, get_abce(stiryy)->dynscope.u.area->u.sc.locidx);
    handle_tgt_freeform_token(scanner, stiryy, $1, 0);
  }
  free($1);
}
| DISTTGT FREEFORM_TOKEN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_emplace_rule(stiryy, get_abce(stiryy)->dynscope.u.area->u.sc.locidx);
    handle_tgt_freeform_token(scanner, stiryy, $2, 1);
  }
  free($2);
}
| tgtdepref
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    stiryy_emplace_rule(stiryy, get_abce(stiryy)->dynscope.u.area->u.sc.locidx);
    if (handle_tgt_tgtdepref(scanner, stiryy, $1, 0))
    {
      YYABORT;
    }
  }
}
| DISTTGT tgtdepref
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (handle_tgt_tgtdepref(scanner, stiryy, $2, 1))
    {
      YYABORT;
    }
  }
}
| targets maybe_disttgt FREEFORM_TOKEN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    handle_tgt_freeform_token(scanner, stiryy, $3, $2);
  }
  free($3);
}
| targets maybe_disttgt tgtdepref
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (handle_tgt_tgtdepref(scanner, stiryy, $3, $2))
    {
      YYABORT;
    }
  }
}
;

maybe_distrule:
{
  $$ = 0;
}
| DISTRULE
{
  $$ = 1;
}
;

maybe_disttgt:
{
  $$ = 0;
}
| DISTTGT
{
  $$ = 1;
}
;

maybe_rec:
{
  $$ = 0;
}
| maybe_rec RECDEP
{
  $$ = ((int)$$) | 1;
}
| maybe_rec ORDERONLY
{
  $$ = ((int)$$) | 2;
}
| maybe_rec WAIT
{
  $$ = ((int)$$) | 4;
}
;

patdeps:
| patdeps maybe_rec FREEFORM_TOKEN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (!stiryy->main->freeform_token_seen)
    {
      recommend(scanner, stiryy, "Recommend using string literals instead of free-form tokens; recommend also using @strict mode");
      stiryy->main->freeform_token_seen=1;
    }
    //printf("dep %s rec? %d\n", $3, (int)$2);
    stiryy_set_patdep(stiryy, $3, ((int)$2) & 1, ((int)$2) & 2, ((int)$2) & 4);
  }
  free($3);
}
| patdeps maybe_rec tgtdepref
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t strsz;
    size_t i;
    int ret;
    char **strs;

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);

    ret = engine_stringlist(get_abce(amyplanyy), $3, "dependency", &strs, &strsz);
    if (ret)
    {
      stiryyerror(scanner, stiryy, "error in dependencies");
      YYABORT;
    }

    for (i = 0; i < strsz; i++)
    {
      stiryy_set_patdep(stiryy, strs[i], ((int)$2) & 1, ((int)$2) & 2, ((int)$2) & 4);
      free(strs[i]);
    }
    free(strs);
  }
}
;

deps:
| deps maybe_rec FREEFORM_TOKEN
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    if (!stiryy->main->freeform_token_seen)
    {
      recommend(scanner, stiryy, "Recommend using string literals instead of free-form tokens; recommend also using @strict mode");
      stiryy->main->freeform_token_seen=1;
    }
    //printf("dep %s rec? %d\n", $3, (int)$2);
    stiryy_set_dep(stiryy, $3, ((int)$2) & 1, ((int)$2) & 2, ((int)$2) & 4);
  }
  free($3);
}
/*
| deps maybe_rec STRING_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    //printf("dep %s rec? %d\n", $3.str, (int)$2);
    stiryy_set_dep(stiryy, $3.str, ((int)$2) & 1, ((int)$2) & 2, ((int)$2) & 4);
  }
  free($3.str);
}
| deps maybe_rec VARREF_LITERAL
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    printf("depref\n");
  }
  free($3);
}
*/
| deps maybe_rec tgtdepref
{
  if (amyplanyy_do_emit(amyplanyy))
  {
    size_t strsz;
    size_t i;
    int ret;
    char **strs;

    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT);

    ret = engine_stringlist(get_abce(amyplanyy), $3, "dependency", &strs, &strsz);
    if (ret)
    {
      stiryyerror(scanner, stiryy, "error in dependencies");
      YYABORT;
    }

    for (i = 0; i < strsz; i++)
    {
      stiryy_set_dep(stiryy, strs[i], ((int)$2) & 1, ((int)$2) & 2, ((int)$2) & 4);
      free(strs[i]);
    }
    free(strs);
  }
}
;

dirinclude: DIRINCLUDE {$$ = 1;} | PROJDIRINCLUDE {$$ = 0;} ;

cdepincludes:
  CDEPINCLUDES
  cdepspecifiers
{
  $$ = $2;
}
;

cdepspecifiers:
{
  $$ = 0;
}
| cdepspecifiers cdepspecifier
{
  $$ = ((unsigned)$1) | ((unsigned)$2);
}
;

cdepspecifier: AUTOPHONY {$$ = 1;} | AUTOTARGET {$$ = 2;} | IGNORE {$$ = 4;};
