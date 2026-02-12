/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn10k_worker.h"

#ifdef _ROC_API_H_
#error "roc_api.h is included"
#endif

#if defined(CNXK_DIS_TMPLT_FUNC)

uint16_t __rte_hot
cn10k_sso_hws_deq_burst_all_offload(void *port, struct rte_event ev[], uint16_t nb_events,
				    uint64_t timeout_ticks)
{
	const uint32_t flags = (NIX_RX_OFFLOAD_RSS_F | NIX_RX_OFFLOAD_PTYPE_F |
				NIX_RX_OFFLOAD_CHECKSUM_F | NIX_RX_OFFLOAD_MARK_UPDATE_F |
				NIX_RX_OFFLOAD_VLAN_STRIP_F |
				NIX_RX_OFFLOAD_SECURITY_F | NIX_RX_MULTI_SEG_F | NIX_RX_REAS_F);
	struct cn10k_sso_hws *ws = port;
	uint16_t ret = 1;
	uint64_t iter;

	RTE_SET_USED(nb_events);
	if (ws->swtag_req) {
		ws->swtag_req = 0;
		ws->gw_rdata = cnxk_sso_hws_swtag_wait(ws->base + SSOW_LF_GWS_WQE0);
		return ret;
	}

	ret = cn10k_sso_hws_get_work(ws, ev, flags);
	for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)
		ret = cn10k_sso_hws_get_work(ws, ev, flags);

	return ret;
}

uint16_t __rte_hot
cn10k_sso_hws_deq_burst_all_offload_tst(void *port, struct rte_event ev[], uint16_t nb_events,
					uint64_t timeout_ticks)
{
	const uint32_t flags = (NIX_RX_OFFLOAD_RSS_F | NIX_RX_OFFLOAD_PTYPE_F |
				NIX_RX_OFFLOAD_CHECKSUM_F | NIX_RX_OFFLOAD_MARK_UPDATE_F |
				NIX_RX_OFFLOAD_TSTAMP_F | NIX_RX_OFFLOAD_VLAN_STRIP_F |
				NIX_RX_OFFLOAD_SECURITY_F | NIX_RX_MULTI_SEG_F | NIX_RX_REAS_F);
	struct cn10k_sso_hws *ws = port;
	uint16_t ret = 1;
	uint64_t iter;

	RTE_SET_USED(nb_events);
	if (ws->swtag_req) {
		ws->swtag_req = 0;
		ws->gw_rdata = cnxk_sso_hws_swtag_wait(ws->base + SSOW_LF_GWS_WQE0);
		return ret;
	}

	ret = cn10k_sso_hws_get_work(ws, ev, flags);
	for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)
		ret = cn10k_sso_hws_get_work(ws, ev, flags);

	return ret;
}

#endif
