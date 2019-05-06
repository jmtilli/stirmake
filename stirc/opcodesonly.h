#ifndef _OPCODESONLY_H_
#define _OPCODESONLY_H_

enum stirbce_opcode {
  //STIRBCE_OPCODE_ZERONUMBERED = 0,
  STIRBCE_OPCODE_PUSH_DBL = 1, // followed by double
  STIRBCE_OPCODE_STRSUB = 2,
  STIRBCE_OPCODE_STRGET = 3,
  STIRBCE_OPCODE_STRLEN = 4,
  STIRBCE_OPCODE_CALL_EQ = 5, //
  STIRBCE_OPCODE_CALL_NE = 6, //
  STIRBCE_OPCODE_CALL_LOGICAL_AND = 7, //
  STIRBCE_OPCODE_CALL_LOGICAL_OR = 8, //
  STIRBCE_OPCODE_CALL_LOGICAL_NOT = 9, //
  STIRBCE_OPCODE_CALL_BITWISE_AND = 10, //
  STIRBCE_OPCODE_CALL_BITWISE_OR = 11, //
  STIRBCE_OPCODE_CALL_BITWISE_XOR = 12, //
  STIRBCE_OPCODE_CALL_BITWISE_NOT = 13, //
  STIRBCE_OPCODE_CALL_LT = 14, //
  STIRBCE_OPCODE_CALL_GT = 15, //
  STIRBCE_OPCODE_CALL_LE = 16, //
  STIRBCE_OPCODE_CALL_GE = 17, //
  STIRBCE_OPCODE_CALL_SHL = 18, //
  STIRBCE_OPCODE_CALL_SHR = 19, //
  STIRBCE_OPCODE_CALL_ADD = 20, //
  STIRBCE_OPCODE_CALL_SUB = 21, //
  STIRBCE_OPCODE_CALL_MUL = 22, //
  STIRBCE_OPCODE_CALL_DIV = 23, //
  STIRBCE_OPCODE_CALL_MOD = 24, //
  STIRBCE_OPCODE_CALL_UNARY_MINUS = 25, //
  STIRBCE_OPCODE_IF_NOT_JMP = 26, //
  STIRBCE_OPCODE_JMP = 27, //
  STIRBCE_OPCODE_CALL = 28, //
  STIRBCE_OPCODE_RET = 29,
  STIRBCE_OPCODE_NOP = 30,
  STIRBCE_OPCODE_STR_FROMCHR = 31,
  STIRBCE_OPCODE_LISTPOP = 32,
  STIRBCE_OPCODE_DICTDEL = 33,
  STIRBCE_OPCODE_POP = 34,
  STIRBCE_OPCODE_POP_MANY = 35,
  STIRBCE_OPCODE_PUSH_STACK = 36,
  STIRBCE_OPCODE_SET_STACK = 37,
  STIRBCE_OPCODE_PUSH_NEW_ARRAY = 38,
  STIRBCE_OPCODE_PUSH_NEW_DICT = 39,
  STIRBCE_OPCODE_EXIT = 40,
  STIRBCE_OPCODE_PUSH_STRINGTAB = 41,
  STIRBCE_OPCODE_APPEND = 42,
  STIRBCE_OPCODE_STRAPPEND = 43,
  STIRBCE_OPCODE_LISTLEN = 44,
  STIRBCE_OPCODE_LISTGET = 45,
  STIRBCE_OPCODE_LISTSET = 46,
  //STIRBCE_OPCODE_RETEX = 47,
  STIRBCE_OPCODE_FUN_JMP_ADDR = 48,
  STIRBCE_OPCODE_FUN_HEADER = 49, // followed by double
  STIRBCE_OPCODE_PUSH_NIL = 50,
  STIRBCE_OPCODE_BOOLEANIFY = 51,
  STIRBCE_OPCODE_PUSH_TRUE = 52,
  STIRBCE_OPCODE_PUSH_FALSE = 53,
  STIRBCE_OPCODE_LUAPUSH = 54,
  STIRBCE_OPCODE_LUAEVAL = 55,
  STIRBCE_OPCODE_DUMP = 56,
  STIRBCE_OPCODE_GETSCOPE_DYN = 57, // get dynamic scope
  STIRBCE_OPCODE_GETSCOPE_LEX = 58, // get lexical scope by id
  STIRBCE_OPCODE_SCOPEVAR = 59, // var from scope
  STIRBCE_OPCODE_CALL_IF_FUN = 60,
  STIRBCE_OPCODE_SCOPEVAR_SET = 61, // var to scope
  STIRBCE_OPCODE_DICTLEN = 62,
  STIRBCE_OPCODE_DICTGET = 63,
  STIRBCE_OPCODE_DICTSET = 64,
  STIRBCE_OPCODE_FUNIFY = 65,
  STIRBCE_OPCODE_APPEND_MAINTAIN = 66,
  STIRBCE_OPCODE_APPENDALL_MAINTAIN = 67,
  STIRBCE_OPCODE_DUP_NONRECURSIVE = 68,
  STIRBCE_OPCODE_DICTNEXT_SAFE = 69,
  STIRBCE_OPCODE_TYPE = 70,
  STIRBCE_OPCODE_RETEX2 = 71,
  // math
  STIRBCE_OPCODE_ABS = 72,
  STIRBCE_OPCODE_ACOS = 73,
  STIRBCE_OPCODE_ASIN = 74,
  STIRBCE_OPCODE_ATAN = 75,
  STIRBCE_OPCODE_CEIL = 76,
  STIRBCE_OPCODE_FLOOR = 77,
  STIRBCE_OPCODE_COS = 78,
  STIRBCE_OPCODE_SIN = 79,
  STIRBCE_OPCODE_TAN = 80,
  STIRBCE_OPCODE_EXP = 81,
  STIRBCE_OPCODE_LOG = 82,
  STIRBCE_OPCODE_SQRT = 83,
  // str
  STIRBCE_OPCODE_STR_LOWER = 84,
  STIRBCE_OPCODE_STR_UPPER = 85,
  STIRBCE_OPCODE_STR_REVERSE = 86, // needed?
  // general
  STIRBCE_OPCODE_ERROR = 87,
  // import another file
  STIRBCE_OPCODE_IMPORT = 88,
  STIRBCE_OPCODE_LUA_IMPORT = 89,
  STIRBCE_OPCODE_DEP_IMPORT = 90,
  STIRBCE_OPCODE_SCOPE_NEW = 91,
  STIRBCE_OPCODE_TOSTRING = 92,
  STIRBCE_OPCODE_TONUMBER = 93,
  // file I/O, mainly I
  STIRBCE_OPCODE_FILE_OPEN = 94,
  STIRBCE_OPCODE_FILE_CLOSE = 95,
  STIRBCE_OPCODE_FILE_GETDELIM = 96,
  STIRBCE_OPCODE_FILE_GETN = 97, // infinity: get all
  STIRBCE_OPCODE_FILE_SEEK_TELL = 98,
  STIRBCE_OPCODE_FILE_FLUSH = 99,
  STIRBCE_OPCODE_FILE_WRITE = 100,
  // add rule or deps
  STIRBCE_OPCODE_RULE_ADD = 101,
  STIRBCE_OPCODE_DEPS_ADD = 102,
  STIRBCE_OPCODE_DICTHAS = 103,
  STIRBCE_OPCODE_SCOPE_HAS = 104,
  STIRBCE_OPCODE_SUFSUBONE = 105,
  STIRBCE_OPCODE_SUFSUBALL = 106,
  STIRBCE_OPCODE_SUFFILTER = 107,
  STIRBCE_OPCODE_GETPROJTOP = 108,
  // JSON
  STIRBCE_OPCODE_JSON_ENCODE = 109,
  STIRBCE_OPCODE_JSON_DECODE = 110,
  STIRBCE_OPCODE_PATH_SIMPLIFY = 111,
  STIRBCE_OPCODE_STRSTR = 112,
  STIRBCE_OPCODE_STRGSUB = 113,
  STIRBCE_OPCODE_STRREP = 114,
  STIRBCE_OPCODE_STRFMT = 115,
  STIRBCE_OPCODE_DIR_IMPORT = 116,
  STIRBCE_OPCODE_PROJ_IMPORT = 117,
  STIRBCE_OPCODE_MEMFILE_IOPEN = 118,
  STIRBCE_OPCODE_STRSET = 119,
};

#endif
