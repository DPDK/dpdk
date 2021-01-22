/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_POWER_INTRINSIC_H_
#define _RTE_POWER_INTRINSIC_H_

#include <inttypes.h>

#include <rte_compat.h>
#include <rte_spinlock.h>

/**
 * @file
 * Advanced power management operations.
 *
 * This file define APIs for advanced power management,
 * which are architecture-dependent.
 */

struct rte_power_monitor_cond {
	volatile void *addr;  /**< Address to monitor for changes */
	uint64_t val;         /**< If the `mask` is non-zero, location pointed
	                       *   to by `addr` will be read and compared
	                       *   against this value.
	                       */
	uint64_t mask;   /**< 64-bit mask to extract value read from `addr` */
	uint8_t size;    /**< Data size (in bytes) that will be used to compare
	                  *   expected value (`val`) with data read from the
	                  *   monitored memory location (`addr`). Can be 1, 2,
	                  *   4, or 8. Supplying any other value will result in
	                  *   an error.
	                  */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Monitor specific address for changes. This will cause the CPU to enter an
 * architecture-defined optimized power state until either the specified
 * memory address is written to, a certain TSC timestamp is reached, or other
 * reasons cause the CPU to wake up.
 *
 * Additionally, an expected value (`pmc->val`), mask (`pmc->mask`), and data
 * size (`pmc->size`) are provided in the `pmc` power monitoring condition. If
 * the mask is non-zero, the current value pointed to by the `pmc->addr` pointer
 * will be read and compared against the expected value, and if they match, the
 * entering of optimized power state will be aborted. This is intended to
 * prevent the CPU from entering optimized power state and waiting on a write
 * that has already happened by the time this API is called.
 *
 * @warning It is responsibility of the user to check if this function is
 *   supported at runtime using `rte_cpu_get_intrinsics_support()` API call.
 *
 * @param pmc
 *   The monitoring condition structure.
 * @param tsc_timestamp
 *   Maximum TSC timestamp to wait for. Note that the wait behavior is
 *   architecture-dependent.
 *
 * @return
 *   0 on success
 *   -EINVAL on invalid parameters
 *   -ENOTSUP if unsupported
 */
__rte_experimental
int rte_power_monitor(const struct rte_power_monitor_cond *pmc,
		const uint64_t tsc_timestamp);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Wake up a specific lcore that is in a power optimized state and is monitoring
 * an address.
 *
 * @note It is safe to call this function if the lcore in question is not
 *   sleeping. The function will have no effect.
 *
 * @note This function will *not* wake up a core that is in a power optimized
 *   state due to calling `rte_power_pause`.
 *
 * @param lcore_id
 *   Lcore ID of a sleeping thread.
 */
__rte_experimental
int rte_power_monitor_wakeup(const unsigned int lcore_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enter an architecture-defined optimized power state until a certain TSC
 * timestamp is reached.
 *
 * @warning It is responsibility of the user to check if this function is
 *   supported at runtime using `rte_cpu_get_intrinsics_support()` API call.
 *
 * @param tsc_timestamp
 *   Maximum TSC timestamp to wait for. Note that the wait behavior is
 *   architecture-dependent.
 *
 * @return
 *   0 on success
 *   -EINVAL on invalid parameters
 *   -ENOTSUP if unsupported
 */
__rte_experimental
int rte_power_pause(const uint64_t tsc_timestamp);

#endif /* _RTE_POWER_INTRINSIC_H_ */
