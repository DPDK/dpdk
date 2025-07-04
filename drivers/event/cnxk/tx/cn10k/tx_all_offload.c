/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn10k_tx_worker.h"

#ifdef _ROC_API_H_
#error "roc_api.h is included"
#endif

#if defined(CNXK_DIS_TMPLT_FUNC)

uint16_t __rte_hot
cn10k_sso_hws_tx_adptr_enq_seg_all_offload(void *port, struct rte_event ev[], uint16_t nb_events)
{
	const uint32_t flags = (NIX_TX_OFFLOAD_L3_L4_CSUM_F | NIX_TX_OFFLOAD_MBUF_NOFF_F |
				NIX_TX_MULTI_SEG_F | NIX_TX_OFFLOAD_SECURITY_F);
	uint64_t cmd[8 + CNXK_NIX_TX_MSEG_SG_DWORDS - 2];

	struct cn10k_sso_hws *ws = port;
	RTE_SET_USED(nb_events);
	return cn10k_sso_hws_event_tx(ws, &ev[0], cmd, (const uint64_t *)ws->tx_adptr_data, flags);
}

uint16_t __rte_hot
cn10k_sso_hws_tx_adptr_enq_seg_all_offload_tst(void *port, struct rte_event ev[],
					       uint16_t nb_events)
{
	const uint32_t flags =
		(NIX_TX_OFFLOAD_L3_L4_CSUM_F | NIX_TX_OFFLOAD_OL3_OL4_CSUM_F |
		 NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_MBUF_NOFF_F | NIX_TX_OFFLOAD_TSO_F |
		 NIX_TX_OFFLOAD_TSTAMP_F | NIX_TX_OFFLOAD_SECURITY_F | NIX_TX_MULTI_SEG_F);
	uint64_t cmd[8 + CNXK_NIX_TX_MSEG_SG_DWORDS - 2];

	struct cn10k_sso_hws *ws = port;
	RTE_SET_USED(nb_events);
	return cn10k_sso_hws_event_tx(ws, &ev[0], cmd, (const uint64_t *)ws->tx_adptr_data, flags);
}

#endif
