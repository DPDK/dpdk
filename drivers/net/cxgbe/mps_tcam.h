/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef _CXGBE_MPSTCAM_H_
#define _CXGBE_MPSTCAM_H_

#include "base/common.h"

enum {
	MPS_ENTRY_UNUSED,	/* Keep this first so memset 0 renders
				 * the correct state. Other states can
				 * be added in future like MPS_ENTRY_BUSY
				 * to reduce contention while mboxing
				 * the request to f/w or to denote attributes
				 * for a specific entry
				 */
	MPS_ENTRY_USED,
	MPS_ENTRY_RAWF, /* Reserved for RAW MAC Filters */
};

struct mps_tcam_entry {
	u8 state;
	u16 idx;

	/* add data here which uniquely defines an entry */
	u8 eth_addr[RTE_ETHER_ADDR_LEN];
	u8 mask[RTE_ETHER_ADDR_LEN];

	struct mpstcam_table *mpstcam; /* backptr */
	u32 refcnt;
};

struct mpstcam_table {
	u16 size;
	rte_rwlock_t lock;
	u16 free_idx;	/* next free index */
	bool full;	/* since free index can be present
			 * anywhere in the table, size and
			 * free_idx cannot alone determine
			 * if the table is full
			 */
	struct mps_tcam_entry entry[0];
};

struct mpstcam_table *t4_init_mpstcam(struct adapter *adap);
void t4_cleanup_mpstcam(struct adapter *adap);
int cxgbe_mpstcam_alloc(struct port_info *pi, const u8 *mac, const u8 *mask);
int cxgbe_mpstcam_remove(struct port_info *pi, u16 idx);
int cxgbe_mpstcam_modify(struct port_info *pi, int idx, const u8 *addr);
int cxgbe_mpstcam_rawf_enable(struct port_info *pi);
int cxgbe_mpstcam_rawf_disable(struct port_info *pi);
#endif /* _CXGBE_MPSTCAM_H_ */
