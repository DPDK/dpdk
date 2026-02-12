/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CNXK_COMMON_H__
#define __CNXK_COMMON_H__

#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

struct cnxk_sso_hws_prf {
	uint64_t base;
	uint32_t gw_wdata;
	void *lookup_mem;
	uint64_t gw_rdata;
	uint8_t swtag_req;
	uint8_t hws_id;
};

static uint32_t
cnxk_sso_hws_prf_wdata(struct cnxk_sso_evdev *dev)
{
	uint32_t wdata = 1;

	if (dev->deq_tmo_ns)
		wdata |= BIT(16);

	switch (dev->gw_mode) {
	case CNXK_GW_MODE_NONE:
	default:
		break;
	case CNXK_GW_MODE_PREF:
		wdata |= BIT(19);
		break;
	case CNXK_GW_MODE_PREF_WFE:
		wdata |= BIT(20) | BIT(19);
		break;
	}

	return wdata;
}

static uint8_t
cnxk_sso_hws_preschedule_get(uint8_t preschedule_type)
{
	uint8_t gw_mode = 0;

	switch (preschedule_type) {
	default:
	case RTE_EVENT_PRESCHEDULE_NONE:
		gw_mode = CNXK_GW_MODE_NONE;
		break;
	case RTE_EVENT_PRESCHEDULE:
		gw_mode = CNXK_GW_MODE_PREF;
		break;
	case RTE_EVENT_PRESCHEDULE_ADAPTIVE:
		gw_mode = CNXK_GW_MODE_PREF_WFE;
		break;
	}

	return gw_mode;
}

static void
cnxk_sso_hws_reset(void *arg, void *ws)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cnxk_sso_hws_prf *ws_prf;
	uint64_t pend_state;
	uint8_t swtag_req;
	uintptr_t base;
	uint8_t hws_id;
	union {
		__uint128_t wdata;
		uint64_t u64[2];
	} gw;
	uint8_t pend_tt;
	bool is_pend;

	ws_prf = ws;
	base = ws_prf->base;
	hws_id = ws_prf->hws_id;
	swtag_req = ws_prf->swtag_req;

	roc_sso_hws_gwc_invalidate(&dev->sso, &hws_id, 1);
	plt_write64(0, base + SSOW_LF_GWS_OP_GWC_INVAL);
	/* Wait till getwork/swtp/waitw/desched completes. */
	is_pend = false;
	/* Work in WQE0 is always consumed, unless its a SWTAG. */
	pend_state = plt_read64(base + SSOW_LF_GWS_PENDSTATE);
	if (pend_state & (BIT_ULL(63) | BIT_ULL(62) | BIT_ULL(54)) || swtag_req)
		is_pend = true;

	do {
		pend_state = plt_read64(base + SSOW_LF_GWS_PENDSTATE);
	} while (pend_state &
		 (BIT_ULL(63) | BIT_ULL(62) | BIT_ULL(58) | BIT_ULL(56) | BIT_ULL(54)));
	pend_tt = CNXK_TT_FROM_TAG(plt_read64(base + SSOW_LF_GWS_WQE0));
	if (is_pend && pend_tt != SSO_TT_EMPTY) { /* Work was pending */
		if (pend_tt == SSO_TT_ATOMIC || pend_tt == SSO_TT_ORDERED)
			cnxk_sso_hws_swtag_untag(base + SSOW_LF_GWS_OP_SWTAG_UNTAG);
		plt_write64(0, base + SSOW_LF_GWS_OP_DESCHED);
	} else if (pend_tt != SSO_TT_EMPTY) {
		plt_write64(0, base + SSOW_LF_GWS_OP_SWTAG_FLUSH);
	}

	/* Wait for desched to complete. */
	do {
		pend_state = plt_read64(base + SSOW_LF_GWS_PENDSTATE);
	} while (pend_state & (BIT_ULL(58) | BIT_ULL(56)));

	switch (dev->gw_mode) {
	case CNXK_GW_MODE_PREF:
	case CNXK_GW_MODE_PREF_WFE:
		while (plt_read64(base + SSOW_LF_GWS_PRF_WQE0) & BIT_ULL(63))
			;
		break;
	case CNXK_GW_MODE_NONE:
	default:
		break;
	}

	if (CNXK_TT_FROM_TAG(plt_read64(base + SSOW_LF_GWS_PRF_WQE0)) != SSO_TT_EMPTY) {
		plt_write64(BIT_ULL(16) | 1, base + SSOW_LF_GWS_OP_GET_WORK0);
		do {
			roc_load_pair(gw.u64[0], gw.u64[1], base + SSOW_LF_GWS_WQE0);
		} while (gw.u64[0] & BIT_ULL(63));
		pend_tt = CNXK_TT_FROM_TAG(plt_read64(base + SSOW_LF_GWS_WQE0));
		if (pend_tt != SSO_TT_EMPTY) { /* Work was pending */
			if (pend_tt == SSO_TT_ATOMIC || pend_tt == SSO_TT_ORDERED)
				cnxk_sso_hws_swtag_untag(base + SSOW_LF_GWS_OP_SWTAG_UNTAG);
			plt_write64(0, base + SSOW_LF_GWS_OP_DESCHED);
		}
	}

	plt_write64(0, base + SSOW_LF_GWS_OP_GWC_INVAL);
	roc_sso_hws_gwc_invalidate(&dev->sso, &hws_id, 1);
	rte_mb();
}

static void
cnxk_sso_configure_queue_stash(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct roc_sso_hwgrp_stash stash[dev->stash_cnt];
	int i, rc;

	plt_sso_dbg();
	for (i = 0; i < dev->stash_cnt; i++) {
		stash[i].hwgrp = dev->stash_parse_data[i].queue;
		stash[i].stash_offset = dev->stash_parse_data[i].stash_offset;
		stash[i].stash_count = dev->stash_parse_data[i].stash_length;
	}
	rc = roc_sso_hwgrp_stash_config(&dev->sso, stash, dev->stash_cnt);
	if (rc < 0)
		plt_warn("failed to configure HWGRP WQE stashing rc = %d", rc);
}

#endif /* __CNXK_COMMON_H__ */
