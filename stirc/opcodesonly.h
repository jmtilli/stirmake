#ifndef _OPCODESONLY_H_
#define _OPCODESONLY_H_

enum stirbce_opcode {
  STIRBCE_OPCODE_PUSH_DBL = 1, // followed by double
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
  STIRBCE_OPCODE_RETEX = 47,
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
};

#endif
