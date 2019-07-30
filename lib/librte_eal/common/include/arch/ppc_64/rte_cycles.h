/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) IBM Corporation 2014.
 */

#ifndef _RTE_CYCLES_PPC_64_H_
#define _RTE_CYCLES_PPC_64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "generic/rte_cycles.h"

#include <rte_byteorder.h>
#include <rte_common.h>

/**
 * Read the time base register.
 *
 * @return
 *   The time base for this lcore.
 */
static inline uint64_t
rte_rdtsc(void)
{
	union {
		uint64_t tsc_64;
		RTE_STD_C11
		struct {
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
			uint32_t hi_32;
			uint32_t lo_32;
#else
			uint32_t lo_32;
			uint32_t hi_32;
#endif
		};
	} tsc;
	uint32_t tmp;

	asm volatile(
			"0:\n"
			"mftbu   %[hi32]\n"
			"mftb    %[lo32]\n"
			"mftbu   %[tmp]\n"
			"cmpw    %[tmp],%[hi32]\n"
			"bne     0b\n"
			: [hi32] "=r"(tsc.hi_32), [lo32] "=r"(tsc.lo_32),
			[tmp] "=r"(tmp)
		    );
	return tsc.tsc_64;
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

#endif /* _RTE_CYCLES_PPC_64_H_ */
