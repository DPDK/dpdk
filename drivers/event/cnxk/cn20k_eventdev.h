/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef __CN20K_EVENTDEV_H__
#define __CN20K_EVENTDEV_H__

#define CN20K_SSO_DEFAULT_STASH_OFFSET -1
#define CN20K_SSO_DEFAULT_STASH_LENGTH 2

struct __rte_cache_aligned cn20k_sso_hws {
	uint64_t base;
	uint32_t gw_wdata;
	uint64_t gw_rdata;
	uint8_t swtag_req;
	uint8_t hws_id;
	/* Add Work Fastpath data */
	alignas(RTE_CACHE_LINE_SIZE) int64_t __rte_atomic *fc_mem;
	int64_t __rte_atomic *fc_cache_space;
	uintptr_t aw_lmt;
	uintptr_t grp_base;
	uint16_t xae_waes;
	int32_t xaq_lmt;
};

#endif /* __CN20K_EVENTDEV_H__ */
