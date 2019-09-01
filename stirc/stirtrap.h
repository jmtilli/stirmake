#ifndef _STIRTRAP_H_
#define _STIRTRAP_H_

struct scope_ud {
  char *prefix;
  char *prjprefix;
};

int stir_trap(void **pbaton, uint16_t ins, unsigned char *addcode, size_t addsz);

enum {
  STIR_E_RULECHANGE_NOT_PERMITTED = 0x1001,
  STIR_E_SUFFIX_NOT_FOUND = 0x1002,
  STIR_E_DIR_NOT_FOUND = 0x1003,
  STIR_E_GLOB_FAILED = 0x1004,
  STIR_E_BAD_JSON = 0x1005,
  STIR_E_RULE_NOT_FOUND = 0x1006,
  STIR_E_TARGETS_MISSING = 0x1007,
  STIR_E_NAME_MISSING = 0x1008,
  STIR_E_NO_SHELLARG = 0x1009,
  STIR_E_RULE_FROM_WITHIN_MIDRULE = 0x1010,
  STIR_E_INSANE_RULE = 0x1011,
};

#endif
