/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Chelsio Communications.
 * All rights reserved.
 */

#include "base/common.h"
#include "smt.h"

/**
 * Initialize Source MAC Table
 */
struct smt_data *t4_init_smt(u32 smt_start_idx, u32 smt_size)
{
	struct smt_data *s;
	u32 i;

	s = t4_alloc_mem(sizeof(*s) + smt_size * sizeof(struct smt_entry));
	if (!s)
		return NULL;

	s->smt_start = smt_start_idx;
	s->smt_size = smt_size;
	t4_os_rwlock_init(&s->lock);

	for (i = 0; i < s->smt_size; ++i) {
		s->smtab[i].idx = i;
		s->smtab[i].hw_idx = smt_start_idx + i;
		s->smtab[i].state = SMT_STATE_UNUSED;
		memset(&s->smtab[i].src_mac, 0, RTE_ETHER_ADDR_LEN);
		t4_os_lock_init(&s->smtab[i].lock);
		rte_atomic32_set(&s->smtab[i].refcnt, 0);
	}
	return s;
}

/**
 * Cleanup Source MAC Table
 */
void t4_cleanup_smt(struct adapter *adap)
{
	if (adap->smt)
		t4_os_free(adap->smt);
}
