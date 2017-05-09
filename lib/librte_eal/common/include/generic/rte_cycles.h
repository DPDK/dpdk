/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright(c) 2013 6WIND.
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

#ifndef _RTE_CYCLES_H_
#define _RTE_CYCLES_H_

/**
 * @file
 *
 * Simple Time Reference Functions (Cycles and HPET).
 */

#include <stdint.h>
#include <rte_debug.h>
#include <rte_atomic.h>

#define MS_PER_S 1000
#define US_PER_S 1000000
#define NS_PER_S 1000000000

enum timer_source {
	EAL_TIMER_TSC = 0,
	EAL_TIMER_HPET
};
extern enum timer_source eal_timer_source;

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
rte_get_tsc_cycles(void);

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
 *	If set, the hpet timer becomes the default timer whose values are
 *	returned by the rte_get_timer_hz/cycles API calls
 *
 * @return
 *	0 on success,
 *	-1 on error, and the make_default parameter is ignored.
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
#ifdef RTE_LIBEAL_USE_HPET
	switch(eal_timer_source) {
	case EAL_TIMER_TSC:
#endif
		return rte_get_tsc_cycles();
#ifdef RTE_LIBEAL_USE_HPET
	case EAL_TIMER_HPET:
		return rte_get_hpet_cycles();
	default: rte_panic("Invalid timer source specified\n");
	}
#endif
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
#ifdef RTE_LIBEAL_USE_HPET
	switch(eal_timer_source) {
	case EAL_TIMER_TSC:
#endif
		return rte_get_tsc_hz();
#ifdef RTE_LIBEAL_USE_HPET
	case EAL_TIMER_HPET:
		return rte_get_hpet_hz();
	default: rte_panic("Invalid timer source specified\n");
	}
#endif
}
/**
 * Wait at least us microseconds.
 * This function can be replaced with user-defined function.
 * @see rte_delay_us_callback_register
 *
 * @param us
 *   The number of microseconds to wait.
 */
extern void
(*rte_delay_us)(unsigned int us);

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

/**
 * Blocking delay function.
 *
 * @param us
 *   Number of microseconds to wait.
 */
void rte_delay_us_block(unsigned int us);

/**
 * Replace rte_delay_us with user defined function.
 *
 * @param userfunc
 *   User function which replaces rte_delay_us. rte_delay_us_block restores
 *   buildin block delay function.
 */
void rte_delay_us_callback_register(void(*userfunc)(unsigned int));

#endif /* _RTE_CYCLES_H_ */
