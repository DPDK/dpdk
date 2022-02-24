/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn10k_worker.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

#define R(name, flags)                                                         \
	SSO_DUAL_DEQ(cn10k_sso_hws_dual_deq_##name, flags)                     \
	SSO_DUAL_DEQ(cn10k_sso_hws_reas_dual_deq_##name, flags | NIX_RX_REAS_F)

NIX_RX_FASTPATH_MODES_48_63
#undef R
