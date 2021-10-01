/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn9k_worker.h"

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)                         \
	uint16_t __rte_hot cn9k_sso_hws_tx_adptr_enq_##name(                   \
		void *port, struct rte_event ev[], uint16_t nb_events)         \
	{                                                                      \
		struct cn9k_sso_hws *ws = port;                                \
		uint64_t cmd[sz];                                              \
									       \
		RTE_SET_USED(nb_events);                                       \
		return cn9k_sso_hws_event_tx(                                  \
			ws->base, &ev[0], cmd,                                 \
			(const uint64_t(*)[RTE_MAX_QUEUES_PER_PORT]) &         \
				ws->tx_adptr_data,                             \
			flags);                                                \
	}

NIX_TX_FASTPATH_MODES
#undef T
