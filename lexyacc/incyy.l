%option nounput noinput
%option prefix="incyy"

%{
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include "yyutils.h"
#include "incyy.tab.h"

#define YYSTYPE INCYYSTYPE
#define YYLTYPE INCYYLTYPE

#define YY_USER_ACTION yylloc->first_line = yylloc->last_line = yylineno; \
  yylloc->first_column = yycolumn; yylloc->last_column = yycolumn+yyleng-1; \
  yycolumn += yyleng;

%}

%option reentrant bison-bridge bison-locations

%%

[$]^ {
  printf("Variable references not supported\n");
  return ERROR_TOK;
}
[$]< {
  printf("Variable references not supported\n");
  return ERROR_TOK;
}
[$]@ {
  printf("Variable references not supported\n");
  return ERROR_TOK;
}
[$][A-Za-z0-9_.-/]+ {
  printf("Variable references not supported\n");
  return ERROR_TOK;
}
[$][(][A-Za-z0-9_.-/]+[)] {
  printf("Variable references not supported\n");
  return ERROR_TOK;
}
[A-Za-z0-9_.][A-Za-z0-9_.-/]* {
  yylval->s = strdup(yytext);
  return FREEFORM_TOKEN;
}
:         return COLON;

#[^\n]*  /* ignore */;

\n                                { ++yylineno; yycolumn=1; return NEWLINE; }
\\\n                              { ++yylineno; yycolumn=1; }
\t {
  if (yycolumn == 2)
  {
    printf("Make include-only files do not support commands\n");
    return ERROR_TOK;
  }
  else
  {
    // ignore
  }
}
[ \t]                            /* ignore whitespaces */;
.            return ERROR_TOK;


%%
