/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn9k_worker.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)                             \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_burst_##name(                 \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks)                                        \
	{                                                                      \
		RTE_SET_USED(nb_events);                                       \
									       \
		return cn9k_sso_hws_dual_deq_##name(port, ev, timeout_ticks);  \
	}                                                                      \
									       \
	uint16_t __rte_hot cn9k_sso_hws_dual_deq_seg_burst_##name(             \
		void *port, struct rte_event ev[], uint16_t nb_events,         \
		uint64_t timeout_ticks)                                        \
	{                                                                      \
		RTE_SET_USED(nb_events);                                       \
									       \
		return cn9k_sso_hws_dual_deq_seg_##name(port, ev,              \
							timeout_ticks);        \
	}

NIX_RX_FASTPATH_MODES
#undef R
