/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Cavium, Inc
 */

#ifndef _RTE_CYCLES_ARM64_H_
#define _RTE_CYCLES_ARM64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "generic/rte_cycles.h"

/**
 * Read the time base register.
 *
 * @return
 *   The time base for this lcore.
 */
#ifndef RTE_ARM_EAL_RDTSC_USE_PMU
/**
 * This call is portable to any ARMv8 architecture, however, typically
 * cntvct_el0 runs at <= 100MHz and it may be imprecise for some tasks.
 */
static inline uint64_t
rte_rdtsc(void)
{
	uint64_t tsc;

	asm volatile("mrs %0, cntvct_el0" : "=r" (tsc));
	return tsc;
}
#else
/**
 * This is an alternative method to enable rte_rdtsc() with high resolution
 * PMU cycles counter.The cycle counter runs at cpu frequency and this scheme
 * uses ARMv8 PMU subsystem to get the cycle counter at userspace, However,
 * access to PMU cycle counter from user space is not enabled by default in
 * arm64 linux kernel.
 * It is possible to enable cycle counter at user space access by configuring
 * the PMU from the privileged mode (kernel space).
 *
 * asm volatile("msr pmintenset_el1, %0" : : "r" ((u64)(0 << 31)));
 * asm volatile("msr pmcntenset_el0, %0" :: "r" BIT(31));
 * asm volatile("msr pmuserenr_el0, %0" : : "r"(BIT(0) | BIT(2)));
 * asm volatile("mrs %0, pmcr_el0" : "=r" (val));
 * val |= (BIT(0) | BIT(2));
 * isb();
 * asm volatile("msr pmcr_el0, %0" : : "r" (val));
 *
 */
static inline uint64_t
rte_rdtsc(void)
{
	uint64_t tsc;

	asm volatile("mrs %0, pmccntr_el0" : "=r"(tsc));
	return tsc;
}
#endif

static inline uint64_t
rte_rdtsc_precise(void)
{
	asm volatile("isb" : : : "memory");
	return rte_rdtsc();
}

static inline uint64_t
rte_get_tsc_cycles(void) { return rte_rdtsc(); }

#ifdef __cplusplus
}
#endif

#endif /* _RTE_CYCLES_ARM64_H_ */
