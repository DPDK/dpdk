/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef __RTE_DPAA2_MEMPOOL_H__
#define __RTE_DPAA2_MEMPOOL_H__

/**
 * @file
 *
 * NXP specific mempool related functions.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_mempool.h>

/**
 * Get BPID corresponding to the packet pool
 *
 * @param mp
 *   memory pool
 *
 * @return
 *   BPID of the buffer pool
 */
uint16_t
rte_dpaa2_mbuf_pool_bpid(struct rte_mempool *mp);

/**
 * Get MBUF from the corresponding 'buf_addr'
 *
 * @param mp
 *   memory pool
 * @param buf_addr
 *   The 'buf_addr' of the mbuf. This is the start buffer address
 *   of the packet buffer (mbuf).
 *
 * @return
 *   - MBUF pointer for success
 *   - NULL in case of error
 */
struct rte_mbuf *
rte_dpaa2_mbuf_from_buf_addr(struct rte_mempool *mp, void *buf_addr);

#ifdef __cplusplus
}
#endif

#endif /* __RTE_DPAA2_MEMPOOL_H__ */
