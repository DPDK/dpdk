/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef _CXGBE_OFLD_H_
#define _CXGBE_OFLD_H_

#include <rte_bitmap.h>

#include "cxgbe_filter.h"

/*
 * Max # of ATIDs.  The absolute HW max is 16K but we keep it lower.
 */
#define MAX_ATIDS 8192U

union aopen_entry {
	void *data;
	union aopen_entry *next;
};

/*
 * Holds the size, base address, free list start, etc of filter TID.
 * The tables themselves are allocated dynamically.
 */
struct tid_info {
	void **tid_tab;
	unsigned int ntids;
	struct filter_entry *ftid_tab;	/* Normal filters */
	union aopen_entry *atid_tab;
	struct rte_bitmap *ftid_bmap;
	uint8_t *ftid_bmap_array;
	unsigned int nftids, natids;
	unsigned int ftid_base, hash_base;

	union aopen_entry *afree;
	unsigned int atids_in_use;

	/* TIDs in the TCAM */
	rte_atomic32_t tids_in_use;
	/* TIDs in the HASH */
	rte_atomic32_t hash_tids_in_use;
	rte_atomic32_t conns_in_use;

	rte_spinlock_t atid_lock __rte_cache_aligned;
	rte_spinlock_t ftid_lock;
};
#endif /* _CXGBE_OFLD_H_ */
