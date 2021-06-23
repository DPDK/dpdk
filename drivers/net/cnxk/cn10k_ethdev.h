/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CN10K_ETHDEV_H__
#define __CN10K_ETHDEV_H__

#include <cnxk_ethdev.h>

struct cn10k_eth_rxq {
	uint64_t mbuf_initializer;
	uintptr_t desc;
	void *lookup_mem;
	uintptr_t cq_door;
	uint64_t wdata;
	int64_t *cq_status;
	uint32_t head;
	uint32_t qmask;
	uint32_t available;
	uint16_t data_off;
	uint16_t rq;
} __plt_cache_aligned;

#endif /* __CN10K_ETHDEV_H__ */
