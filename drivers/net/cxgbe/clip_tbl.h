/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef _CXGBE_CLIP_H_
#define _CXGBE_CLIP_H_

/*
 * State for the corresponding entry of the HW CLIP table.
 */
struct clip_entry {
	enum filter_type type;       /* entry type */
	u32 addr[4];                 /* IPV4 or IPV6 address */
	rte_spinlock_t lock;         /* entry lock */
	u32 refcnt;                  /* entry reference count */
};

struct clip_tbl {
	unsigned int clipt_start;     /* start index of CLIP table */
	unsigned int clipt_size;      /* size of CLIP table */
	rte_rwlock_t lock;            /* table rw lock */
	struct clip_entry cl_list[]; /* MUST BE LAST */
};

struct clip_tbl *t4_init_clip_tbl(unsigned int clipt_start,
				  unsigned int clipt_end);
void t4_cleanup_clip_tbl(struct adapter *adap);
struct clip_entry *cxgbe_clip_alloc(struct rte_eth_dev *dev, u32 *lip);
void cxgbe_clip_release(struct rte_eth_dev *dev, struct clip_entry *ce);
#endif /* _CXGBE_CLIP_H_ */
