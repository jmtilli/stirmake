%option nounput noinput
%option prefix="dbyy"

%{
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "yyutils.h"
#include "dbyy.tab.h"

/*
#define YYSTYPE DBYYSTYPE
#define YYLTYPE DBYYLTYPE
*/

/*
#define YY_USER_ACTION yylloc->first_line = yylloc->last_line = yylineno; \
  yylloc->first_column = yycolumn; yylloc->last_column = yycolumn+yyleng-1; \
  yycolumn += yyleng;
*/
#define YY_USER_ACTION yycolumn += yyleng;

%}

%option reentrant bison-bridge

%%
  int last_newline_true = 1;

:         return COLON;

@v1 return V1;


\"([^\\\"]|\\.)*\"  yylval->str=yy_escape_string(yytext); return STRING_LITERAL;
\'([^\\\']|\\.)*\'  yylval->str=yy_escape_string_single(yytext); return STRING_LITERAL;
#[^\n]*  /* ignore comments */;

\n                                { ++yylineno; yycolumn=1; last_newline_true = 1; return NEWLINE; }
\\\n                              { ++yylineno; yycolumn=1; last_newline_true = 0;}
\t {
  if (yycolumn == 2 && last_newline_true)
  {
    // Stirmake databases do not support commands
    return TAB;
  }
  else
  {
    // ignore
  }
}
[ \t]                            /* ignore whitespaces */;
.            return ERROR_TOK;


%%
