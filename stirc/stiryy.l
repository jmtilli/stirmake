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

/*
#define YYSTYPE STIRYYSTYPE
#define YYLTYPE STIRYYLTYPE
*/

/*
#define YY_USER_ACTION yylloc->first_line = yylloc->last_line = yylineno; \
  yylloc->first_column = yycolumn; yylloc->last_column = yycolumn+yyleng-1; \
  yycolumn += yyleng;
*/
#define YY_USER_ACTION yycolumn += yyleng;

%}

%option reentrant bison-bridge
%x SHELL
%x PERCENTLUA

%%

%{
struct CSnippet luaSnippet = {
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
@strappend return STRAPPEND;
@sufsub return SUFSUB;
@suffilter return SUFFILTER;
@phonyrule return PHONYRULE;
@mayberule return MAYBERULE;
@rectgtrule return RECTGTRULE;
@distrule return DISTRULE;
@patrule return PATRULE;
@fileinclude return FILEINCLUDE;
@dirinclude return DIRINCLUDE;
@projdirinclude return PROJDIRINCLUDE;
@cdepincludescurdir return CDEPINCLUDESCURDIR;
@function return FUNCTION;
@endfunction return ENDFUNCTION;
@locvar   return LOCVAR;
@recdep   return RECDEP;
@orderonly   return ORDERONLY;
@once return ONCE;
@endonce return ENDONCE;
@stdout return STDOUT;
@stderr return STDERR;
@error return ERROR;
@dump return DUMP;
@deponly return DEPONLY;
@exit return EXIT;
@D     return D;
@L     return L;
@I     return I;
@DO     return DO;
@LO     return LO;
@IO     return IO;
@DP     return DP;
@LP     return LP;
@IP     return IP;
@DPO     return DPO;
@LPO     return LPO;
@IPO     return IPO;
@dyn   return DYN;
@lex   return LEX;
@imm   return IMM;
@dyno   return DYNO;
@lexo   return LEXO;
@immo   return IMMO;
@loc   return LOC;
@if   return IF;
@else   return ELSE;
@endif   return ENDIF;
@while return WHILE;
@endwhile return ENDWHILE;
@break return BREAK;
@continue return CONTINUE;
@return  return RETURN;
@append   return APPEND;
@appendlist   return APPEND_LIST;

@[?] return ATQM;
@scope return SCOPE;
@true return TRUE;
@type return TYPE;
@false return FALSE;
@nil return NIL;
@strfromchr return STR_FROMCHR;
@strlower return STR_LOWER;
@strupper return STR_UPPER;
@strreverse return STR_REVERSE;
@strcmp return STRCMP;
@strstr return STRSTR;
@strrep return STRREP;
@strlistjoin return STRLISTJOIN;
@strstrip return STRSTRIP;
@strsub return STRSUB;
@strgsub return STRGSUB;
@strset return STRSET;
@strword return STRWORD;
@strwordcnt return STRWORDCNT;
@strwordlist return STRWORDLIST;
@abs return ABS;
@acos return ACOS;
@asin return ASIN;
@atan return ATAN;
@ceil return CEIL;
@cos return COS;
@sin return SIN;
@tan return TAN;
@exp return EXP;
@log return LOG;
@sqrt return SQRT;
@dupnonrec return DUP_NONRECURSIVE;
@pbnew return PB_NEW;
@tostring return TOSTRING;
@tonumber return TONUMBER;
@scopeparent return SCOPE_PARENT;
@scopenew return SCOPE_NEW;
@getscopedyn return GETSCOPE_DYN;
@getscopelex return GETSCOPE_LEX;
@beginscope return BEGINSCOPE;
@beginholeyscope return BEGINHOLEYSCOPE;
@endscope return ENDSCOPE;

@dirup return DIRUP;
@dirdown return DIRDOWN;

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
\?=         return QMEQUALS;
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
\<\> return MAYBE_CALL;

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
[$][*] {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
[$][A-Za-z0-9_.]+ {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
[$][(][A-Za-z0-9_.]+[)] {
  yylval->s = strdup(yytext+2);
  yylval->s[strlen(yylval->s)-1] = '\0';
  return VARREF_LITERAL;
}
[-+]?[0-9]+(.[0-9]+)?([eE][-+]?[0-9]+)? {
  yylval->d = atof(yytext);
  return NUMBER;
}
[A-Za-z0-9_.]([A-Za-z0-9_./]*[A-Za-z0-9_.])? {
  /*
   * Note this is slightly imperfect. 6/2 which ought to be an
   * expression evaluating to 3 is handled instead as a free-form
   * token. We can't avoid that as we want to support slashes in
   * file names. The solution for the user is to type (6)/(2) or
   * 6 / 2 or anything else. Very similar to the ">>" problem in C++.
   */
  yylval->s = strdup(yytext);
  return FREEFORM_TOKEN;
}

\"([^\\\"]|\\.)*\"  yylval->str=yy_escape_string(yytext); return STRING_LITERAL;
\'([^\\\']|\\.)*\'  yylval->str=yy_escape_string_single(yytext); return STRING_LITERAL;
#[^\n]*  /* ignore */;

"%lua{"             { BEGIN(PERCENTLUA); luaSnippet.capacity = 0; luaSnippet.len = 0; luaSnippet.data = NULL; }
<PERCENTLUA>%\}  {
                     BEGIN(INITIAL);
                     yylval->s = luaSnippet.data;
                     luaSnippet.data = NULL;
                     luaSnippet.capacity = 0;
                     luaSnippet.len = 0;
                     return PERCENTLUA_LITERAL;
                 }
<PERCENTLUA>.             { csadd(&luaSnippet, yytext[0]); }
<PERCENTLUA>"\n"          { csadd(&luaSnippet, yytext[0]); ++yylineno; yycolumn=1; }

\n                                { ++yylineno; yycolumn=1; return NEWLINE; }
\\\n                              { ++yylineno; yycolumn=1; }
\t {
  if (yycolumn == 2)
  {
    BEGIN(SHELL);
  }
  else
  {
    // ignore
  }
}
<SHELL>[^\n]+(\\\n[^\n]*)* {
  yylval->s = strdup(yytext);
  size_t len = strlen(yylval->s);
  size_t idx;
  yycolumn = 2;
  for (idx = 0; idx < len; idx++)
  {
    if (yylval->s[idx] == '\n')
    {
      ++yylineno;
      yycolumn=1;
    }
    else
    {
      ++yycolumn;
    }
  }
  return SHELL_COMMAND;
}
<SHELL>\n {
  ++yylineno;
  yycolumn=1;
  BEGIN(INITIAL);
  return NEWLINE;
}
[ \t]                            /* ignore whitespaces */;
.            return ERROR_TOK;


%%
