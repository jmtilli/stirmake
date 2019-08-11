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
#include "opcodesonly.h"
#if 0
#include "abce/stiryy.h"
#include "abce/stiryyutils.h"
#endif
#include "abce/abce.h"
#include "abce/amyplan.h"
#include "abce/abceopcodes.h"
#include "abce/amyplanlocvarctx.h"
#include <arpa/inet.h>

void stiryyerror(/*YYLTYPE *yylloc,*/ yyscan_t scanner, struct stiryy *stiryy, const char *str)
{
        //fprintf(stderr, "error: %s at line %d col %d\n",str, yylloc->first_line, yylloc->first_column);
        // FIXME we need better location info!
        fprintf(stderr, "stir error: %s at line %d col %d\n", str, stiryyget_lineno(scanner), stiryyget_column(scanner));
}

int stiryywrap(yyscan_t scanner)
{
        return 1;
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

#define amyplanyy stiryy
#define amyplanyy_add_byte stiryy_add_byte
#define amyplanyy_add_double stiryy_add_double
#define amyplanyy_set_double stiryy_set_double
#define amyplanyy_add_fun_sym stiryy_add_fun_sym
#define amyplan_symbol_add stiryy_symbol_add

#define get_abce(stiryy) ((stiryy)->main->abce)

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

%token <s> SHELL_COMMAND
%token ATTAB

%token NEWLINE

%token ONCE ENDONCE STDOUT STDERR ERROR DUMP EXIT
%token EQUALS
%token PLUSEQUALS
%token QMEQUALS
%token COLONEQUALS
%token PLUSCOLONEQUALS
%token QMCOLONEQUALS
%token COLON
%token COMMA
%token <str> STRING_LITERAL
%token <d> NUMBER
%token <s> VARREF_LITERAL
%token <s> FREEFORM_TOKEN
%token MAYBE_CALL
%token LT
%token GT
%token LE
%token GE
%token AT
%token FUNCTION
%token ENDFUNCTION
%token LOCVAR
%token RECDEP

%token DELAYVAR
%token DELAYEXPR
%token DELAYLISTEXPAND
%token SUFFILTER
%token SUFSUBONE
%token STRAPPEND
%token SUFSUB
%token PHONYRULE
%token DISTRULE
%token PATRULE
%token FILEINCLUDE
%token DIRINCLUDE
%token CDEPINCLUDESCURDIR
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
%token LOC
%token APPEND
%token APPEND_LIST
%token RETURN
%token ADD_RULE
%token RULE_DIST
%token RULE_PHONY
%token RULE_ORDINARY
%token PRINT

%token ATQM SCOPE TRUE TYPE FALSE NIL STR_FROMCHR STR_LOWER STR_UPPER
%token STR_REVERSE STRCMP STRSTR STRREP STRLISTJOIN STRSTRIP STRSUB
%token STRGSUB STRSET STRWORD STRWORDCNT STRWORDLIST ABS ACOS ASIN ATAN
%token CEIL COS SIN TAN EXP LOG SQRT DUP_NONRECURSIVE PB_NEW TOSTRING
%token TONUMBER SCOPE_PARENT SCOPE_NEW GETSCOPE_DYN GETSCOPE_LEX



%token IF
%token ELSE
%token ENDIF
%token WHILE
%token ENDWHILE
%token BREAK
%token CONTINUE

%token DIV MUL ADD SUB SHL SHR NE EQ LOGICAL_AND LOGICAL_OR LOGICAL_NOT MOD BITWISE_AND BITWISE_OR BITWISE_NOT BITWISE_XOR


%token ERROR_TOK

%type<d> value
%type<d> lvalue
%type<d> arglist
%type<d> valuelistentry
%type<d> maybe_arglist
%type<d> maybe_atqm
%type<d> dynstart
%type<d> scopstart
%type<d> lexstart
%type<d> varref_tail
%type<d> varref

%type<d> maybeqmequals
%type<d> maybe_rec

%start st

%%

st: stirrules;

maybeqmequals: EQUALS {$$ = 0;} | QMEQUALS {$$ = 1;} ;

assignrule:
VARREF_LITERAL QMCOLONEQUALS expr NEWLINE
{
  free($1);
  printf("not implemented yet\n");
  YYABORT;
}
| VARREF_LITERAL COLONEQUALS expr NEWLINE
{
  free($1);
  printf("not implemented yet\n");
  YYABORT;
}
| VARREF_LITERAL PLUSCOLONEQUALS expr NEWLINE
{
  free($1);
  printf("not implemented yet\n");
  YYABORT;
}
| VARREF_LITERAL maybeqmequals
{
  size_t funloc = stiryy->main->abce->bytecodesz;
  stiryy_add_byte(stiryy, ABCE_OPCODE_FUN_HEADER);
  stiryy_add_double(stiryy, 0);
  $<d>$ = funloc;
}
expr NEWLINE
{
  printf("Assigning to %s\n", $1);
  stiryy_add_byte(stiryy, ABCE_OPCODE_RET);
  stiryy_add_byte(stiryy, ABCE_OPCODE_FUN_TRAILER);
  stiryy_add_double(stiryy, stiryy_symbol_add(stiryy, $1, strlen($1)));
  stiryy_add_fun_sym(stiryy, $1, $2, $<d>3);
  free($1);
}
| VARREF_LITERAL PLUSEQUALS
{
  size_t funloc = stiryy->main->abce->bytecodesz;
  size_t oldloc = stiryy_add_fun_sym(stiryy, $1, 0, funloc); // FIXME move later
  if (oldloc == (size_t)-1)
  {
    printf("Can't find old symbol function\n");
    YYABORT;
  }
  stiryy_add_byte(stiryy, ABCE_OPCODE_FUN_HEADER);
  stiryy_add_double(stiryy, 0);
  stiryy_add_byte(stiryy, ABCE_OPCODE_PUSH_DBL);
  stiryy_add_double(stiryy, oldloc);
  stiryy_add_byte(stiryy, ABCE_OPCODE_PUSH_FROM_CACHE);
  stiryy_add_byte(stiryy, ABCE_OPCODE_CALL_IF_FUN);
  // FIXME what if it's not a list?
  stiryy_add_byte(stiryy, ABCE_OPCODE_DUP_NONRECURSIVE);
/*
  stiryy_add_byte(stiryy, STIRBCE_OPCODE_FUNIFY);
  stiryy_add_byte(stiryy, STIRBCE_OPCODE_PUSH_DBL);
  stiryy_add_double(stiryy, 0); // arg cnt
  stiryy_add_byte(stiryy, STIRBCE_OPCODE_CALL);
*/
}
expr NEWLINE
{
  printf("Plus-assigning to %s\n", $1);
  // FIXME what if it's not a list?
  stiryy_add_byte(stiryy, ABCE_OPCODE_APPENDALL_MAINTAIN);
  stiryy_add_byte(stiryy, ABCE_OPCODE_RET);
  stiryy_add_byte(stiryy, ABCE_OPCODE_FUN_TRAILER);
  stiryy_add_double(stiryy, stiryy_symbol_add(stiryy, $1, strlen($1)));
  free($1);
}
;


stirrule:
  targetspec COLON depspec NEWLINE shell_commands
| PHONYRULE COLON targetspec COLON depspec NEWLINE shell_commands
| DISTRULE COLON targetspec COLON depspec NEWLINE shell_commands
| PATRULE COLON targetspec COLON targetspec COLON depspec NEWLINE shell_commands
;

shell_commands:
| shell_commands shell_command
;

shell_command:
  SHELL_COMMAND NEWLINE
{
  char *outbuf = NULL;
  size_t outcap = 0;
  size_t outsz = 0;
  size_t len = strlen($1);
  size_t i;
  stiryy_add_shell_section(stiryy);
  for (i = 0; i < len; i++)
  {
    if ($1[i] == '\\')
    {
      if (i+1 >= len)
      {
        abort();
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
    else if ($1[i] == '$')
    {
      if (i+1 < len && $1[i+1] == '<')
      {
        char *dep;
        size_t deplen;
        struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
        if (rule->depsz <= 0)
        {
          fprintf(stderr, "$< on rule with no dependencies\n");
          abort();
        }
        dep = rule->deps[0].name;
        deplen = strlen(dep);
        if (outsz + deplen > outcap)
        {
          outcap = 2*outcap + deplen;
          outbuf = realloc(outbuf, outcap);
        }
        memcpy(&outbuf[outsz], dep, deplen);
        outsz += deplen;
        i++;
        continue;
      }
      if (i+1 < len && $1[i+1] == '@')
      {
        char *tgt;
        size_t tgtlen;
        struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
        if (rule->targetsz <= 0)
        {
          fprintf(stderr, "$@ on rule with no targets\n");
          abort();
        }
        tgt = rule->targets[0];
        tgtlen = strlen(tgt);
        if (outsz + tgtlen > outcap)
        {
          outcap = 2*outcap + tgtlen;
          outbuf = realloc(outbuf, outcap);
        }
        memcpy(&outbuf[outsz], tgt, tgtlen);
        outsz += tgtlen;
        i++;
        continue;
      }
      if (i+1 < len && $1[i+1] == '^')
      {
        char *tgt;
        size_t tgtlen;
        size_t tgtidx;
        struct stiryyrule *rule = &stiryy->main->rules[stiryy->main->rulesz - 1];
        for (tgtidx = 0; tgtidx < rule->targetsz; tgtidx++)
        {
          if (tgtidx > 0 || outsz > 0)
          {
            // Emit new command
            if (outsz >= outcap)
            {
              outcap = 2*outcap + 16;
              outbuf = realloc(outbuf, outcap);
            }
            outbuf[outsz++] = '\0';
            stiryy_add_shell(stiryy, outbuf);
            outsz = 0;
          }

          tgt = rule->targets[tgtidx].namenodir;
          tgtlen = strlen(tgt);
          if (outsz + tgtlen > outcap)
          {
            outcap = 2*outcap + tgtlen;
            outbuf = realloc(outbuf, outcap);
          }
          memcpy(&outbuf[outsz], tgt, tgtlen);
          outsz += tgtlen;
        }

        if (tgtidx > 0 || outsz > 0)
        {
          // Emit new command
          if (outsz >= outcap)
          {
            outcap = 2*outcap + 16;
            outbuf = realloc(outbuf, outcap);
          }
          outbuf[outsz++] = '\0';
          stiryy_add_shell(stiryy, outbuf);
          outsz = 0;
        }

        i++;
        continue;
      }
      abort();
    }
    else if ($1[i] == ' ')
    {
      while ($1[i] == ' ')
      {
        i++;
      }
      i--; // will be incremented by for
      if (outsz >= outcap)
      {
        outcap = 2*outcap + 16;
        outbuf = realloc(outbuf, outcap);
      }
      outbuf[outsz++] = '\0';
      stiryy_add_shell(stiryy, outbuf);
      outsz = 0;
      continue;
    }
    if (outsz >= outcap)
    {
      outcap = 2*outcap + 16;
      outbuf = realloc(outbuf, outcap);
    }
    outbuf[outsz++] = $1[i];
  }
  if (outsz >= outcap)
  {
    outcap = 2*outcap + 16;
    outbuf = realloc(outbuf, outcap);
  }
  outbuf[outsz++] = '\0';
  printf("\tshell\n");
  stiryy_add_shell(stiryy, outbuf);
  free($1);
  free(outbuf);
  stiryy_add_shell(stiryy, NULL);
  // FIXME multiple shell command lines
}
| ATTAB expr NEWLINE
{
  // FIXME figure out a way to make expr temporary and exec immediately
  printf("\tshell expr\n");
}
;

targetspec:
  targets
| list
{
  printf("targetlist\n");
}
;
  
depspec:
  deps
| list
{
  printf("deplist\n");
}
;
  

targets:
  FREEFORM_TOKEN
{
  if (!stiryy->main->freeform_token_seen)
  {
    printf("Recommend using string literals instead of free-form tokens\n");
    stiryy->main->freeform_token_seen=1;
  }
  printf("target1 %s\n", $1);
  stiryy_emplace_rule(stiryy);
  stiryy_set_tgt(stiryy, $1);
  free($1);
}
| STRING_LITERAL
{
  printf("target1 %s\n", $1.str);
  stiryy_emplace_rule(stiryy);
  stiryy_set_tgt(stiryy, $1.str);
  free($1.str);
}
| VARREF_LITERAL
{
  stiryy_emplace_rule(stiryy);
  printf("target1ref\n");
  free($1);
}
| targets FREEFORM_TOKEN
{
  if (!stiryy->main->freeform_token_seen)
  {
    printf("Recommend using string literals instead of free-form tokens\n");
    stiryy->main->freeform_token_seen=1;
  }
  printf("target %s\n", $2);
  stiryy_set_tgt(stiryy, $2);
  free($2);
}
| targets STRING_LITERAL
{
  printf("target %s\n", $2.str);
  stiryy_set_tgt(stiryy, $2.str);
  free($2.str);
}
| targets VARREF_LITERAL
{
  printf("targetref\n");
  free($2);
}
;

maybe_rec:
{
  $$ = 0;
}
| RECDEP
{
  $$ = 1;
}
;

deps:
| deps maybe_rec FREEFORM_TOKEN
{
  if (!stiryy->main->freeform_token_seen)
  {
    printf("Recommend using string literals instead of free-form tokens\n");
    stiryy->main->freeform_token_seen=1;
  }
  printf("dep %s rec? %d\n", $3, (int)$2);
  stiryy_set_dep(stiryy, $3, $2);
  free($3);
}
| deps maybe_rec STRING_LITERAL
{
  printf("dep %s rec? %d\n", $3.str, (int)$2);
  stiryy_set_dep(stiryy, $3.str, $2);
  free($3.str);
}
| deps maybe_rec VARREF_LITERAL
{
  printf("depref\n");
  free($3);
}
;

stirrules:
| stirrules stirrule
| stirrules NEWLINE
| stirrules assignrule
| stirrules PRINT NUMBER NEWLINE
{
  printf("%g\n", $3);
}
| stirrules FILEINCLUDE STRING_LITERAL NEWLINE
{
  free($3.str);
}
| stirrules FILEINCLUDE VARREF_LITERAL NEWLINE
| stirrules DIRINCLUDE STRING_LITERAL NEWLINE
{
  free($3.str);
}
| stirrules DIRINCLUDE VARREF_LITERAL NEWLINE
| stirrules CDEPINCLUDESCURDIR STRING_LITERAL NEWLINE
{
  stiryy_set_cdepinclude(stiryy, $3.str);
  free($3.str);
}
| stirrules CDEPINCLUDESCURDIR VARREF_LITERAL NEWLINE
| stirrules FUNCTION VARREF_LITERAL
{
  amyplanyy->ctx = amyplan_locvarctx_alloc(NULL, 2, (size_t)-1, (size_t)-1);
}
OPEN_PAREN maybe_parlist CLOSE_PAREN NEWLINE
{
  size_t funloc = get_abce(amyplanyy)->bytecodesz;
  amyplanyy_add_fun_sym(amyplanyy, $3, 0, funloc);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FUN_HEADER);
  amyplanyy_add_double(amyplanyy, amyplanyy->ctx->args);
}
  funlines
  ENDFUNCTION NEWLINE
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NIL); // retval
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, amyplanyy->ctx->args); // argcnt
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, amyplanyy->ctx->sz - amyplanyy->ctx->args); // locvarcnt
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_RETEX2);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_FUN_TRAILER);
  amyplanyy_add_double(amyplanyy, amyplan_symbol_add(amyplanyy, $3, strlen($3)));
  free($3);
  amyplan_locvarctx_free(amyplanyy->ctx);
  amyplanyy->ctx = NULL;
}
;

maybe_parlist:
| parlist
;

parlist:
VARREF_LITERAL
{
  amyplan_locvarctx_add_param(amyplanyy->ctx, $1);
  free($1);
}
| parlist COMMA VARREF_LITERAL
{
  amyplan_locvarctx_add_param(amyplanyy->ctx, $3);
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
  amyplan_locvarctx_add(amyplanyy->ctx, $3);
  free($3);
}
| locvarlines NEWLINE
;

bodylines:
| statement bodylinescont
;

bodylinescont:
| bodylinescont statement
| bodylinescont NEWLINE
;

statement:
  lvalue EQUALS SUB NEWLINE
{
  if ($1 != ABCE_OPCODE_DICTSET_MAINTAIN)
  {
    printf("Can remove only from dict\n");
    YYABORT;
  }
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DICTDEL);
}
| lvalue EQUALS expr NEWLINE
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
| RETURN expr NEWLINE
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, amyplan_locvarctx_arg_sz(amyplanyy->ctx)); // arg
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, amyplan_locvarctx_recursive_sz(amyplanyy->ctx)); // loc
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_RETEX2);
}
| BREAK NEWLINE
{
  int64_t loc = amyplan_locvarctx_break(amyplanyy->ctx, 1);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FALSE);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, loc);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
}
| CONTINUE NEWLINE
{
  int64_t loc = amyplan_locvarctx_continue(amyplanyy->ctx, 1);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, loc);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
}
| BREAK NUMBER NEWLINE
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
| CONTINUE NUMBER NEWLINE
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
| expr NEWLINE
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP); // called for side effects only
}
| IF OPEN_PAREN expr CLOSE_PAREN NEWLINE
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  $<d>$ = get_abce(amyplanyy)->bytecodesz;
  amyplanyy_add_double(amyplanyy, -50); // to be overwritten
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_IF_NOT_JMP);
}
  bodylinescont
{
  amyplanyy_set_double(amyplanyy, $<d>6, get_abce(amyplanyy)->bytecodesz);
  $<d>$ = $<d>6; // For overwrite by maybe_else
}
  maybe_else
  ENDIF NEWLINE
| WHILE
{
  $<d>$ = get_abce(amyplanyy)->bytecodesz; // startpoint
}
  OPEN_PAREN expr CLOSE_PAREN NEWLINE
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
  bodylinescont
  ENDWHILE NEWLINE
{
  struct amyplan_locvarctx *ctx = amyplanyy->ctx->parent;
  free(amyplanyy->ctx);
  amyplanyy->ctx = ctx;
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, $<d>2);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
  amyplanyy_set_double(amyplanyy, $<d>7 + 1, get_abce(amyplanyy)->bytecodesz);
}
| ONCE
{
  $<d>$ = get_abce(amyplanyy)->bytecodesz; // startpoint
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_TRUE);
}
  NEWLINE
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
  bodylinescont
  ENDONCE NEWLINE
{
  struct amyplan_locvarctx *ctx = amyplanyy->ctx->parent;
  free(amyplanyy->ctx);
  amyplanyy->ctx = ctx;
  amyplanyy_set_double(amyplanyy, $<d>4 + 1, get_abce(amyplanyy)->bytecodesz);
}
| APPEND OPEN_PAREN expr COMMA expr CLOSE_PAREN NEWLINE
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPEND_MAINTAIN);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
}
| APPEND_LIST OPEN_PAREN expr COMMA expr CLOSE_PAREN NEWLINE
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPENDALL_MAINTAIN);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_POP);
}
| STDOUT OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, 0);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_OUT);
}
| STDERR OPEN_PAREN expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, 1);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_OUT);
}
| ERROR OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ERROR); }
| DUMP OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DUMP); }
| EXIT OPEN_PAREN CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXIT); }
;

maybe_else:
| ELSE NEWLINE
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  $<d>$ = get_abce(amyplanyy)->bytecodesz;
  amyplanyy_add_double(amyplanyy, -50); // to be overwritten
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_JMP);
  amyplanyy_set_double(amyplanyy, $<d>0, get_abce(amyplanyy)->bytecodesz); // Overwrite
}
bodylinescont
{
  amyplanyy_set_double(amyplanyy, $<d>3, get_abce(amyplanyy)->bytecodesz);
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
| OPEN_BRACE AT expr CLOSE_BRACE
{
  $$ = ABCE_OPCODE_PBGET; // FIXME needs transfer size of operation
}
| OPEN_BRACE AT CLOSE_BRACE
{
  $$ = ABCE_OPCODE_PBLEN; // This is very special: CAN assign to length query
}
;

lvalue:
  varref
{
  $$ = $1;
}
| varref
{
  amyplanyy_add_byte(amyplanyy, $1);
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
  amyplanyy_add_byte(amyplanyy, $1);
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
  amyplanyy_add_byte(amyplanyy, $1);
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
  amyplanyy_add_byte(amyplanyy, $1);
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
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, get_abce(amyplanyy)->dynscope.u.area->u.sc.locidx);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
}
  OPEN_BRACKET maybe_atqm expr CLOSE_BRACKET
{
  $$ = $4;
}
;

dynstart:
  DYN
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN);
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
  if ($2 == ABCE_OPCODE_LISTSET)
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LISTGET);
  }
  else
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DICTGET);
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
    free($1);
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
      abort();
    }
    free($1);
    $$ = ABCE_OPCODE_PUSH_STACK;
  }
}
| DO VARREF_LITERAL
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN);
  int64_t idx = abce_cache_add_str(get_abce(amyplanyy), $2, strlen($2));
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, idx);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  free($2);
  $$ = ABCE_OPCODE_SCOPEVAR_NONRECURSIVE;
}
| LO VARREF_LITERAL
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, get_abce(amyplanyy)->dynscope.u.area->u.sc.locidx);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);

  int64_t idx = abce_cache_add_str(get_abce(amyplanyy), $2, strlen($2));
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, idx);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  free($2);
  $$ = ABCE_OPCODE_SCOPEVAR_NONRECURSIVE;
}
| IO VARREF_LITERAL
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
  free($2);
  $$ = ABCE_OPCODE_PUSH_FROM_CACHE;
}
| D VARREF_LITERAL
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN);

  int64_t idx = abce_cache_add_str(get_abce(amyplanyy), $2, strlen($2));
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, idx);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  free($2);
  $$ = ABCE_OPCODE_SCOPEVAR;
}
| L VARREF_LITERAL
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, get_abce(amyplanyy)->dynscope.u.area->u.sc.locidx);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);

  int64_t idx = abce_cache_add_str(get_abce(amyplanyy), $2, strlen($2));
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, idx);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  free($2);
  $$ = ABCE_OPCODE_SCOPEVAR;
}
| I VARREF_LITERAL
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
  free($2);
  $$ = ABCE_OPCODE_PUSH_FROM_CACHE;
}
;

expr: expr11;

expr1:
  expr0
| LOGICAL_NOT expr1
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LOGICAL_NOT);
}
| BITWISE_NOT expr1
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_BITWISE_NOT);
}
| ADD expr1
| SUB expr1
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_UNARY_MINUS);
}
;

expr2:
  expr1
| expr2 MUL expr1
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_MUL);
}
| expr2 DIV expr1
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DIV);
}
| expr2 MOD expr1
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_MOD);
}
;

expr3:
  expr2
| expr3 ADD expr2
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ADD);
}
| expr3 SUB expr2
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SUB);
}
;

expr4:
  expr3
| expr4 SHL expr3
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SHL);
}
| expr4 SHR expr3
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SHR);
}
;

expr5:
  expr4
| expr5 LT expr4
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LT);
}
| expr5 LE expr4
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LE);
}
| expr5 GT expr4
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GT);
}
| expr5 GE expr4
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GE);
}
;

expr6:
  expr5
| expr6 EQ expr5
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EQ);
}
| expr6 NE expr5
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_NE);
}
;

expr7:
  expr6
| expr7 BITWISE_AND expr6
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_BITWISE_AND);
}
;

expr8:
  expr7
| expr8 BITWISE_XOR expr7
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_BITWISE_XOR);
}
;

expr9:
  expr8
| expr9 BITWISE_OR expr8
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_BITWISE_OR);
}
;

expr10:
  expr9
| expr10 LOGICAL_AND expr9
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LOGICAL_AND);
}
;

expr11:
  expr10
| expr11 LOGICAL_OR expr10
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LOGICAL_OR);
}
;


expr0:
  OPEN_PAREN expr CLOSE_PAREN
| OPEN_PAREN expr CLOSE_PAREN OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, $5);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL);
}
| OPEN_PAREN expr CLOSE_PAREN MAYBE_CALL
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL_IF_FUN);
}
| dict maybe_bracketexprlist
| list maybe_bracketexprlist
| STRING_LITERAL
{
  int64_t idx = abce_cache_add_str(get_abce(amyplanyy), $1.str, $1.sz);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, idx);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
  free($1.str);
}
| NUMBER
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, $1);
}
| TRUE { amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_TRUE); }
| TYPE OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TYPE); }
| FALSE { amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FALSE); }
| NIL { amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NIL); }
| STR_FROMCHR OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STR_FROMCHR); }
| STR_LOWER OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STR_LOWER); }
| STR_UPPER OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STR_UPPER); }
| STR_REVERSE OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STR_REVERSE); }
| STRCMP OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STR_CMP); }
| STRSTR OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRSTR); }
| STRREP OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRREP); }
| STRLISTJOIN OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRLISTJOIN); }
| STRAPPEND OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRAPPEND); }
| STRSTRIP OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRSTRIP); }
| STRSUB OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRSUB); }
| STRGSUB OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRGSUB); }
| STRSET OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRSET); }
| STRWORD OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRWORD); }
| STRWORDCNT OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRWORDCNT); }
| STRWORDLIST OPEN_PAREN expr COMMA expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_STRWORDCNT); }
| ABS OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ABS); }
| ACOS OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ACOS); }
| ASIN OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ASIN); }
| ATAN OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_ATAN); }
| CEIL OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CEIL); }
| COS OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_COS); }
| SIN OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SIN); }
| TAN OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TAN); }
| EXP OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_EXP); }
| LOG OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_LOG); }
| SQRT OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SQRT); }
| DUP_NONRECURSIVE OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DUP_NONRECURSIVE); }
| PB_NEW OPEN_PAREN CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NEW_PB); }
| TOSTRING OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TOSTRING); }
| TONUMBER OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_TONUMBER); }
| SCOPE_PARENT OPEN_PAREN expr CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SCOPE_PARENT); }
| SCOPE_NEW OPEN_PAREN expr COMMA expr CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_SCOPE_NEW);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
}
| GETSCOPE_DYN OPEN_PAREN CLOSE_PAREN
{ amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_GETSCOPE_DYN); }
| GETSCOPE_LEX OPEN_PAREN CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, get_abce(amyplanyy)->dynscope.u.area->u.sc.locidx);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_FROM_CACHE);
}
| lvalue
{
  add_corresponding_get(amyplanyy, $1);
}
| lvalue
{
  add_corresponding_get(amyplanyy, $1);
}
  OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_DBL);
  amyplanyy_add_double(amyplanyy, $4);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL);
}
| lvalue MAYBE_CALL
{
  add_corresponding_get(amyplanyy, $1);
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_CALL_IF_FUN);
}
| IMM OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist
{
  abort();
}
| IMM OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  abort();
}
| IMM OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist MAYBE_CALL
{
  abort();
}
| DYNO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist
{
  abort();
}
| DYNO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  abort();
}
| DYNO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist MAYBE_CALL
{
  abort();
}
| LEXO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist
{
  abort();
}
| LEXO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  abort();
}
| LEXO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist MAYBE_CALL
{
  abort();
}
| IMMO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist
{
  abort();
}
| IMMO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  abort();
}
| IMMO OPEN_BRACKET expr CLOSE_BRACKET maybe_bracketexprlist MAYBE_CALL
{
  abort();
}
| LOC OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET maybe_bracketexprlist
{
  free($3.str);
  abort();
}
| LOC OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET maybe_bracketexprlist OPEN_PAREN maybe_arglist CLOSE_PAREN
{
  free($3.str);
  abort();
}
| LOC OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET maybe_bracketexprlist MAYBE_CALL
{
  free($3.str);
  abort();
}
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
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NEW_ARRAY);
}
maybe_valuelist CLOSE_BRACKET
;

dict:
OPEN_BRACE
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_PUSH_NEW_DICT);
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
  value COLON value
{
  amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_DICTSET_MAINTAIN);
}
;

maybe_valuelist:
| valuelist
;

valuelist:
  valuelistentry
{
  if ($1)
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPENDALL_MAINTAIN);
  }
  else
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPEND_MAINTAIN);
  }
}
| valuelist COMMA valuelistentry
{
  if ($3)
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPENDALL_MAINTAIN);
  }
  else
  {
    amyplanyy_add_byte(amyplanyy, ABCE_OPCODE_APPEND_MAINTAIN);
  }
}
;

valuelistentry:
  value
{
  $$ = $1;
};
