/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn9k_worker.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)                             \
	uint16_t __rte_hot cn9k_sso_hws_deq_##name(                            \
		void *port, struct rte_event *ev, uint64_t timeout_ticks)      \
	{                                                                      \
		struct cn9k_sso_hws *ws = port;                                \
									       \
		RTE_SET_USED(timeout_ticks);                                   \
									       \
		if (ws->swtag_req) {                                           \
			ws->swtag_req = 0;                                     \
			cnxk_sso_hws_swtag_wait(ws->base + SSOW_LF_GWS_TAG);   \
			return 1;                                              \
		}                                                              \
									       \
		return cn9k_sso_hws_get_work(ws, ev, flags, ws->lookup_mem);   \
	}                                                                      \
									       \
	uint16_t __rte_hot cn9k_sso_hws_deq_seg_##name(                        \
		void *port, struct rte_event *ev, uint64_t timeout_ticks)      \
	{                                                                      \
		struct cn9k_sso_hws *ws = port;                                \
									       \
		RTE_SET_USED(timeout_ticks);                                   \
									       \
		if (ws->swtag_req) {                                           \
			ws->swtag_req = 0;                                     \
			cnxk_sso_hws_swtag_wait(ws->base + SSOW_LF_GWS_TAG);   \
			return 1;                                              \
		}                                                              \
									       \
		return cn9k_sso_hws_get_work(                                  \
			ws, ev, flags | NIX_RX_MULTI_SEG_F, ws->lookup_mem);   \
	}

NIX_RX_FASTPATH_MODES
#undef R
