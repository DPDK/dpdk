/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

/**
 * @file rte_pmd_cnxk_mempool.h
 * Marvell CNXK Mempool PMD specific functions.
 *
 **/

#ifndef _PMD_CNXK_MEMPOOL_H_
#define _PMD_CNXK_MEMPOOL_H_

#include <rte_mbuf.h>
#include <rte_mempool.h>

/**
 * Exchange mbufs between two mempools.
 *
 * @param m1
 *   First mbuf
 * @param m2
 *   Second mbuf
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
__rte_experimental
int rte_pmd_cnxk_mempool_mbuf_exchange(struct rte_mbuf *m1,
				       struct rte_mbuf *m2);

/**
 * Check whether a mempool is a hwpool.
 *
 * @param mp
 *   Mempool to check.
 *
 * @return
 *   1 if mp is a hwpool, 0 otherwise.
 */
__rte_experimental
int rte_pmd_cnxk_mempool_is_hwpool(struct rte_mempool *mp);

/**
 * Disable buffer address range check on a mempool.
 *
 * @param mp
 *   Mempool to disable range check on.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
__rte_experimental
int rte_pmd_cnxk_mempool_range_check_disable(struct rte_mempool *mp);

#endif /* _PMD_CNXK_MEMPOOL_H_ */
