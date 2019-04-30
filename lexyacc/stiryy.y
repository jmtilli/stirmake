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
%token COLON
%token COMMA
%token STRING_LITERAL
%token INT_LITERAL
%token FREEFORM_TOKEN
%token LT
%token GT
%token PIPE
%token MINUS
%token CB
%token COND
%token I


%token ERROR_TOK

%type<str> STRING_LITERAL
%type<s> FREEFORM_TOKEN
%type<s> SHELL_COMMAND

%%

stirrules:
| stirrules stirrule
| stirrules NEWLINE
| stirrules assignrule
;

assignrule:
  FREEFORM_TOKEN EQUALS value
;

value:
  STRING_LITERAL
| dict
| list
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
  value
| valuelist COMMA value
;

stirrule:
  targetspec COLON depspec NEWLINE
  shell_commands
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
| targets FREEFORM_TOKEN
{
  printf("target %s\n", $2);
}
;

deps:
| deps FREEFORM_TOKEN
{
  printf("dep %s\n", $2);
}
;
