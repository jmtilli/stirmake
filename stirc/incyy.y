/*
%code requires {
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void *yyscan_t;
#endif
#include "incyy.h"
#include <sys/types.h>
}

%define api.prefix {incyy}
*/

%{

/*
#define YYSTYPE INCYYSTYPE
#define YYLTYPE INCYYLTYPE
*/

#include "incyy.h"
#include "yyutils.h"
#include "incyy.tab.h"
#include "incyy.lex.h"
#include <arpa/inet.h>

void incyyerror(/*YYLTYPE *yylloc, */yyscan_t scanner, struct incyy *incyy, const char *str)
{
        //fprintf(stderr, "error: %s at line %d col %d\n",str, yylloc->first_line, yylloc->first_column);
        // FIXME we need better location info!
        fprintf(stderr, "inc error: %s at line %d\n", str, incyyget_lineno(scanner));
	abort();
}

int incyywrap(yyscan_t scanner)
{
        return 1;
}

%}

%pure-parser
%lex-param {yyscan_t scanner}
%parse-param {yyscan_t scanner}
%parse-param {struct incyy *incyy}
/*
%locations
*/

%union {
  char *s;
  struct escaped_string str;
}

/*
%destructor { free ($$); } FREEFORM_TOKEN
*/

%token NEWLINE

%token COLON
%token <s> FREEFORM_TOKEN

%token ERROR_TOK

/*
%type<s> FREEFORM_TOKEN
*/

%start st

%%

st: incrules ;

incrules:
| incrules incrule
| incrules NEWLINE
;

incrule:
{
  incyy_emplace_rule(incyy);
}
  targetspec COLON depspec NEWLINE
;

targetspec:
  targets
;
  
depspec:
  deps
;
  

targets:
  FREEFORM_TOKEN
{
  //printf("target1 %s\n", $1);
  incyy_set_tgt(incyy, $1);
  free($1);
}
| targets FREEFORM_TOKEN
{
  //printf("target %s\n", $2);
  incyy_set_tgt(incyy, $2);
  free($2);
}
;

deps:
| deps FREEFORM_TOKEN
{
  //printf("dep %s\n", $2);
  incyy_set_dep(incyy, $2);
  free($2);
}
;
