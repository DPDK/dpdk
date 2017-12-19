/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <rte_memcpy.h>
#include <rte_cpuflags.h>
#include <rte_log.h>

void *(*rte_memcpy_ptr)(void *dst, const void *src, size_t n) = NULL;

RTE_INIT(rte_memcpy_init)
{
#ifdef CC_SUPPORT_AVX512F
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F)) {
		rte_memcpy_ptr = rte_memcpy_avx512f;
		RTE_LOG(DEBUG, EAL, "AVX512 memcpy is using!\n");
		return;
	}
#endif
#ifdef CC_SUPPORT_AVX2
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2)) {
		rte_memcpy_ptr = rte_memcpy_avx2;
		RTE_LOG(DEBUG, EAL, "AVX2 memcpy is using!\n");
		return;
	}
#endif
	rte_memcpy_ptr = rte_memcpy_sse;
	RTE_LOG(DEBUG, EAL, "Default SSE/AVX memcpy is using!\n");
}
