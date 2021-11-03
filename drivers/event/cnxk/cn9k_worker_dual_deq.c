/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn9k_worker.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)                             \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_##name(                       \
		void *port, struct rte_event *ev, uint64_t timeout_ticks)      \
	{                                                                      \
		struct cn9k_sso_hws_dual *dws = port;                          \
		uint16_t gw;                                                   \
									       \
		RTE_SET_USED(timeout_ticks);                                   \
		if (dws->swtag_req) {                                          \
			dws->swtag_req = 0;                                    \
			cnxk_sso_hws_swtag_wait(dws->base[!dws->vws] +         \
						SSOW_LF_GWS_TAG);              \
			return 1;                                              \
		}                                                              \
									       \
		gw = cn9k_sso_hws_dual_get_work(                               \
			dws->base[dws->vws], dws->base[!dws->vws], ev, flags,  \
			dws->lookup_mem, dws->tstamp);                         \
		dws->vws = !dws->vws;                                          \
		return gw;                                                     \
	}                                                                      \
									       \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_seg_##name(                   \
		void *port, struct rte_event *ev, uint64_t timeout_ticks)      \
	{                                                                      \
		struct cn9k_sso_hws_dual *dws = port;                          \
		uint16_t gw;                                                   \
									       \
		RTE_SET_USED(timeout_ticks);                                   \
		if (dws->swtag_req) {                                          \
			dws->swtag_req = 0;                                    \
			cnxk_sso_hws_swtag_wait(dws->base[!dws->vws] +         \
						SSOW_LF_GWS_TAG);              \
			return 1;                                              \
		}                                                              \
									       \
		gw = cn9k_sso_hws_dual_get_work(                               \
			dws->base[dws->vws], dws->base[!dws->vws], ev, flags,  \
			dws->lookup_mem, dws->tstamp);                         \
		dws->vws = !dws->vws;                                          \
		return gw;                                                     \
	}

NIX_RX_FASTPATH_MODES
#undef R
