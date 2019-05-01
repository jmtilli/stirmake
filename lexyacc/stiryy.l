%option nounput noinput
%option prefix="stiryy"

%{
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include "yyutils.h"
#include "stiryy.tab.h"

#define YYSTYPE STIRYYSTYPE
#define YYLTYPE STIRYYLTYPE

#define YY_USER_ACTION yylloc->first_line = yylloc->last_line = yylineno; \
  yylloc->first_column = yycolumn; yylloc->last_column = yycolumn+yyleng-1; \
  yycolumn += yyleng;

%}

%option reentrant bison-bridge bison-locations
%%

%{
struct CSnippet cSnippet = {
  .data = NULL,
  .len = 0,
  .capacity = 0,
};
%}

@         return AT;
@print    return PRINT;
@delayvar return DELAYVAR;
@delaylistexpand return DELAYLISTEXPAND;
@delayexpr return DELAYEXPR;
@sufsubone return SUFSUBONE;
@sufsub return SUFSUB;
@suffilter return SUFFILTER;
@phonyrule return PHONYRULE;
@distrule return DISTRULE;
@patrule return PATRULE;
@fileinclude return FILEINCLUDE;
@dirinclude return DIRINCLUDE;
@cdepincludescurdir return CDEPINCLUDESCURDIR;
@function return FUNCTION;
@endfunction return ENDFUNCTION;
@locvar   return LOCVAR;
@D     return D;
@L     return L;
@I     return I;
@DO     return DO;
@LO     return LO;
@IO     return IO;
@dyn   return DYN;
@lex   return LEX;
@imm   return IMM;
@dyno   return DYNO;
@lexo   return LEXO;
@immo   return IMMO;
@loc   return LOC;
@if   return IF;
@endif   return ENDIF;
@while return WHILE;
@endwhile return ENDWHILE;
@break return BREAK;
@continue return CONTINUE;
@return  return RETURN;
@append   return APPEND;
@appendList   return APPEND_LIST;
@addRule   return ADD_RULE;
@RULE_DIST   return RULE_DIST;
@RULE_PHONY   return RULE_PHONY;
@RULE_ORDINARY   return RULE_ORDINARY;
:         return COLON;
,         return COMMA;
=         return EQUALS;
&& return LOGICAL_AND;
\|\| return LOGICAL_OR;
!= return NE;
== return EQ;
\+=         return PLUSEQUALS;
\<\< return SHL;
\>\> return SHR;
\<        return LT;
\>        return GT;
[(]       return OPEN_PAREN;
[)]       return CLOSE_PAREN;
[[]       return OPEN_BRACKET;
[]]       return CLOSE_BRACKET;
[{]       return OPEN_BRACE;
[}]       return CLOSE_BRACE;
\/ return DIV;
\* return MUL;
\+ return ADD;
- return SUB;
& return BITWISE_AND;
\| return BITWISE_OR;
~ return BITWISE_NOT;
\^ return BITWISE_XOR;
! return LOGICAL_NOT;
% return MOD;
\>= return GE;
\<= return LE;

\t[^\n]* {
  yylval->s = strdup(yytext+1);
  return SHELL_COMMAND;
}
@\t return ATTAB;


[$]^ {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
[$]< {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
[$]@ {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
[$][A-Za-z0-9_.-/]+ {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
[$][(][A-Za-z0-9_.-/]+[)] {
  yylval->s = strdup(yytext+2);
  yylval->s[strlen(yylval->s)-1] = '\0';
  return VARREF_LITERAL;
}
[-+]?[0-9]+(.[0-9]+)?([eE][-+]?[0-9]+)? {
  yylval->d = atof(yytext);
  return NUMBER;
}
[A-Za-z0-9_.][A-Za-z0-9_.-/]* {
  yylval->s = strdup(yytext);
  return FREEFORM_TOKEN;
}

\"([^\\\"]|\\.)*\"  yylval->str=yy_escape_string(yytext); return STRING_LITERAL;
\'([^\\\']|\\.)*\'  yylval->str=yy_escape_string_single(yytext); return STRING_LITERAL;
#[^\n]*  /* ignore */;

\n                                { ++yylineno; yycolumn=1; return NEWLINE; }
\\\n                              { ++yylineno; yycolumn=1; }
[ \t]+                            /* ignore whitespaces */;
.            return ERROR_TOK;


%%
