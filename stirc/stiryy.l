%option nounput noinput
%option prefix="stiryy"

%{
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
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
%option stack
%option extra-type="unsigned int"
%x SHELL
%x PERCENTLUA
%x STRICT

%%

%{
struct CSnippet luaSnippet = {
  .data = NULL,
  .len = 0,
  .capacity = 0,
};
%}

<INITIAL,STRICT>@strict BEGIN(STRICT);
<INITIAL,STRICT>@         return AT;
<INITIAL,STRICT>@toplevel  return TOPLEVEL;
<INITIAL,STRICT>@subfile  return SUBFILE;
<INITIAL,STRICT>@print    return PRINT;
<INITIAL,STRICT>@call    return CALL;
<INITIAL,STRICT>@delayvar return DELAYVAR;
<INITIAL,STRICT>@delaylistexpand return DELAYLISTEXPAND;
<INITIAL,STRICT>@delayexpr return DELAYEXPR;
<INITIAL,STRICT>@sufsubone return SUFSUBONE;
<INITIAL,STRICT>@strappend return STRAPPEND;
<INITIAL,STRICT>@sufsuball return SUFSUBALL;
<INITIAL,STRICT>@suffilter return SUFFILTER;
<INITIAL,STRICT>@suffilterout return SUFFILTEROUT;
<INITIAL,STRICT>@pathsuffix return PATHSUFFIX;
<INITIAL,STRICT>@pathsimplify return PATHSIMPLIFY;
<INITIAL,STRICT>@pathbasename return PATHBASENAME;
<INITIAL,STRICT>@pathdir return PATHDIR;
<INITIAL,STRICT>@pathnotdir return PATHNOTDIR;
<INITIAL,STRICT>@pathsuffixall return PATHSUFFIXALL;
<INITIAL,STRICT>@pathsimplifyall return PATHSIMPLIFYALL;
<INITIAL,STRICT>@pathbasenameall return PATHBASENAMEALL;
<INITIAL,STRICT>@pathdirall return PATHDIRALL;
<INITIAL,STRICT>@pathnotdirall return PATHNOTDIRALL;
<INITIAL,STRICT>@glob return GLOB;
<INITIAL,STRICT>@jsonin return JSONIN;
<INITIAL,STRICT>@phonyrule return PHONYRULE;
<INITIAL,STRICT>@mayberule return MAYBERULE;
<INITIAL,STRICT>@rectgtrule return RECTGTRULE;
<INITIAL,STRICT>@detouchrule return DETOUCHRULE;
<INITIAL,STRICT>@distrule return DISTRULE;
<INITIAL,STRICT>@disttgt return DISTTGT;
<INITIAL,STRICT>@cleanhook return CLEANHOOK;
<INITIAL,STRICT>@distcleanhook return DISTCLEANHOOK;
<INITIAL,STRICT>@bothcleanhook return BOTHCLEANHOOK;
<INITIAL,STRICT>@patrule return PATRULE;
<INITIAL,STRICT>@fileinclude return FILEINCLUDE;
<INITIAL,STRICT>@dirinclude return DIRINCLUDE;
<INITIAL,STRICT>@projdirinclude return PROJDIRINCLUDE;

<INITIAL,STRICT>@cdepincludes return CDEPINCLUDES;
<INITIAL,STRICT>@autophony return AUTOPHONY;
<INITIAL,STRICT>@autotarget return AUTOTARGET;
<INITIAL,STRICT>@ignore return IGNORE;
<INITIAL,STRICT>@noecho return NOECHO;
<INITIAL,STRICT>@ismake return ISMAKE;

<INITIAL,STRICT>@function return FUNCTION;
<INITIAL,STRICT>@endfunction return ENDFUNCTION;
<INITIAL,STRICT>@locvar   return LOCVAR;
<INITIAL,STRICT>@recdep   return RECDEP;
<INITIAL,STRICT>@order   return ORDER;
<INITIAL,STRICT>@wait   return WAIT;
<INITIAL,STRICT>@orderonly   return ORDERONLY;
<INITIAL,STRICT>@once return ONCE;
<INITIAL,STRICT>@endonce return ENDONCE;
<INITIAL,STRICT>@stdout return STDOUT;
<INITIAL,STRICT>@stderr return STDERR;
<INITIAL,STRICT>@error return ERROR;
<INITIAL,STRICT>@dump return DUMP;
<INITIAL,STRICT>@deponly return DEPONLY;
<INITIAL,STRICT>@exit return EXIT;
<INITIAL,STRICT>@D     return D;
<INITIAL,STRICT>@L     return L;
<INITIAL,STRICT>@I     return I;
<INITIAL,STRICT>@DO     return DO;
<INITIAL,STRICT>@LO     return LO;
<INITIAL,STRICT>@IO     return IO;
<INITIAL,STRICT>@DP     return DP;
<INITIAL,STRICT>@LP     return LP;
<INITIAL,STRICT>@IP     return IP;
<INITIAL,STRICT>@DPO     return DPO;
<INITIAL,STRICT>@LPO     return LPO;
<INITIAL,STRICT>@IPO     return IPO;
<INITIAL,STRICT>@dyn   return DYN;
<INITIAL,STRICT>@lex   return LEX;
<INITIAL,STRICT>@imm   return IMM;
<INITIAL,STRICT>@dyno   return DYNO;
<INITIAL,STRICT>@lexo   return LEXO;
<INITIAL,STRICT>@immo   return IMMO;
<INITIAL,STRICT>@loc   return LOC;
<INITIAL,STRICT>@if   return IF;
<INITIAL,STRICT>@else   return ELSE;
<INITIAL,STRICT>@elseif   return ELSEIF;
<INITIAL,STRICT>@endif   return ENDIF;
<INITIAL,STRICT>@for   return FOR;
<INITIAL,STRICT>@fordict  return FORDICT;
<INITIAL,STRICT>@fordictprev  return FORDICTPREV;
<INITIAL,STRICT>@endfor   return ENDFOR;
<INITIAL,STRICT>@while return WHILE;
<INITIAL,STRICT>@endwhile return ENDWHILE;
<INITIAL,STRICT>@break return BREAK;
<INITIAL,STRICT>@continue return CONTINUE;
<INITIAL,STRICT>@return  return RETURN;
<INITIAL,STRICT>@append   return APPEND;
<INITIAL,STRICT>@dictnext return DICTNEXT;
<INITIAL,STRICT>@dictprev return DICTPREV;
<INITIAL,STRICT>@appendlist   return APPEND_LIST;
<INITIAL,STRICT>@dynluacall  return DYNLUACALL;
<INITIAL,STRICT>@lexluacall  return LEXLUACALL;

<INITIAL,STRICT>@[?] return ATQM;
<INITIAL,STRICT>@scope return SCOPE;
<INITIAL,STRICT>@true return TRUE;
<INITIAL,STRICT>@type return TYPE;
<INITIAL,STRICT>@false return FALSE;
<INITIAL,STRICT>@nil return NIL;
<INITIAL,STRICT>@strfromchr return STR_FROMCHR;
<INITIAL,STRICT>@strlower return STR_LOWER;
<INITIAL,STRICT>@strupper return STR_UPPER;
<INITIAL,STRICT>@strreverse return STR_REVERSE;
<INITIAL,STRICT>@strcmp return STRCMP;
<INITIAL,STRICT>@strstr return STRSTR;
<INITIAL,STRICT>@strrep return STRREP;
<INITIAL,STRICT>@strlistjoin return STRLISTJOIN;
<INITIAL,STRICT>@strstrip return STRSTRIP;
<INITIAL,STRICT>@strsub return STRSUB;
<INITIAL,STRICT>@strgsub return STRGSUB;
<INITIAL,STRICT>@strset return STRSET;
<INITIAL,STRICT>@strword return STRWORD;
<INITIAL,STRICT>@strwordcnt return STRWORDCNT;
<INITIAL,STRICT>@strwordlist return STRWORDLIST;
<INITIAL,STRICT>@abs return ABS;
<INITIAL,STRICT>@acos return ACOS;
<INITIAL,STRICT>@asin return ASIN;
<INITIAL,STRICT>@atan return ATAN;
<INITIAL,STRICT>@ceil return CEIL;
<INITIAL,STRICT>@cos return COS;
<INITIAL,STRICT>@sin return SIN;
<INITIAL,STRICT>@tan return TAN;
<INITIAL,STRICT>@exp return EXP;
<INITIAL,STRICT>@log return LOG;
<INITIAL,STRICT>@sqrt return SQRT;
<INITIAL,STRICT>@dupnonrec return DUP_NONRECURSIVE;
<INITIAL,STRICT>@pbnew return PB_NEW;
<INITIAL,STRICT>@tostring return TOSTRING;
<INITIAL,STRICT>@tonumber return TONUMBER;
<INITIAL,STRICT>@scopeparent return SCOPE_PARENT;
<INITIAL,STRICT>@scopenew return SCOPE_NEW;
<INITIAL,STRICT>@getscopedyn return GETSCOPE_DYN;
<INITIAL,STRICT>@getscopelex return GETSCOPE_LEX;
<INITIAL,STRICT>@beginscope return BEGINSCOPE;
<INITIAL,STRICT>@beginholeyscope return BEGINHOLEYSCOPE;
<INITIAL,STRICT>@endscope return ENDSCOPE;

<INITIAL,STRICT>@dirup return DIRUP;
<INITIAL,STRICT>@dirdown return DIRDOWN;

<INITIAL,STRICT>@addrule   return ADD_RULE;
<INITIAL,STRICT>@adddeps   return ADD_DEPS;
<INITIAL,STRICT>@RULE_DIST   return RULE_DIST;
<INITIAL,STRICT>@RULE_PHONY   return RULE_PHONY;
<INITIAL,STRICT>@RULE_ORDINARY   return RULE_ORDINARY;
<INITIAL,STRICT>:         return COLON;
<INITIAL,STRICT>,         return COMMA;
<INITIAL,STRICT>=         return EQUALS;
<INITIAL,STRICT>&& return LOGICAL_AND;
<INITIAL,STRICT>\|\| return LOGICAL_OR;
<INITIAL,STRICT>!= return NE;
<INITIAL,STRICT>== return EQ;
<INITIAL,STRICT>\+=         return PLUSEQUALS;
<INITIAL,STRICT>\?=         return QMEQUALS;
<INITIAL,STRICT>\`=         return SHELLEQUALS;
<INITIAL,STRICT>\`\`=       return SHELLSHELLEQUALS;
<INITIAL,STRICT>\<\< return SHL;
<INITIAL,STRICT>\>\> return SHR;
<INITIAL,STRICT>\<        return LT;
<INITIAL,STRICT>\>        return GT;
<INITIAL,STRICT>[(]       return OPEN_PAREN;
<INITIAL,STRICT>[)]       return CLOSE_PAREN;
<INITIAL,STRICT>[[]       return OPEN_BRACKET;
<INITIAL,STRICT>[]]       return CLOSE_BRACKET;
<INITIAL,STRICT>[{]       return OPEN_BRACE;
<INITIAL,STRICT>[}]       return CLOSE_BRACE;
<INITIAL,STRICT>\/ return DIV;
<INITIAL,STRICT>\* return MUL;
<INITIAL,STRICT>\+ return ADD;
<INITIAL,STRICT>- return SUB;
<INITIAL,STRICT>& return BITWISE_AND;
<INITIAL,STRICT>\| return BITWISE_OR;
<INITIAL,STRICT>~ return BITWISE_NOT;
<INITIAL,STRICT>\^ return BITWISE_XOR;
<INITIAL,STRICT>! return LOGICAL_NOT;
<INITIAL,STRICT>% return MOD;
<INITIAL,STRICT>\>= return GE;
<INITIAL,STRICT>\<= return LE;
<INITIAL,STRICT>\<\> return MAYBE_CALL;
<INITIAL,STRICT>[.] return PERIOD;

<INITIAL,STRICT>@\t {
  if (yycolumn == 3 && (yyextra&1) != 0 && (yyextra&2) == 0)
  {
    return ATTAB;
  }
  else
  {
    return AT;
  }
}
<INITIAL,STRICT>@@\t {
  if (yycolumn == 4 && (yyextra&1) != 0 && (yyextra&2) == 0)
  {
    return ATATTAB;
  }
  else
  {
    // Ouch. We should return two tokens. However, this is not valid.
    return ERROR_TOK;
  }
}


<INITIAL,STRICT>[$]^ {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
<INITIAL,STRICT>[$][+] {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
<INITIAL,STRICT>[$][|] {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
<INITIAL,STRICT>[$]< {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
<INITIAL,STRICT>[$]@ {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
<INITIAL,STRICT>[$][*] {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
<INITIAL,STRICT>[$][A-Za-z0-9_]+ {
  yylval->s = strdup(yytext+1);
  return VARREF_LITERAL;
}
<INITIAL,STRICT>[$][(][A-Za-z0-9_]+[)] {
  yylval->s = strdup(yytext+2);
  yylval->s[strlen(yylval->s)-1] = '\0';
  return VARREF_LITERAL;
}
<INITIAL,STRICT>[0-9]+([.][0-9]+)?([eE][-+]?[0-9]+)? {
  // Original regexp was: [-+]?[0-9]+([.][0-9]+)?([eE][-+]?[0-9]+)?
  // The sign from original regexp was removed to allow $a=$a+1 to work
  yylval->d = atof(yytext);
  return NUMBER;
}
<INITIAL>[A-Za-z0-9_.]([A-Za-z0-9_./]*[A-Za-z0-9_.])? {
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

<INITIAL,STRICT>\"([^\\\"]|\\.)*\"  yylval->str=yy_escape_string(yytext); return STRING_LITERAL;
<INITIAL,STRICT>\'([^\\\']|\\.)*\'  yylval->str=yy_escape_string_single(yytext); return STRING_LITERAL;
<INITIAL,STRICT>#[^\n]*  /* ignore */;

<INITIAL,STRICT>"%lua{"             { yy_push_state(PERCENTLUA, yyscanner); luaSnippet.capacity = 0; luaSnippet.len = 0; luaSnippet.data = NULL; }
<PERCENTLUA>%\}  {
                     yy_pop_state(yyscanner);
                     yylval->s = luaSnippet.data;
                     luaSnippet.data = NULL;
                     luaSnippet.capacity = 0;
                     luaSnippet.len = 0;
                     return PERCENTLUA_LITERAL;
                 }
<PERCENTLUA>.             { csadd(&luaSnippet, yytext[0]); }
<PERCENTLUA>"\n"          { csadd(&luaSnippet, yytext[0]); ++yylineno; yycolumn=1; }

<INITIAL,STRICT>\n                                { ++yylineno; yycolumn=1; yyextra &= (~2); return NEWLINE; }
<INITIAL,STRICT>\\\n                              { ++yylineno; yycolumn=1; yyextra |= 2; }
<INITIAL,STRICT>\t {
  if (yycolumn == 2 && (yyextra&1) != 0 && (yyextra&2) == 0)
  {
    yy_push_state(SHELL, yyscanner);
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
  (void)yy_top_state(yyscanner); // so that it's not unused
  yy_pop_state(yyscanner);
  yyextra &= (~2);
  return NEWLINE;
}
<INITIAL,STRICT>[ \t]                            /* ignore whitespaces */;
<INITIAL,STRICT>.            return ERROR_TOK;


%%
