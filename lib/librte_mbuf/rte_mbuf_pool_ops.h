/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef _RTE_MBUF_POOL_OPS_H_
#define _RTE_MBUF_POOL_OPS_H_

/**
 * @file
 * RTE Mbuf Pool Ops
 *
 * These APIs are for configuring the mbuf pool ops names to be largely used by
 * rte_pktmbuf_pool_create(). However, this can also be used to set and inquire
 * the best mempool ops available.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set the platform supported pktmbuf HW mempool ops name
 *
 * This function allow the HW to register the actively supported HW mempool
 * ops_name. Only one HW mempool ops can be registered at any point of time.
 *
 * @param ops_name
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int __rte_experimental
rte_mbuf_set_platform_mempool_ops(const char *ops_name);

/**
 * Get configured platform supported pktmbuf HW mempool ops name
 *
 * This function returns the platform supported mempool ops name.
 *
 * @return
 *   - On success, platform pool ops name.
 *   - On failure, NULL.
 */
const char * __rte_experimental
rte_mbuf_platform_mempool_ops(void);

/**
 * Set the user preferred pktmbuf mempool ops name
 *
 * This function can be used by the user to configure user preferred
 * mempool ops name.
 *
 * @param ops_name
 * @return
 *   - On success, zero.
 *   - On failure, a negative value.
 */
int __rte_experimental
rte_mbuf_set_user_mempool_ops(const char *ops_name);

/**
 * Get user preferred pool ops name for mbuf
 *
 * This function returns the user configured mempool ops name.
 *
 * @return
 *   - On success, user pool ops name..
 *   - On failure, NULL.
 */
const char * __rte_experimental
rte_mbuf_user_mempool_ops(void);

/**
 * Get the best mempool ops name for pktmbuf.
 *
 * This function is used to determine the best options for mempool ops for
 * pktmbuf allocations. Following are the priority order:
 * 1. User defined, 2. Platform HW supported, 3. Compile time configured.
 * This function is also used by the rte_pktmbuf_pool_create to get the best
 * mempool ops name.
 *
 * @return
 *   returns preferred mbuf pool ops name
 */
const char * __rte_experimental
rte_mbuf_best_mempool_ops(void);


#ifdef __cplusplus
}
#endif

#endif /* _RTE_MBUF_POOL_OPS_H_ */
