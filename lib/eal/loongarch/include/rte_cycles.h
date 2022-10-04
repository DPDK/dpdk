/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Loongson Technology Corporation Limited
 */

#ifndef RTE_CYCLES_LOONGARCH_H
#define RTE_CYCLES_LOONGARCH_H

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
static inline uint64_t
rte_rdtsc(void)
{
	uint64_t count;

	__asm__ __volatile__ (
		"rdtime.d %[cycles], $zero\n"
		: [cycles] "=r" (count)
		::
		);
	return count;
}

static inline uint64_t
rte_rdtsc_precise(void)
{
	rte_mb();
	return rte_rdtsc();
}

static inline uint64_t
rte_get_tsc_cycles(void) { return rte_rdtsc(); }

#ifdef __cplusplus
}
#endif

#endif /* RTE_CYCLES_LOONGARCH_H */
