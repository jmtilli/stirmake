#include <stdint.h>
#include "abce/abce.h"
#include "abce/abcetrees.h"
#include "stiropcodes.h"
#include "stirtrap.h"
#include "canon.h"
#include "stiryy.h"

#define VERIFYMB(idx, type) \
  if(1) { \
    int _getdbl_rettmp = abce_verifymb(abce, (idx), (type)); \
    if (_getdbl_rettmp != 0) \
    { \
      ret = _getdbl_rettmp; \
      break; \
    } \
  }

#define GETGENERIC(fun, val, idx) \
  if(1) { \
    int _getgen_rettmp = fun((val), abce, (idx)); \
    if (_getgen_rettmp != 0) \
    { \
      ret = _getgen_rettmp; \
      break; \
    } \
  }

#define GETMB(mb, idx) GETGENERIC(abce_getmb, mb, idx)
#define GETMBAR(mb, idx) GETGENERIC(abce_getmbar, mb, idx)

/*
  Planned argument for STIR_OPCODE_RULE_ADD:
{
  "tgts": [ // "name" has no default
    {"name": "a"},
    {"name": "b"}
  ],
  "deps": [ // default: []
    // default for all: false except "name" has no default
    // "rec" and "orderonly" can't be set simultaneously
    {"name": "c", "rec": false, "orderonly": false},
    {"name": "d", "rec": false, "orderonly": false}
  ],
  "attrs": { // default for all: false, default for "attrs": {}
    "phony": false, // if true, "rectgt", "maybe", "dist" must be false
    "rectgt": false, // if true, "maybe" must be false
    "maybe": false,
    "dist": false,
    "deponly": false, // if set, "phony"/"rectgt"/"maybe"/"dist" must be false
    // zero or one of these must be true, two being true not permitted:
    // if any of these is true, "phony" xor "deponly" must be true
    // if any of these is true, "dist" must be false
    // if any of these is true, "rectgt" must be false
    // if any of these is true, "maybe" must be false
    // if any of these is true, "tgts" must be omitted
    "iscleanhook": false,
    "isdistcleanhook": false,
    "isbothcleanhook": false,
  },
  "shells": [ // default: []
    {
      "embed": true, // default: false
      "isfun": false, // default: false
      "cmds": [["true"], ["true"]]
    },
    {
      "embed": false,
      "isfun": false,
      "cmd": ["true"]
    },
    {
      "embed": true,
      "isfun": true,
      "fun": $FNMANY,
      "arg": $FNMANYARG
    },
    {
      "embed": false,
      "isfun": true,
      "fun": $FN,
      "arg": $FNARG
    }
  ]
}
 */

int stir_trap(void **pbaton, uint16_t ins, unsigned char *addcode, size_t addsz)
{
  int ret = 0;
  size_t i;
  char *prefix, *backpath;
  struct abce *abce = ABCE_CONTAINER_OF(pbaton, struct abce, trap_baton);
  struct abce_mb mb = {};
  struct stiryy_main *main = *pbaton;
  switch (ins)
  {
    case STIR_OPCODE_SUFFILTER:
    {
      struct abce_mb suf = {};
      struct abce_mb bases = {};
      struct abce_mb mods = {};
      size_t bcnt, bsz, ssz, i;
      VERIFYMB(-1, ABCE_T_S); // suffix
      VERIFYMB(-2, ABCE_T_A); // bases
      mods = abce_mb_create_array(abce);
      if (mods.typ == ABCE_T_N)
      {
        return -ENOMEM;
      }
      if (abce_push_mb(abce, &mods) != 0)
      {
        return -EOVERFLOW;
      }
      GETMB(&suf, -2); // now the indices are different
      GETMB(&bases, -3);
      bcnt = bases.u.area->u.ar.size;
      ssz = suf.u.area->u.str.size;
      for (i = 0; i < bcnt; i++)
      {
        if (bases.u.area->u.ar.mbs[i].typ != ABCE_T_S)
        {
          abce->err.code = ABCE_E_EXPECT_STR;
          abce->err.mb = abce_mb_refup(abce, &bases.u.area->u.ar.mbs[i]);
          abce_mb_refdn(abce, &suf);
          abce_mb_refdn(abce, &bases);
          abce_mb_refdn(abce, &mods);
          return -EINVAL;
        }
        bsz = bases.u.area->u.ar.mbs[i].u.area->u.str.size;
        if (   bsz < ssz
            || memcmp(&bases.u.area->u.ar.mbs[i].u.area->u.str.buf[bsz-ssz],
                      suf.u.area->u.str.buf, ssz) != 0)
        {
          continue;
        }
        if (abce_mb_array_append(abce, &mods, &bases.u.area->u.ar.mbs[i]) != 0)
        {
          abce_mb_refdn(abce, &suf);
          abce_mb_refdn(abce, &bases);
          abce_mb_refdn(abce, &mods);
          abce_pop(abce);
          abce_pop(abce);
          abce_pop(abce);
          return -ENOMEM;
        }
      }
      abce_pop(abce);
      abce_pop(abce);
      abce_pop(abce);
      if (abce_push_mb(abce, &mods) != 0)
      {
        my_abort();
      }
      abce_mb_refdn(abce, &suf);
      abce_mb_refdn(abce, &bases);
      abce_mb_refdn(abce, &mods);
      return 0;
    }
    case STIR_OPCODE_SUFSUBALL:
    {
      struct abce_mb oldsuf = {};
      struct abce_mb newsuf = {};
      struct abce_mb bases = {};
      struct abce_mb newstr = {};
      struct abce_mb mods = {};
      size_t bcnt, bsz, osz, nsz, i;
      VERIFYMB(-1, ABCE_T_S); // newsuffix
      VERIFYMB(-2, ABCE_T_S); // oldsuffix
      VERIFYMB(-3, ABCE_T_A); // bases
      mods = abce_mb_create_array(abce);
      if (mods.typ == ABCE_T_N)
      {
        return -ENOMEM;
      }
      if (abce_push_mb(abce, &mods) != 0)
      {
        return -EOVERFLOW;
      }
      GETMB(&newsuf, -2); // now the indices are different
      GETMB(&oldsuf, -3);
      GETMB(&bases, -4);
      bcnt = bases.u.area->u.ar.size;
      osz = oldsuf.u.area->u.str.size;
      nsz = newsuf.u.area->u.str.size;
      for (i = 0; i < bcnt; i++)
      {
        if (bases.u.area->u.ar.mbs[i].typ != ABCE_T_S)
        {
          abce->err.code = ABCE_E_EXPECT_STR;
          abce->err.mb = abce_mb_refup(abce, &bases.u.area->u.ar.mbs[i]);
          abce_mb_refdn(abce, &oldsuf);
          abce_mb_refdn(abce, &newsuf);
          abce_mb_refdn(abce, &bases);
          abce_mb_refdn(abce, &mods);
          return -EINVAL;
        }
        bsz = bases.u.area->u.ar.mbs[i].u.area->u.str.size;
        if (   bsz < osz
            || memcmp(&bases.u.area->u.ar.mbs[i].u.area->u.str.buf[bsz-osz],
                      oldsuf.u.area->u.str.buf, osz) != 0)
        {
          fprintf(stderr, "stirmake: %s does not end with %s\n",
                  bases.u.area->u.ar.mbs[i].u.area->u.str.buf,
                  oldsuf.u.area->u.str.buf);
          abce->err.code = STIR_E_SUFFIX_NOT_FOUND;
          abce->err.mb = abce_mb_refup(abce, &bases.u.area->u.ar.mbs[i]);
          abce_mb_refdn(abce, &oldsuf);
          abce_mb_refdn(abce, &newsuf);
          abce_mb_refdn(abce, &bases);
          abce_mb_refdn(abce, &mods);
          return -EINVAL;
        }
      }
      for (i = 0; i < bcnt; i++)
      {
        bsz = bases.u.area->u.ar.mbs[i].u.area->u.str.size;
        newstr = abce_mb_create_string_to_be_filled(abce, bsz-osz+nsz);
        if (newstr.typ == ABCE_T_N)
        {
          abce_mb_refdn(abce, &oldsuf);
          abce_mb_refdn(abce, &newsuf);
          abce_mb_refdn(abce, &bases);
          abce_mb_refdn(abce, &mods);
          abce_pop(abce);
          abce_pop(abce);
          abce_pop(abce);
          abce_pop(abce);
          return -ENOMEM;
        }
        memcpy(newstr.u.area->u.str.buf,
               bases.u.area->u.ar.mbs[i].u.area->u.str.buf, bsz-osz);
        memcpy(newstr.u.area->u.str.buf + (bsz - osz),
               newsuf.u.area->u.str.buf, nsz);
        newstr.u.area->u.str.buf[bsz-osz+nsz] = '\0';
        if (abce_mb_array_append(abce, &mods, &newstr) != 0)
        {
          abce_mb_refdn(abce, &oldsuf);
          abce_mb_refdn(abce, &newsuf);
          abce_mb_refdn(abce, &bases);
          abce_mb_refdn(abce, &mods);
          abce_mb_refdn(abce, &newstr);
          abce_pop(abce);
          abce_pop(abce);
          abce_pop(abce);
          abce_pop(abce);
          return -ENOMEM;
        }
        abce_mb_refdn(abce, &newstr);
      }
      abce_pop(abce);
      abce_pop(abce);
      abce_pop(abce);
      abce_pop(abce);
      if (abce_push_mb(abce, &mods) != 0)
      {
        my_abort();
      }
      abce_mb_refdn(abce, &oldsuf);
      abce_mb_refdn(abce, &newsuf);
      abce_mb_refdn(abce, &bases);
      abce_mb_refdn(abce, &mods);
      return 0;
    }
    case STIR_OPCODE_SUFSUBONE:
    {
      struct abce_mb oldsuf = {};
      struct abce_mb newsuf = {};
      struct abce_mb base = {};
      struct abce_mb newstr = {};
      size_t bsz, osz, nsz;
      VERIFYMB(-1, ABCE_T_S); // newsuffix
      VERIFYMB(-2, ABCE_T_S); // oldsuffix
      VERIFYMB(-3, ABCE_T_S); // base
      GETMB(&newsuf, -1);
      GETMB(&oldsuf, -2);
      GETMB(&base, -3);
      bsz = base.u.area->u.str.size;
      osz = oldsuf.u.area->u.str.size;
      nsz = newsuf.u.area->u.str.size;
      if (   bsz < osz
          || memcmp(&base.u.area->u.str.buf[bsz-osz], oldsuf.u.area->u.str.buf,
                    osz) != 0)
      {
        fprintf(stderr, "stirmake: %s does not end with %s\n",
                base.u.area->u.str.buf, oldsuf.u.area->u.str.buf);
        abce->err.code = STIR_E_SUFFIX_NOT_FOUND;
        abce->err.mb = abce_mb_refup(abce, &base);
        abce_mb_refdn(abce, &oldsuf);
        abce_mb_refdn(abce, &newsuf);
        abce_mb_refdn(abce, &base);
        return -EINVAL;
      }
      newstr = abce_mb_create_string_to_be_filled(abce, bsz-osz+nsz);
      if (newstr.typ == ABCE_T_N)
      {
        abce_mb_refdn(abce, &oldsuf);
        abce_mb_refdn(abce, &newsuf);
        abce_mb_refdn(abce, &base);
        abce_pop(abce);
        abce_pop(abce);
        abce_pop(abce);
        return -ENOMEM;
      }
      memcpy(newstr.u.area->u.str.buf, base.u.area->u.str.buf, bsz-osz);
      memcpy(newstr.u.area->u.str.buf + (bsz - osz),
             newsuf.u.area->u.str.buf, nsz);
      newstr.u.area->u.str.buf[bsz-osz+nsz] = '\0';
      abce_pop(abce);
      abce_pop(abce);
      abce_pop(abce);
      if (abce_push_mb(abce, &newstr) != 0)
      {
        my_abort();
      }
      abce_mb_refdn(abce, &oldsuf);
      abce_mb_refdn(abce, &newsuf);
      abce_mb_refdn(abce, &base);
      abce_mb_refdn(abce, &newstr);
      return 0;
    }
    // FIXME what if there are many deps? Is it added for all rules?
    case STIR_OPCODE_DEP_ADD:
    {
      struct abce_mb depar = {};
      struct abce_mb tgtar = {};
      struct abce_mb tree = {};
      struct abce_mb orderonly = {};
      struct abce_mb rec = {};
      struct abce_mb *orderonlyres = NULL;
      struct abce_mb *recres = NULL;
      if (!main->parsing)
      {
        fprintf(stderr, "stirmake: trying to add dep after parsing stage\n");
        abce->err.code = STIR_E_RULECHANGE_NOT_PERMITTED;
        return -EINVAL;
      }
      if (abce_scope_get_userdata(&abce->dynscope))
      {
        prefix =
          ((struct scope_ud*)abce_scope_get_userdata(&abce->dynscope))->prefix;
      }
      else
      {
        prefix = ".";
      }
      VERIFYMB(-1, ABCE_T_T);
      VERIFYMB(-2, ABCE_T_A);
      VERIFYMB(-3, ABCE_T_A);
      GETMB(&tree, -1);
      GETMB(&depar, -2);
      GETMB(&tgtar, -3);
      for (i = 0; i < depar.u.area->u.ar.size; i++)
      {
        const struct abce_mb *mb = &depar.u.area->u.ar.mbs[i];
        if (mb->typ != ABCE_T_S)
        {
          abce_mb_refdn(abce, &depar);
          abce_mb_refdn(abce, &tgtar);
          abce_mb_refdn(abce, &tree);
          abce->err.code = ABCE_E_EXPECT_STR;
          abce->err.mb = abce_mb_refup(abce, mb);
          return -EINVAL;
        }
      }
      for (i = 0; i < tgtar.u.area->u.ar.size; i++)
      {
        const struct abce_mb *mb = &tgtar.u.area->u.ar.mbs[i];
        if (mb->typ != ABCE_T_S)
        {
          abce_mb_refdn(abce, &depar);
          abce_mb_refdn(abce, &tgtar);
          abce_mb_refdn(abce, &tree);
          abce->err.code = ABCE_E_EXPECT_STR;
          abce->err.mb = abce_mb_refup(abce, mb);
          return -EINVAL;
        }
      }

      rec = abce_mb_create_string_nul(abce, "rec");
      if (rec.typ == ABCE_T_N)
      {
        abce_mb_refdn(abce, &depar);
        abce_mb_refdn(abce, &tgtar);
        abce_mb_refdn(abce, &tree);
        abce_pop(abce);
        abce_pop(abce);
        abce_pop(abce);
        return -ENOMEM;
      }
      if (abce_tree_get_str(abce, &recres, &tree, &rec) != 0)
      {
        recres = NULL;
      }
      abce_mb_refdn(abce, &rec);

      orderonly = abce_mb_create_string_nul(abce, "orderonly");
      if (orderonly.typ == ABCE_T_N)
      {
        abce_mb_refdn(abce, &depar);
        abce_mb_refdn(abce, &tgtar);
        abce_mb_refdn(abce, &tree);
        abce_pop(abce);
        abce_pop(abce);
        abce_pop(abce);
        return -ENOMEM;
      }
      if (abce_tree_get_str(abce, &orderonlyres, &tree, &orderonly) != 0)
      {
        orderonlyres = NULL;
      }
      abce_mb_refdn(abce, &orderonly);
      if (recres && recres->typ != ABCE_T_B && recres->typ != ABCE_T_D)
      {
        abce->err.code = ABCE_E_EXPECT_BOOL;
        abce->err.mb = abce_mb_refup(abce, recres);
        //abce_mb_refdn(abce, &recres);
        //abce_mb_refdn(abce, &orderonlyres);
        abce_mb_refdn(abce, &depar);
        abce_mb_refdn(abce, &tgtar);
        abce_mb_refdn(abce, &tree);
        abce_pop(abce);
        abce_pop(abce);
        abce_pop(abce);
        return -EINVAL;
      }
      if (orderonlyres && orderonlyres->typ != ABCE_T_B && orderonlyres->typ != ABCE_T_D)
      {
        abce->err.code = ABCE_E_EXPECT_BOOL;
        abce->err.mb = abce_mb_refup(abce, orderonlyres);
        //abce_mb_refdn(abce, &recres);
        //abce_mb_refdn(abce, &orderonlyres);
        abce_mb_refdn(abce, &depar);
        abce_mb_refdn(abce, &tgtar);
        abce_mb_refdn(abce, &tree);
        abce_pop(abce);
        abce_pop(abce);
        abce_pop(abce);
        return -EINVAL;
      }

      stiryy_main_emplace_rule(main, prefix, abce->dynscope.u.area->u.sc.locidx);
      for (i = 0; i < tgtar.u.area->u.ar.size; i++)
      {
        const struct abce_mb *mb = &tgtar.u.area->u.ar.mbs[i];
        stiryy_main_set_tgt(main, prefix, mb->u.area->u.str.buf);
      }
      for (i = 0; i < depar.u.area->u.ar.size; i++)
      {
        const struct abce_mb *mb = &depar.u.area->u.ar.mbs[i];
        stiryy_main_set_dep(main, prefix, mb->u.area->u.str.buf, recres && recres->u.d != 0, orderonlyres && orderonlyres->u.d != 0);
      }
      stiryy_main_mark_deponly(main);

      //abce_mb_refdn(abce, &recres);
      //abce_mb_refdn(abce, &orderonlyres);
      abce_mb_refdn(abce, &depar);
      abce_mb_refdn(abce, &tgtar);
      abce_mb_refdn(abce, &tree);
      abce_pop(abce);
      abce_pop(abce);
      abce_pop(abce);
      return 0;
    }
    case STIR_OPCODE_RULE_ADD:
      if (abce_scope_get_userdata(&abce->dynscope))
      {
        prefix =
          ((struct scope_ud*)abce_scope_get_userdata(&abce->dynscope))->prefix;
      }
      else
      {
        prefix = ".";
      }
      return -EILSEQ;
    case STIR_OPCODE_TOP_DIR:
      if (abce_scope_get_userdata(&abce->dynscope))
      {
        prefix =
          ((struct scope_ud*)abce_scope_get_userdata(&abce->dynscope))->prjprefix;
      }
      else
      {
        prefix = ".";
      }
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
      if (abce_scope_get_userdata(&abce->dynscope))
      {
        prefix =
          ((struct scope_ud*)abce_scope_get_userdata(&abce->dynscope))->prjprefix;
      }
      else
      {
        prefix = ".";
      }
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
