/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
/*   BSD LICENSE
 *
 *   Copyright(c) 2013 6WIND.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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
#include <rte_debug.h>
#include <rte_atomic.h>

#ifdef RTE_LIBRTE_EAL_VMWARE_TSC_MAP_SUPPORT
/** Global switch to use VMWARE mapping of TSC instead of RDTSC */
extern int rte_cycles_vmware_tsc_map;
#include <rte_branch_prediction.h>
#endif

#define MS_PER_S 1000
#define US_PER_S 1000000
#define NS_PER_S 1000000000

enum timer_source {
	EAL_TIMER_TSC = 0,
	EAL_TIMER_HPET
};
extern enum timer_source eal_timer_source;

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
		/* ecx = 0x10000 corresponds to the physical TSC for VMware */
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
 * Read the TSC register precisely where function is called.
 *
 * @return
 *   The TSC for this lcore.
 */
static inline uint64_t
rte_rdtsc_precise(void)
{
	rte_mb();
	return rte_rdtsc();
}

/**
 * Get the measured frequency of the RDTSC counter
 *
 * @return
 *   The TSC frequency for this lcore
 */
uint64_t
rte_get_tsc_hz(void);

/**
 * Return the number of TSC cycles since boot
 *
  * @return
 *   the number of cycles
 */
static inline uint64_t
rte_get_tsc_cycles(void) { return rte_rdtsc(); }

#ifdef RTE_LIBEAL_USE_HPET
/**
 * Return the number of HPET cycles since boot
 *
 * This counter is global for all execution units. The number of
 * cycles in one second can be retrieved using rte_get_hpet_hz().
 *
 * @return
 *   the number of cycles
 */
uint64_t
rte_get_hpet_cycles(void);

/**
 * Get the number of HPET cycles in one second.
 *
 * @return
 *   The number of cycles in one second.
 */
uint64_t
rte_get_hpet_hz(void);

/**
 * Initialise the HPET for use. This must be called before the rte_get_hpet_hz
 * and rte_get_hpet_cycles APIs are called. If this function does not succeed,
 * then the HPET functions are unavailable and should not be called.
 *
 * @param make_default
 * 	If set, the hpet timer becomes the default timer whose values are
 * 	returned by the rte_get_timer_hz/cycles API calls
 *
 * @return
 * 	0 on success,
 * 	-1 on error, and the make_default parameter is ignored.
 */
int rte_eal_hpet_init(int make_default);

#endif

/**
 * Get the number of cycles since boot from the default timer.
 *
 * @return
 *   The number of cycles
 */
static inline uint64_t
rte_get_timer_cycles(void)
{
	switch(eal_timer_source) {
	case EAL_TIMER_TSC:
		return rte_rdtsc();
	case EAL_TIMER_HPET:
#ifdef RTE_LIBEAL_USE_HPET
		return rte_get_hpet_cycles();
#endif
	default: rte_panic("Invalid timer source specified\n");
	}
}

/**
 * Get the number of cycles in one second for the default timer.
 *
 * @return
 *   The number of cycles in one second.
 */
static inline uint64_t
rte_get_timer_hz(void)
{
	switch(eal_timer_source) {
	case EAL_TIMER_TSC:
		return rte_get_tsc_hz();
	case EAL_TIMER_HPET:
#ifdef RTE_LIBEAL_USE_HPET
		return rte_get_hpet_hz();
#endif
	default: rte_panic("Invalid timer source specified\n");
	}
}

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
