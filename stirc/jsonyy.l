%option nounput noinput
%option prefix="jsonyy"

%{
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "jsonyyutils.h"
#include "jsonyy.tab.h"

/*
#define YYSTYPE JSONYYSTYPE
#define YYLTYPE JSONYYLTYPE
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


\"([\x20-\x21\x23-\x5B\x5D-\xFF]|\\[\x22\x5C\x2F\x62\x66\x6E\x72\x74]|\\u[0-9A-F][0-9A-F][0-9A-F][0-9A-F])*\"  yylval->str=jsonyy_escape_string(yytext); return STRING_LITERAL;

[-]?(0|[1-9][0-9]*)([.][0-9]+)?([eE][-+]?[0-9]+)? yylval->d = strtod(yytext, NULL); return NUMBER;



true return TRUE;
false return FALSE;
null return NIL;
[[] return OPEN_BRACKET;
[]] return CLOSE_BRACKET;
[{] return OPEN_BRACE;
[}] return CLOSE_BRACE;
[:] return COLON;
[,] return COMMA;

\n                                { ++yylineno; yycolumn=1; }
[ \t]                            /* ignore whitespaces */;
.            return ERROR_TOK;


%%
