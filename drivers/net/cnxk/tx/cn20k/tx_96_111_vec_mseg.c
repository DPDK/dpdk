/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include "cn20k_tx.h"

#ifdef _ROC_API_H_
#error "roc_api.h is included"
#endif

#if !defined(CNXK_DIS_TMPLT_FUNC)

#define T(name, sz, flags) NIX_TX_XMIT_VEC_MSEG(cn20k_nix_xmit_pkts_vec_mseg_##name, sz, flags)

NIX_TX_FASTPATH_MODES_96_111
#undef T

#endif
