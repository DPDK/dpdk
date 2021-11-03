/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn9k_worker.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)                             \
	uint16_t __rte_hot cn9k_sso_hws_deq_tmo_##name(                        \
		void *port, struct rte_event *ev, uint64_t timeout_ticks)      \
	{                                                                      \
		struct cn9k_sso_hws *ws = port;                                \
		uint16_t ret = 1;                                              \
		uint64_t iter;                                                 \
									       \
		if (ws->swtag_req) {                                           \
			ws->swtag_req = 0;                                     \
			cnxk_sso_hws_swtag_wait(ws->base + SSOW_LF_GWS_TAG);   \
			return ret;                                            \
		}                                                              \
									       \
		ret = cn9k_sso_hws_get_work(ws, ev, flags, ws->lookup_mem);    \
		for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)     \
			ret = cn9k_sso_hws_get_work(ws, ev, flags,             \
						    ws->lookup_mem);           \
									       \
		return ret;                                                    \
	}                                                                      \
									       \
	uint16_t __rte_hot cn9k_sso_hws_deq_tmo_burst_##name(                  \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks)                                        \
	{                                                                      \
		RTE_SET_USED(nb_events);                                       \
									       \
		return cn9k_sso_hws_deq_tmo_##name(port, ev, timeout_ticks);   \
	}                                                                      \
									       \
	uint16_t __rte_hot cn9k_sso_hws_deq_tmo_seg_##name(                    \
		void *port, struct rte_event *ev, uint64_t timeout_ticks)      \
	{                                                                      \
		struct cn9k_sso_hws *ws = port;                                \
		uint16_t ret = 1;                                              \
		uint64_t iter;                                                 \
									       \
		if (ws->swtag_req) {                                           \
			ws->swtag_req = 0;                                     \
			cnxk_sso_hws_swtag_wait(ws->base + SSOW_LF_GWS_TAG);   \
			return ret;                                            \
		}                                                              \
									       \
		ret = cn9k_sso_hws_get_work(ws, ev, flags, ws->lookup_mem);    \
		for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)     \
			ret = cn9k_sso_hws_get_work(ws, ev, flags,             \
						    ws->lookup_mem);           \
									       \
		return ret;                                                    \
	}                                                                      \
									       \
	uint16_t __rte_hot cn9k_sso_hws_deq_tmo_seg_burst_##name(              \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks)                                        \
	{                                                                      \
		RTE_SET_USED(nb_events);                                       \
									       \
		return cn9k_sso_hws_deq_tmo_seg_##name(port, ev,               \
						       timeout_ticks);         \
	}

NIX_RX_FASTPATH_MODES
#undef R
