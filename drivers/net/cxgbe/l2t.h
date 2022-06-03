/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */
#ifndef _CXGBE_L2T_H_
#define _CXGBE_L2T_H_

#include "base/t4_msg.h"

enum {
	L2T_SIZE = 4096       /* # of L2T entries */
};

enum {
	L2T_STATE_VALID,      /* entry is up to date */
	L2T_STATE_SYNC_WRITE, /* synchronous write of entry underway */

	/* when state is one of the below the entry is not hashed */
	L2T_STATE_SWITCHING,  /* entry is being used by a switching filter */
	L2T_STATE_UNUSED      /* entry not in use */
};

/*
 * State for the corresponding entry of the HW L2 table.
 */
struct l2t_entry {
	u16 state;                  /* entry state */
	u16 idx;                    /* entry index within in-memory table */
	u16 vlan;                   /* VLAN TCI (id: bits 0-11, prio: 13-15 */
	u8  lport;                  /* destination port */
	u8  dmac[RTE_ETHER_ADDR_LEN];   /* destination MAC address */
	rte_spinlock_t lock;        /* entry lock */
	u32 refcnt;                 /* entry reference count */
};

struct l2t_data {
	unsigned int l2t_start;     /* start index of our piece of the L2T */
	unsigned int l2t_size;      /* number of entries in l2tab */
	rte_rwlock_t lock;          /* table rw lock */
	struct l2t_entry l2tab[];  /* MUST BE LAST */
};

#define L2T_LPBK	true
#define L2T_ARPMISS	true

/* identifies sync vs async L2T_WRITE_REQs */
#define S_SYNC_WR    12
#define V_SYNC_WR(x) ((x) << S_SYNC_WR)
#define F_SYNC_WR    V_SYNC_WR(1)

struct l2t_data *t4_init_l2t(unsigned int l2t_start, unsigned int l2t_end);
void t4_cleanup_l2t(struct adapter *adap);
struct l2t_entry *cxgbe_l2t_alloc_switching(struct rte_eth_dev *dev, u16 vlan,
					    u8 port, u8 *dmac);
void cxgbe_l2t_release(struct l2t_entry *e);
void cxgbe_do_l2t_write_rpl(struct adapter *p,
			    const struct cpl_l2t_write_rpl *rpl);
#endif /* _CXGBE_L2T_H_ */
