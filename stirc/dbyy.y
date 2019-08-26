/*
%code requires {
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void *yyscan_t;
#endif
#include "dbyy.h"
#include <sys/types.h>
}

%define api.prefix {dbyy}
*/

%{

/*
#define YYSTYPE DBYYSTYPE
#define YYLTYPE DBYYLTYPE
*/

#include "dbyy.h"
#include "yyutils.h"
#include "dbyy.tab.h"
#include "dbyy.lex.h"
#include <arpa/inet.h>

void dbyyerror(/*YYLTYPE *yylloc, */yyscan_t scanner, struct dbyy *dbyy, const char *str)
{
        //fprintf(stderr, "error: %s at line %d col %d\n",str, yylloc->first_line, yylloc->first_column);
        // FIXME we need better location info!
        //fprintf(stderr, "db error: %s at line %d\n", str, dbyyget_lineno(scanner));
	//my_abort();
}

int dbyywrap(yyscan_t scanner)
{
        return 1;
}

%}

%pure-parser
%lex-param {yyscan_t scanner}
%parse-param {yyscan_t scanner}
%parse-param {struct dbyy *dbyy}

%union {
  char *s;
  struct escaped_string str;
}

%token NEWLINE

%token COLON
%token TAB
%token <str> STRING_LITERAL

%token ERROR_TOK

%start st

%%

st: dbrules ;

dbrules:
| dbrules dbrule
| dbrules NEWLINE
;

dbrule: STRING_LITERAL STRING_LITERAL COLON
{
  if (strlen($1.str) != $1.sz)
  {
    fprintf(stderr, "NUL in DB\n");
    YYABORT;
  }
  if (strlen($2.str) != $2.sz)
  {
    fprintf(stderr, "NUL in DB\n");
    YYABORT;
  }
  dbyy_emplace_rule(dbyy, $1.str, $2.str);
}
  NEWLINE
  commands
;

cmdline: 
| cmdline STRING_LITERAL
{
  if (strlen($2.str) != $2.sz)
  {
    fprintf(stderr, "NUL in DB\n");
    YYABORT;
  }
  dbyy_add_arg(dbyy, $2.str);
}
;

commands:
| commands TAB
{
  dbyy_add_cmd(dbyy);
}
  cmdline NEWLINE
;
