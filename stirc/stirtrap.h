#ifndef _STIRTRAP_H_
#define _STIRTRAP_H_

struct scope_ud {
  char *prefix;
  char *prjprefix;
};

int stir_trap(void **pbaton, uint16_t ins, unsigned char *addcode, size_t addsz);

enum {
  STIR_E_RULECHANGE_NOT_PERMITTED = 0x1001,
};

#endif
