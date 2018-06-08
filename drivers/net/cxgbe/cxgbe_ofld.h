/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef _CXGBE_OFLD_H_
#define _CXGBE_OFLD_H_

#include <rte_bitmap.h>

#include "cxgbe_filter.h"

/*
 * Holds the size, base address, free list start, etc of filter TID.
 * The tables themselves are allocated dynamically.
 */
struct tid_info {
	void **tid_tab;
	unsigned int ntids;
	struct filter_entry *ftid_tab;	/* Normal filters */
	struct rte_bitmap *ftid_bmap;
	uint8_t *ftid_bmap_array;
	unsigned int nftids;
	unsigned int ftid_base;
	rte_spinlock_t ftid_lock;
};
#endif /* _CXGBE_OFLD_H_ */
