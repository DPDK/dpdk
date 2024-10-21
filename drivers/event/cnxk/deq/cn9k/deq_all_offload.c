/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn9k_worker.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

#if defined(CNXK_DIS_TMPLT_FUNC)

uint16_t __rte_hot
cn9k_sso_hws_deq_burst_all_offload(void *port, struct rte_event ev[], uint16_t nb_events,
				   uint64_t timeout_ticks)
{
	const uint32_t flags =
		(NIX_RX_OFFLOAD_RSS_F | NIX_RX_OFFLOAD_PTYPE_F | NIX_RX_OFFLOAD_CHECKSUM_F |
		 NIX_RX_OFFLOAD_MARK_UPDATE_F |
		 NIX_RX_OFFLOAD_VLAN_STRIP_F | NIX_RX_OFFLOAD_SECURITY_F | NIX_RX_MULTI_SEG_F);
	struct cn9k_sso_hws *ws = port;
	uint16_t ret = 1;
	uint64_t iter;

	RTE_SET_USED(nb_events);
	if (ws->swtag_req) {
		ws->swtag_req = 0;
		cnxk_sso_hws_swtag_wait(ws->base + SSOW_LF_GWS_TAG);
		return ret;
	}
	ret = cn9k_sso_hws_get_work(ws, ev, flags, ws->lookup_mem);
	for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)
		ret = cn9k_sso_hws_get_work(ws, ev, flags, ws->lookup_mem);
	return ret;
}

uint16_t __rte_hot
cn9k_sso_hws_deq_dual_burst_all_offload(void *port, struct rte_event ev[], uint16_t nb_events,
					uint64_t timeout_ticks)
{
	const uint32_t flags =
		(NIX_RX_OFFLOAD_RSS_F | NIX_RX_OFFLOAD_PTYPE_F | NIX_RX_OFFLOAD_CHECKSUM_F |
		 NIX_RX_OFFLOAD_MARK_UPDATE_F |
		 NIX_RX_OFFLOAD_VLAN_STRIP_F | NIX_RX_OFFLOAD_SECURITY_F | NIX_RX_MULTI_SEG_F);
	struct cn9k_sso_hws_dual *dws = port;
	uint16_t ret = 1;
	uint64_t iter;

	RTE_SET_USED(nb_events);
	if (dws->swtag_req) {
		dws->swtag_req = 0;
		cnxk_sso_hws_swtag_wait(dws->base[!dws->vws] + SSOW_LF_GWS_TAG);
		return ret;
	}
	ret = cn9k_sso_hws_dual_get_work(dws->base[dws->vws], dws->base[!dws->vws], ev, flags, dws);
	dws->vws = !dws->vws;
	for (iter = 1; iter < timeout_ticks && (ret == 0); iter++) {
		ret = cn9k_sso_hws_dual_get_work(dws->base[dws->vws], dws->base[!dws->vws], ev,
						 flags, dws);
		dws->vws = !dws->vws;
	}
	return ret;
}

uint16_t __rte_hot
cn9k_sso_hws_deq_burst_all_offload_tst(void *port, struct rte_event ev[], uint16_t nb_events,
				       uint64_t timeout_ticks)
{
	const uint32_t flags =
		(NIX_RX_OFFLOAD_RSS_F | NIX_RX_OFFLOAD_PTYPE_F | NIX_RX_OFFLOAD_CHECKSUM_F |
		 NIX_RX_OFFLOAD_MARK_UPDATE_F | NIX_RX_OFFLOAD_TSTAMP_F |
		 NIX_RX_OFFLOAD_VLAN_STRIP_F | NIX_RX_OFFLOAD_SECURITY_F | NIX_RX_MULTI_SEG_F);

	struct cn9k_sso_hws *ws = port;
	uint16_t ret = 1;
	uint64_t iter;

	RTE_SET_USED(nb_events);
	if (ws->swtag_req) {
		ws->swtag_req = 0;
		cnxk_sso_hws_swtag_wait(ws->base + SSOW_LF_GWS_TAG);
		return ret;
	}
	ret = cn9k_sso_hws_get_work(ws, ev, flags, ws->lookup_mem);
	for (iter = 1; iter < timeout_ticks && (ret == 0); iter++)
		ret = cn9k_sso_hws_get_work(ws, ev, flags, ws->lookup_mem);
	return ret;
}

uint16_t __rte_hot
cn9k_sso_hws_deq_dual_burst_all_offload_tst(void *port, struct rte_event ev[], uint16_t nb_events,
					    uint64_t timeout_ticks)
{
	const uint32_t flags =
		(NIX_RX_OFFLOAD_RSS_F | NIX_RX_OFFLOAD_PTYPE_F | NIX_RX_OFFLOAD_CHECKSUM_F |
		 NIX_RX_OFFLOAD_MARK_UPDATE_F | NIX_RX_OFFLOAD_TSTAMP_F |
		 NIX_RX_OFFLOAD_VLAN_STRIP_F | NIX_RX_OFFLOAD_SECURITY_F | NIX_RX_MULTI_SEG_F);
	struct cn9k_sso_hws_dual *dws = port;
	uint16_t ret = 1;
	uint64_t iter;

	RTE_SET_USED(nb_events);
	if (dws->swtag_req) {
		dws->swtag_req = 0;
		cnxk_sso_hws_swtag_wait(dws->base[!dws->vws] + SSOW_LF_GWS_TAG);
		return ret;
	}
	ret = cn9k_sso_hws_dual_get_work(dws->base[dws->vws], dws->base[!dws->vws], ev, flags, dws);
	dws->vws = !dws->vws;
	for (iter = 1; iter < timeout_ticks && (ret == 0); iter++) {
		ret = cn9k_sso_hws_dual_get_work(dws->base[dws->vws], dws->base[!dws->vws], ev,
						 flags, dws);
		dws->vws = !dws->vws;
	}
	return ret;
}

#endif
