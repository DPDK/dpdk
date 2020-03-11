/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Chelsio Communications.
 * All rights reserved.
 */
#ifndef __CXGBE_SMT_H_
#define __CXGBE_SMT_H_

enum {
	SMT_STATE_SWITCHING,
	SMT_STATE_UNUSED,
	SMT_STATE_ERROR
};

enum {
	SMT_SIZE = 256
};

struct smt_entry {
	u16 state;
	u16 idx;
	u16 pfvf;
	u16 hw_idx;
	u8 src_mac[RTE_ETHER_ADDR_LEN];
	rte_atomic32_t refcnt;
	rte_spinlock_t lock;
};

struct smt_data {
	unsigned int smt_size;
	unsigned int smt_start;
	rte_rwlock_t lock;
	struct smt_entry smtab[0];
};

struct smt_data *t4_init_smt(u32 smt_start_idx, u32 smt_size);
void t4_cleanup_smt(struct adapter *adap);

#endif  /* __CXGBE_SMT_H_ */

