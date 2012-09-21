/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
 *   Copyright(c) 2012-2013 6WIND.
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
 * 
 *  version: DPDK.L.1.2.3-3
 */

#ifndef _RTE_CYCLES_H_
#define _RTE_CYCLES_H_

/**
 * @file
 *
 * Simple Time Reference Functions (Cycles and HPET).
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifdef RTE_LIBRTE_EAL_VMWARE_TSC_MAP_SUPPORT
/** Global switch to use VMware mapping of TSC instead of RDTSC */
extern int rte_cycles_vmware_tsc_map;
#include <rte_branch_prediction.h>
#endif


/**
 * Read the TSC register.
 *
 * @return
 *   The TSC for this lcore.
 */
static inline uint64_t
rte_rdtsc(void)
{
	union {
		uint64_t tsc_64;
		struct {
			uint32_t lo_32;
			uint32_t hi_32;
		};
	} tsc;

#ifdef RTE_LIBRTE_EAL_VMWARE_TSC_MAP_SUPPORT
	if (unlikely(rte_cycles_vmware_tsc_map)) {
		/* ecx = 0x10000 corresponds to the Physical TSC for VMware */
		asm volatile("rdpmc" :
		             "=a" (tsc.lo_32),
		             "=d" (tsc.hi_32) :
		             "c"(0x10000));
		return tsc.tsc_64;
	}
#endif
	asm volatile("rdtsc" :
		     "=a" (tsc.lo_32),
		     "=d" (tsc.hi_32));

	return tsc.tsc_64;
}

/**
 * Return the number of HPET cycles since boot
 *
 * This counter is global for all execution units. The number of
 * cycles in one second can be retrived using rte_get_hpet_hz().
 *
 * @return
 *   the number of cycles
 */
uint64_t
rte_get_hpet_cycles(void);

/**
 * Get the number of cycles in one second.
 *
 * @return
 *   The number of cycles in one second.
 */
uint64_t
rte_get_hpet_hz(void);

/**
 * Wait at least us microseconds.
 *
 * @param us
 *   The number of microseconds to wait.
 */
void
rte_delay_us(unsigned us);

/**
 * Wait at least ms milliseconds.
 *
 * @param ms
 *   The number of milliseconds to wait.
 */
static inline void
rte_delay_ms(unsigned ms)
{
	rte_delay_us(ms * 1000);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_CYCLES_H_ */
