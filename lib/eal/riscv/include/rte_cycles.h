/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Cavium, Inc
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 */

#ifndef RTE_CYCLES_RISCV_H
#define RTE_CYCLES_RISCV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "generic/rte_cycles.h"

#ifndef RTE_RISCV_RDTSC_USE_HPM
#define RTE_RISCV_RDTSC_USE_HPM 0
#endif

/** Read wall time counter */
static __rte_always_inline uint64_t
__rte_riscv_rdtime(void)
{
	uint64_t tsc;
	asm volatile("csrr %0, time" : "=r" (tsc) : : "memory");
	return tsc;
}

/** Read wall time counter ensuring no re-ordering */
static __rte_always_inline uint64_t
__rte_riscv_rdtime_precise(void)
{
	asm volatile("fence" : : : "memory");
	return __rte_riscv_rdtime();
}

/** Read hart cycle counter */
static __rte_always_inline uint64_t
__rte_riscv_rdcycle(void)
{
	uint64_t tsc;
	asm volatile("csrr %0, cycle" : "=r" (tsc) : : "memory");
	return tsc;
}

/** Read hart cycle counter ensuring no re-ordering */
static __rte_always_inline uint64_t
__rte_riscv_rdcycle_precise(void)
{
	asm volatile("fence" : : : "memory");
	return __rte_riscv_rdcycle();
}

/**
 * Read the time base register.
 *
 * @return
 *   The time base for this lcore.
 */
static __rte_always_inline uint64_t
rte_rdtsc(void)
{
	/**
	 * By default TIME userspace counter is used. It is stable and shared
	 * across cores. Although it's frequency may not be enough for all
	 * applications.
	 */
	if (!RTE_RISCV_RDTSC_USE_HPM)
		return __rte_riscv_rdtime();
	/**
	 * Alternatively HPM's CYCLE counter may be used. However this counter
	 * is not guaranteed by ISA to either be stable frequency or always
	 * enabled for userspace access (it may trap to kernel or firmware,
	 * though as of Linux kernel 5.13 it doesn't).
	 * It is also highly probable that values of this counter are not
	 * synchronized across cores. Therefore if it is to be used as a timer,
	 * it can only be used in the scope of a single core.
	 */
	return __rte_riscv_rdcycle();
}

static inline uint64_t
rte_rdtsc_precise(void)
{
	if (!RTE_RISCV_RDTSC_USE_HPM)
		return __rte_riscv_rdtime_precise();
	return __rte_riscv_rdcycle_precise();
}

static __rte_always_inline uint64_t
rte_get_tsc_cycles(void)
{
	return rte_rdtsc();
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_CYCLES_RISCV_H */
