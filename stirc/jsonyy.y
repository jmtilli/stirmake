/*
%code requires {
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void *yyscan_t;
#endif
#include "jsonyy.h"
#include <sys/types.h>
}

%define api.prefix {jsonyy}
*/

%{

/*
#define YYSTYPE JSONYYSTYPE
#define YYLTYPE JSONYYLTYPE
*/

#include "jsonyy.h"
#include "jsonyyutils.h"
#include "jsonyy.tab.h"
#include "jsonyy.lex.h"
#include <arpa/inet.h>

void jsonyyerror(/*YYLTYPE *yylloc, */yyscan_t scanner, struct jsonyy *jsonyy, const char *str)
{
        //fprintf(stderr, "JSON error: %s at line %d\n", str, jsonyyget_lineno(scanner));
}

int jsonyywrap(yyscan_t scanner)
{
        return 1;
}

%}

%pure-parser
%lex-param {yyscan_t scanner}
%parse-param {yyscan_t scanner}
%parse-param {struct jsonyy *jsonyy}

%union {
  char *s;
  double d;
  struct json_escaped_string str;
}

%token TRUE FALSE NIL
%token COLON COMMA
%token OPEN_BRACE CLOSE_BRACE
%token OPEN_BRACKET CLOSE_BRACKET
%token <d> NUMBER
%token <str> STRING_LITERAL

%token ERROR_TOK

%start st

%%

st: json ;

json:
  NUMBER
{
  abce_push_double(jsonyy->abce, $1);
}
| STRING_LITERAL
{
  struct abce_mb mb;
  mb = abce_mb_create_string(jsonyy->abce, $1.str, $1.sz);
  if (mb.typ == ABCE_T_N)
  {
    YYABORT;
  }
  abce_push_mb(jsonyy->abce, &mb);
  abce_mb_refdn(jsonyy->abce, &mb);
}
| object
| array
| TRUE
| FALSE
| NIL
;

object:
  OPEN_BRACE
{
  struct abce_mb mb = abce_mb_create_tree(jsonyy->abce);
  if (mb.typ == ABCE_T_N)
  {
    YYABORT;
  }
  abce_push_mb(jsonyy->abce, &mb);
  abce_mb_refdn(jsonyy->abce, &mb);
}
  maybe_objlist CLOSE_BRACE ;

array:
  OPEN_BRACKET
{
  struct abce_mb mb = abce_mb_create_array(jsonyy->abce);
  if (mb.typ == ABCE_T_N)
  {
    YYABORT;
  }
  abce_push_mb(jsonyy->abce, &mb);
  abce_mb_refdn(jsonyy->abce, &mb);
}
  maybe_jsonlist CLOSE_BRACKET ;

maybe_jsonlist:
| jsonlist;

maybe_objlist:
| objlist;

jsonlist:
  listentry
| jsonlist COMMA listentry
;

listentry:
  json
{
  struct abce_mb mb, mbar;
  if (abce_getmb(&mb, jsonyy->abce, -1) != 0)
  {
    YYABORT;
  }
  if (abce_getmb(&mbar, jsonyy->abce, -2) != 0)
  {
    abce_mb_refdn(jsonyy->abce, &mb);
    YYABORT;
  }
  if (abce_mb_array_append(jsonyy->abce, &mbar, &mb) != 0)
  {
    abce_mb_refdn(jsonyy->abce, &mb);
    abce_mb_refdn(jsonyy->abce, &mbar);
    YYABORT;
  }
  abce_pop(jsonyy->abce);
  abce_mb_refdn(jsonyy->abce, &mb);
  abce_mb_refdn(jsonyy->abce, &mbar);
}
;

objlist:
  objentry
| objlist COMMA objentry
;

objentry:
  STRING_LITERAL
{
  struct abce_mb mb;
  mb = abce_mb_create_string(jsonyy->abce, $1.str, $1.sz);
  if (mb.typ == ABCE_T_N)
  {
    YYABORT;
  }
  abce_push_mb(jsonyy->abce, &mb);
  abce_mb_refdn(jsonyy->abce, &mb);
}
  COLON json
{
  struct abce_mb mb, mbkey, mbt;
  if (abce_getmb(&mb, jsonyy->abce, -1) != 0)
  {
    YYABORT;
  }
  if (abce_getmb(&mbkey, jsonyy->abce, -2) != 0)
  {
    abce_mb_refdn(jsonyy->abce, &mb);
    YYABORT;
  }
  if (abce_getmb(&mbt, jsonyy->abce, -3) != 0)
  {
    abce_mb_refdn(jsonyy->abce, &mbkey);
    abce_mb_refdn(jsonyy->abce, &mb);
    YYABORT;
  }
  if (abce_tree_set_str(jsonyy->abce, &mbt, &mbkey, &mb) != 0)
  {
    abce_mb_refdn(jsonyy->abce, &mbt);
    abce_mb_refdn(jsonyy->abce, &mbkey);
    abce_mb_refdn(jsonyy->abce, &mb);
    YYABORT;
  }
  abce_pop(jsonyy->abce);
  abce_pop(jsonyy->abce);
  abce_mb_refdn(jsonyy->abce, &mb);
  abce_mb_refdn(jsonyy->abce, &mbkey);
  abce_mb_refdn(jsonyy->abce, &mbt);
}
;
