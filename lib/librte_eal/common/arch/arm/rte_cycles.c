/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Cavium, Inc
 */

#include "eal_private.h"

uint64_t
get_tsc_freq_arch(void)
{
#if defined RTE_ARCH_ARM64 && !defined RTE_ARM_EAL_RDTSC_USE_PMU
	uint64_t freq;
	asm volatile("mrs %0, cntfrq_el0" : "=r" (freq));
	return freq;
#else
	return 0;
#endif
}
