/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef _RTE_IO_X86_H_
#define _RTE_IO_X86_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_compat.h>
#include "rte_cpuflags.h"

#define RTE_NATIVE_WRITE32_WC
#include "generic/rte_io.h"

/**
 * @internal
 * MOVDIRI wrapper.
 */
static __rte_always_inline void
__rte_x86_movdiri(uint32_t value, volatile void *addr)
{
	asm volatile(
		/* MOVDIRI */
		".byte 0x40, 0x0f, 0x38, 0xf9, 0x02"
		:
		: "a" (value), "d" (addr));
}

__rte_experimental
static __rte_always_inline void
rte_write32_wc_relaxed(uint32_t value, volatile void *addr)
{
	static int _x86_movdiri_flag = -1;

	if (_x86_movdiri_flag == 1) {
		__rte_x86_movdiri(value, addr);
	} else if (_x86_movdiri_flag == 0) {
		rte_write32_relaxed(value, addr);
	} else {
		_x86_movdiri_flag =
			(rte_cpu_get_flag_enabled(RTE_CPUFLAG_MOVDIRI) > 0);
		if (_x86_movdiri_flag == 1)
			__rte_x86_movdiri(value, addr);
		else
			rte_write32_relaxed(value, addr);
	}
}

__rte_experimental
static __rte_always_inline void
rte_write32_wc(uint32_t value, volatile void *addr)
{
	/* gcc complains about calling this experimental function even
	 * when not using it. Hide it with ALLOW_EXPERIMENTAL_API.
	 */
#ifdef ALLOW_EXPERIMENTAL_API
	rte_wmb();
	rte_write32_wc_relaxed(value, addr);
#else
	rte_write32(value, addr);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IO_X86_H_ */
