#include <stdint.h>
#include "abce/abce.h"
#include "stiropcodes.h"
#include "canon.h"

int stir_trap(void **pbaton, uint16_t ins, unsigned char *addcode, size_t addsz)
{
  int ret = 0;
  char *prefix, *backpath;
  struct abce *abce = ABCE_CONTAINER_OF(pbaton, struct abce, trap_baton);
  struct abce_mb mb = {};
  switch (ins)
  {
    case STIR_OPCODE_TOP_DIR:
      prefix = abce_scope_get_userdata(&abce->dynscope);
      if (prefix == NULL)
      {
        prefix = ".";
      }
      backpath = construct_backpath(prefix);
      mb = abce_mb_create_string(abce, backpath, strlen(backpath));
      free(backpath);
      if (mb.typ == ABCE_T_N)
      {
        return -ENOMEM;
      }
      if (abce_push_mb(abce, &mb) != 0)
      {
        abce->err.code = ABCE_E_STACK_OVERFLOW;
        abce->err.mb = abce_mb_refup_noinline(abce, &mb);
        return -EOVERFLOW;
      }
      abce_mb_refdn(abce, &mb);
      return 0;
    case STIR_OPCODE_CUR_DIR_FROM_TOP:
      prefix = abce_scope_get_userdata(&abce->dynscope);
      if (prefix == NULL)
      {
        prefix = ".";
      }
      mb = abce_mb_create_string(abce, prefix, strlen(prefix));
      if (mb.typ == ABCE_T_N)
      {
        return -ENOMEM;
      }
      if (abce_push_mb(abce, &mb) != 0)
      {
        abce->err.code = ABCE_E_STACK_OVERFLOW;
        abce->err.mb = abce_mb_refup_noinline(abce, &mb);
        return -EOVERFLOW;
      }
      abce_mb_refdn(abce, &mb);
      return 0;
    default:
      abce->err.code = ABCE_E_UNKNOWN_INSTRUCTION;
      abce->err.mb.typ = ABCE_T_D;
      abce->err.mb.u.d = ins;
      return -EILSEQ;
  }
  return ret;
}
