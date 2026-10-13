#ifndef _YYUTILS_H_
#define _YYUTILS_H_

#include <stdio.h>
#include <stdint.h>
#include "stiryy.h"

#ifdef __cplusplus
extern "C" {
#endif

int gitshas_has(const char *needle, size_t needle_len);
const char *gitversions_head(void);
const char *gitversion_get(void);
void gitversions(char *argv0);
void acceptchars_init(void);

int
engine_stringlist(struct abce *abce,
                  size_t ip,
                  const char *directive,
                  char ***strs, size_t *strsz,
                  int allow_nil,
                  int *was_array);

int stiryydoparse(FILE *filein, struct stiryy *stiryy);

#if !STIR_NO_MEMPARSE
void stiryydomemparse(char *filedata, size_t filesize, struct stiryy *stiryy);
#endif

int stiryynameparse(const char *fname, struct stiryy *stiryy, int require);

int stiryydirparse(
  const char *argv0, const char *fname, struct stiryy *stiryy, int require);

struct escaped_string yy_escape_string(char *orig, char **strendptr);

struct escaped_string yy_escape_string_single(char *orig, char **strendptr);

void file_escape_string(FILE *f, const char *str);

static inline int is_ascii_full(void)
{
  if ('\t' != 9) return 0;
  if ('\n' != 10) return 0;
  if ('\r' != 13) return 0;
  if (' ' != 32) return 0;
  if ('!' != 33) return 0;
  if ('"' != 34) return 0;
  if ('#' != 35) return 0;
  if ('$' != 36) return 0;
  if ('%' != 37) return 0;
  if ('&' != 38) return 0;
  if ('\'' != 39) return 0;
  if ('(' != 40) return 0;
  if (')' != 41) return 0;
  if ('*' != 42) return 0;
  if ('+' != 43) return 0;
  if (',' != 44) return 0;
  if ('-' != 45) return 0;
  if ('.' != 46) return 0;
  if ('/' != 47) return 0;
  if ('0' != 48) return 0;
  if ('1' != 49) return 0;
  if ('2' != 50) return 0;
  if ('3' != 51) return 0;
  if ('4' != 52) return 0;
  if ('5' != 53) return 0;
  if ('6' != 54) return 0;
  if ('7' != 55) return 0;
  if ('8' != 56) return 0;
  if ('9' != 57) return 0;
  if (':' != 58) return 0;
  if (';' != 59) return 0;
  if ('<' != 60) return 0;
  if ('=' != 61) return 0;
  if ('>' != 62) return 0;
  if ('?' != 63) return 0;
  if ('@' != 64) return 0;
  if ('A' != 65) return 0;
  if ('B' != 66) return 0;
  if ('C' != 67) return 0;
  if ('D' != 68) return 0;
  if ('E' != 69) return 0;
  if ('F' != 70) return 0;
  if ('G' != 71) return 0;
  if ('H' != 72) return 0;
  if ('I' != 73) return 0;
  if ('J' != 74) return 0;
  if ('K' != 75) return 0;
  if ('L' != 76) return 0;
  if ('M' != 77) return 0;
  if ('N' != 78) return 0;
  if ('O' != 79) return 0;
  if ('P' != 80) return 0;
  if ('Q' != 81) return 0;
  if ('R' != 82) return 0;
  if ('S' != 83) return 0;
  if ('T' != 84) return 0;
  if ('U' != 85) return 0;
  if ('V' != 86) return 0;
  if ('W' != 87) return 0;
  if ('X' != 88) return 0;
  if ('Y' != 89) return 0;
  if ('Z' != 90) return 0;
  if ('[' != 91) return 0;
  if ('\\' != 92) return 0;
  if (']' != 93) return 0;
  if ('^' != 94) return 0;
  if ('_' != 95) return 0;
  if ('`' != 96) return 0;
  if ('a' != 97) return 0;
  if ('b' != 98) return 0;
  if ('c' != 99) return 0;
  if ('d' != 100) return 0;
  if ('e' != 101) return 0;
  if ('f' != 102) return 0;
  if ('g' != 103) return 0;
  if ('h' != 104) return 0;
  if ('i' != 105) return 0;
  if ('j' != 106) return 0;
  if ('k' != 107) return 0;
  if ('l' != 108) return 0;
  if ('m' != 109) return 0;
  if ('n' != 110) return 0;
  if ('o' != 111) return 0;
  if ('p' != 112) return 0;
  if ('q' != 113) return 0;
  if ('r' != 114) return 0;
  if ('s' != 115) return 0;
  if ('t' != 116) return 0;
  if ('u' != 117) return 0;
  if ('v' != 118) return 0;
  if ('w' != 119) return 0;
  if ('x' != 120) return 0;
  if ('y' != 121) return 0;
  if ('z' != 122) return 0;
  if ('{' != 123) return 0;
  if ('|' != 124) return 0;
  if ('}' != 125) return 0;
  if ('~' != 126) return 0;
  return 1;
}

#ifdef __cplusplus
};
#endif

#endif

