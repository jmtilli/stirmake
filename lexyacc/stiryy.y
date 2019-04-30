%code requires {
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void *yyscan_t;
#endif
#include "stiryy.h"
#include <sys/types.h>
}

%define api.prefix {stiryy}

%{

#include "stiryy.h"
#include "yyutils.h"
#include "stiryy.tab.h"
#include "stiryy.lex.h"
#include <arpa/inet.h>

void stiryyerror(YYLTYPE *yylloc, yyscan_t scanner, struct stiryy *stiryy, const char *str)
{
        fprintf(stderr, "error: %s at line %d col %d\n",str, yylloc->first_line, yylloc->first_column);
}

int stiryywrap(yyscan_t scanner)
{
        return 1;
}

%}

%pure-parser
%lex-param {yyscan_t scanner}
%parse-param {yyscan_t scanner}
%parse-param {struct stiryy *stiryy}
%locations

%union {
  int i;
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

%destructor { free ($$.str); } STRING_LITERAL
%destructor { free ($$); } FREEFORM_TOKEN

%token PERCENTC_LITERAL
%token STATEINCLUDE
%token PARSERINCLUDE
%token INITINCLUDE
%token HDRINCLUDE
%token BYTESSIZETYPE
%token OPEN_BRACKET
%token CLOSE_BRACKET
%token OPEN_BRACE
%token CLOSE_BRACE
%token OPEN_PAREN
%token CLOSE_PAREN

%token SHELL_COMMAND

%token TOKEN
%token ACTION
%token PRIO
%token DIRECTIVE
%token NOFASTPATH
%token SHORTCUTTING
%token MAIN

%token BYTES
%token NEWLINE

%token PARSERNAME
%token EQUALS
%token PLUSEQUALS
%token COLON
%token COMMA
%token STRING_LITERAL
%token INT_LITERAL
%token VARREF_LITERAL
%token FREEFORM_TOKEN
%token LT
%token GT
%token PIPE
%token MINUS
%token CB
%token COND
%token I
%token AT
%token FUNCTION
%token ENDFUNCTION
%token LOCVAR

%token DELAYVAR
%token DELAYEXPR
%token DELAYLISTEXPAND
%token SUFFILTER
%token SUFSUBONE
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
%token LOC
%token APPEND
%token APPEND_LIST
%token RETURN
%token ADD_RULE
%token RULE_DIST
%token RULE_PHONY
%token RULE_ORDINARY


%token ERROR_TOK

%type<str> STRING_LITERAL
%type<s> FREEFORM_TOKEN
%type<s> VARREF_LITERAL
%type<s> SHELL_COMMAND

%%

stirrules:
| stirrules stirrule
| stirrules NEWLINE
| stirrules assignrule
| stirrules FILEINCLUDE STRING_LITERAL
{
  free($3.str);
}
| stirrules FILEINCLUDE VARREF_LITERAL
| stirrules DIRINCLUDE STRING_LITERAL
{
  free($3.str);
}
| stirrules DIRINCLUDE VARREF_LITERAL
| stirrules CDEPINCLUDESCURDIR VARREF_LITERAL
| stirrules CDEPINCLUDESCURDIR STRING_LITERAL
{
  free($3.str);
}
| stirrules FUNCTION FREEFORM_TOKEN OPEN_PAREN CLOSE_PAREN NEWLINE
{ free($3); }
  funlines
  ENDFUNCTION NEWLINE
;

funlines:
  locvarlines
  bodylines
;

locvarlines:
| locvarlines LOCVAR FREEFORM_TOKEN EQUALS expr NEWLINE
{
  free($3);
}
;

bodylines:
| bodylines statement
;

statement:
  lvalue EQUALS expr NEWLINE
| RETURN expr NEWLINE
| ADD_RULE OPEN_PAREN expr CLOSE_PAREN NEWLINE
| expr NEWLINE
;

lvalue:
  VARREF_LITERAL
| VARREF_LITERAL OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET
{
  free($3.str);
}
;

assignrule:
  FREEFORM_TOKEN EQUALS expr
{
  printf("Assigning to %s\n", $1);
}
| FREEFORM_TOKEN PLUSEQUALS expr
{
  printf("Plus-assigning to %s\n", $1);
}
;

value:
  STRING_LITERAL
| VARREF_LITERAL
| dict
| list
| DELAYVAR OPEN_PAREN VARREF_LITERAL CLOSE_PAREN
| DELAYLISTEXPAND OPEN_PAREN expr CLOSE_PAREN
| DELAYEXPR OPEN_PAREN expr CLOSE_PAREN
;

expr:
  OPEN_PAREN expr CLOSE_PAREN
| dict
| list
| VARREF_LITERAL
| STRING_LITERAL
| DYN OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET
{ free($3.str); }
| LEX OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET
{ free($3.str); }
| IMM OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET
{ free($3.str); }
| DYNO OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET
{ free($3.str); }
| LEXO OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET
{ free($3.str); }
| IMMO OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET
{ free($3.str); }
| LOC OPEN_BRACKET STRING_LITERAL CLOSE_BRACKET
{ free($3.str); }
| SUFSUBONE OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
| SUFSUB OPEN_PAREN expr COMMA expr COMMA expr CLOSE_PAREN
| SUFFILTER OPEN_PAREN expr COMMA expr CLOSE_PAREN
| APPEND OPEN_PAREN expr COMMA expr CLOSE_PAREN
| APPEND_LIST OPEN_PAREN expr COMMA expr CLOSE_PAREN
| RULE_DIST
| RULE_PHONY
| RULE_ORDINARY
;

list:
OPEN_BRACKET maybe_valuelist CLOSE_BRACKET
;

dict:
OPEN_BRACE maybe_dictlist CLOSE_BRACE
;

maybe_dictlist:
| dictlist
;

dictlist:
  dictentry
| dictlist COMMA dictentry
;

dictentry:
  STRING_LITERAL COLON value
;

maybe_valuelist:
| valuelist
;

valuelist:
  valuelistentry
| valuelist COMMA valuelistentry
;

valuelistentry:
  AT VARREF_LITERAL
| value;

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
  printf("\tshell %s\n", $1);
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
  printf("target1 %s\n", $1);
}
| STRING_LITERAL
{
  printf("target1 %s\n", $1.str);
}
| VARREF_LITERAL
{
  printf("target1ref\n");
}
| targets FREEFORM_TOKEN
{
  printf("target %s\n", $2);
}
| targets STRING_LITERAL
{
  printf("target %s\n", $2.str);
}
| targets VARREF_LITERAL
{
  printf("targetref\n");
}
;

deps:
| deps FREEFORM_TOKEN
{
  printf("dep %s\n", $2);
}
| deps STRING_LITERAL
{
  printf("dep %s\n", $2.str);
}
| deps VARREF_LITERAL
{
  printf("depref\n");
}
;
