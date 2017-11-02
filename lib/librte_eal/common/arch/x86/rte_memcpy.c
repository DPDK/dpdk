/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
