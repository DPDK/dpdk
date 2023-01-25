/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn10k_tx.h"

#ifdef _ROC_API_H_
#error "roc_api.h is included"
#endif

#define T(name, sz, flags)                                                     \
	NIX_TX_XMIT_VEC(cn10k_nix_xmit_pkts_vec_##name, sz, flags)

NIX_TX_FASTPATH_MODES_48_63
#undef T
