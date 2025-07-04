/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn10k_worker.h"

#ifdef _ROC_API_H_
#error "roc_api.h is included"
#endif

#if !defined(CNXK_DIS_TMPLT_FUNC)

#define R(name, flags)                                                         \
	SSO_CMN_DEQ_BURST(cn10k_sso_hws_deq_tmo_burst_##name,                  \
			  cn10k_sso_hws_deq_tmo_##name, flags)                 \
	SSO_CMN_DEQ_BURST(cn10k_sso_hws_reas_deq_tmo_burst_##name,             \
			  cn10k_sso_hws_reas_deq_tmo_##name, flags | NIX_RX_REAS_F)

NIX_RX_FASTPATH_MODES_0_15
#undef R

#endif
