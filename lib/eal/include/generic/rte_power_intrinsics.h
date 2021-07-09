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

/** Size of the opaque data in monitor condition */
#define RTE_POWER_MONITOR_OPAQUE_SZ 4

/**
 * Callback definition for monitoring conditions. Callbacks with this signature
 * will be used by `rte_power_monitor()` to check if the entering of power
 * optimized state should be aborted.
 *
 * @param val
 *   The value read from memory.
 * @param opaque
 *   Callback-specific data.
 *
 * @return
 *   0 if entering of power optimized state should proceed
 *   -1 if entering of power optimized state should be aborted
 */
typedef int (*rte_power_monitor_clb_t)(const uint64_t val,
		const uint64_t opaque[RTE_POWER_MONITOR_OPAQUE_SZ]);

struct rte_power_monitor_cond {
	volatile void *addr;  /**< Address to monitor for changes */
	uint8_t size;    /**< Data size (in bytes) that will be read from the
	                  *   monitored memory location (`addr`). Can be 1, 2,
	                  *   4, or 8. Supplying any other value will result in
	                  *   an error.
	                  */
	rte_power_monitor_clb_t fn; /**< Callback to be used to check if
	                             *   entering power optimized state should
	                             *   be aborted.
	                             */
	uint64_t opaque[RTE_POWER_MONITOR_OPAQUE_SZ];
	/**< Callback-specific data */
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

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Monitor a set of addresses for changes. This will cause the CPU to enter an
 * architecture-defined optimized power state until either one of the specified
 * memory addresses is written to, a certain TSC timestamp is reached, or other
 * reasons cause the CPU to wake up.
 *
 * Additionally, `expected` 64-bit values and 64-bit masks are provided. If
 * mask is non-zero, the current value pointed to by the `p` pointer will be
 * checked against the expected value, and if they do not match, the entering of
 * optimized power state may be aborted.
 *
 * @warning It is responsibility of the user to check if this function is
 *   supported at runtime using `rte_cpu_get_intrinsics_support()` API call.
 *   Failing to do so may result in an illegal CPU instruction error.
 *
 * @param pmc
 *   An array of monitoring condition structures.
 * @param num
 *   Length of the `pmc` array.
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
int rte_power_monitor_multi(const struct rte_power_monitor_cond pmc[],
		const uint32_t num, const uint64_t tsc_timestamp);

#endif /* _RTE_POWER_INTRINSIC_H_ */
