#ifndef _CONST_H_
#define _CONST_H_

enum {
  RULEID_BY_TGT_SIZE = 8192,
  RULEIDS_BY_DEP_SIZE = 8192,
  STATHASH_SIZE_RB = 8192,
  STATHASH_SIZE = 256*1024,
  STATHASH_SIZE_INIT = 256,
  TGTS_SIZE = 4,
  DEPS_SIZE = 8,
  DEPS_REMAIN_SIZE = 8,
  ONE_RULEID_BY_DEP_SIZE = 8,
  ADD_DEP_SIZE = 8,
  ADD_DEPS_SIZE = 8192,
  RULEID_BY_PID_SIZE = 64,
  RULES_REMAIN_SIZE = 64,
  DB_SIZE = 8192,
  STRINGTAB_SIZE = 8192,
  MAX_JOBCNT = 1000,
};

enum {
  HASH_SEED = 0x12345678U,
};

#define STIR_LINKED_LIST_HEAD_INITER(x) { \
  .node = { \
    .prev = &(x).node, \
    .next = &(x).node, \
  }, \
}

#endif
