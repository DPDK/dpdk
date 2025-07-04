/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn10k_tx_worker.h"

#ifdef _ROC_API_H_
#error "roc_api.h is included"
#endif

#if !defined(CNXK_DIS_TMPLT_FUNC)

#define T(name, sz, flags) SSO_TX(cn10k_sso_hws_tx_adptr_enq_##name, sz, flags)

NIX_TX_FASTPATH_MODES_48_63
#undef T

#endif
