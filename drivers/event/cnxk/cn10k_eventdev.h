/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef __CN10K_EVENTDEV_H__
#define __CN10K_EVENTDEV_H__

#define CN10K_SSO_DEFAULT_STASH_OFFSET -1
#define CN10K_SSO_DEFAULT_STASH_LENGTH 2

struct cn10k_sso_hws {
	uint64_t base;
	uint64_t gw_rdata;
	void *lookup_mem;
	uint32_t gw_wdata;
	uint8_t swtag_req;
	uint8_t hws_id;
	/* PTP timestamp */
	struct cnxk_timesync_info **tstamp;
	uint64_t meta_aura;
	/* Add Work Fastpath data */
	uint64_t xaq_lmt __rte_cache_aligned;
	uint64_t *fc_mem;
	uintptr_t grp_base;
	/* Tx Fastpath data */
	uintptr_t lmt_base __rte_cache_aligned;
	uint64_t lso_tun_fmt;
	uint8_t tx_adptr_data[];
} __rte_cache_aligned;

#endif /* __CN10K_EVENTDEV_H__ */
