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

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Monitor specific address for changes. This will cause the CPU to enter an
 * architecture-defined optimized power state until either the specified
 * memory address is written to, a certain TSC timestamp is reached, or other
 * reasons cause the CPU to wake up.
 *
 * Additionally, an `expected` 64-bit value and 64-bit mask are provided. If
 * mask is non-zero, the current value pointed to by the `p` pointer will be
 * checked against the expected value, and if they match, the entering of
 * optimized power state may be aborted.
 *
 * @warning It is responsibility of the user to check if this function is
 *   supported at runtime using `rte_cpu_get_intrinsics_support()` API call.
 *   Failing to do so may result in an illegal CPU instruction error.
 *
 * @param p
 *   Address to monitor for changes.
 * @param expected_value
 *   Before attempting the monitoring, the `p` address may be read and compared
 *   against this value. If `value_mask` is zero, this step will be skipped.
 * @param value_mask
 *   The 64-bit mask to use to extract current value from `p`.
 * @param tsc_timestamp
 *   Maximum TSC timestamp to wait for. Note that the wait behavior is
 *   architecture-dependent.
 * @param data_sz
 *   Data size (in bytes) that will be used to compare expected value with the
 *   memory address. Can be 1, 2, 4 or 8. Supplying any other value will lead
 *   to undefined result.
 */
__rte_experimental
static inline void rte_power_monitor(const volatile void *p,
		const uint64_t expected_value, const uint64_t value_mask,
		const uint64_t tsc_timestamp, const uint8_t data_sz);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Monitor specific address for changes. This will cause the CPU to enter an
 * architecture-defined optimized power state until either the specified
 * memory address is written to, a certain TSC timestamp is reached, or other
 * reasons cause the CPU to wake up.
 *
 * Additionally, an `expected` 64-bit value and 64-bit mask are provided. If
 * mask is non-zero, the current value pointed to by the `p` pointer will be
 * checked against the expected value, and if they match, the entering of
 * optimized power state may be aborted.
 *
 * This call will also lock a spinlock on entering sleep, and release it on
 * waking up the CPU.
 *
 * @warning It is responsibility of the user to check if this function is
 *   supported at runtime using `rte_cpu_get_intrinsics_support()` API call.
 *   Failing to do so may result in an illegal CPU instruction error.
 *
 * @param p
 *   Address to monitor for changes.
 * @param expected_value
 *   Before attempting the monitoring, the `p` address may be read and compared
 *   against this value. If `value_mask` is zero, this step will be skipped.
 * @param value_mask
 *   The 64-bit mask to use to extract current value from `p`.
 * @param tsc_timestamp
 *   Maximum TSC timestamp to wait for. Note that the wait behavior is
 *   architecture-dependent.
 * @param data_sz
 *   Data size (in bytes) that will be used to compare expected value with the
 *   memory address. Can be 1, 2, 4 or 8. Supplying any other value will lead
 *   to undefined result.
 * @param lck
 *   A spinlock that must be locked before entering the function, will be
 *   unlocked while the CPU is sleeping, and will be locked again once the CPU
 *   wakes up.
 */
__rte_experimental
static inline void rte_power_monitor_sync(const volatile void *p,
		const uint64_t expected_value, const uint64_t value_mask,
		const uint64_t tsc_timestamp, const uint8_t data_sz,
		rte_spinlock_t *lck);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Enter an architecture-defined optimized power state until a certain TSC
 * timestamp is reached.
 *
 * @warning It is responsibility of the user to check if this function is
 *   supported at runtime using `rte_cpu_get_intrinsics_support()` API call.
 *   Failing to do so may result in an illegal CPU instruction error.
 *
 * @param tsc_timestamp
 *   Maximum TSC timestamp to wait for. Note that the wait behavior is
 *   architecture-dependent.
 */
__rte_experimental
static inline void rte_power_pause(const uint64_t tsc_timestamp);

#endif /* _RTE_POWER_INTRINSIC_H_ */
