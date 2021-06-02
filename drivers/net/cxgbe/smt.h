/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Chelsio Communications.
 * All rights reserved.
 */
#ifndef __CXGBE_SMT_H_
#define __CXGBE_SMT_H_

#include "base/t4_msg.h"

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
	u32 refcnt;
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
void cxgbe_do_smt_write_rpl(struct adapter *adap,
			    const struct cpl_smt_write_rpl *rpl);
struct smt_entry *cxgbe_smt_alloc_switching(struct rte_eth_dev *dev, u8 *smac);
void cxgbe_smt_release(struct smt_entry *e);

#endif  /* __CXGBE_SMT_H_ */

