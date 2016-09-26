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

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_cycles.h>

#include "eal_private.h"

/* The frequency of the RDTSC timer resolution */
static uint64_t eal_tsc_resolution_hz;

/* Pointer to user delay function */
void (*rte_delay_us)(unsigned int) = NULL;

void
rte_delay_us_block(unsigned int us)
{
	const uint64_t start = rte_get_timer_cycles();
	const uint64_t ticks = (uint64_t)us * rte_get_timer_hz() / 1E6;
	while ((rte_get_timer_cycles() - start) < ticks)
		rte_pause();
}

uint64_t
rte_get_tsc_hz(void)
{
	return eal_tsc_resolution_hz;
}

static uint64_t
estimate_tsc_freq(void)
{
	RTE_LOG(WARNING, EAL, "WARNING: TSC frequency estimated roughly"
		" - clock timings may be less accurate.\n");
	/* assume that the sleep(1) will sleep for 1 second */
	uint64_t start = rte_rdtsc();
	sleep(1);
	return rte_rdtsc() - start;
}

void
set_tsc_freq(void)
{
	uint64_t freq = get_tsc_freq();

	if (!freq)
		freq = estimate_tsc_freq();

	RTE_LOG(DEBUG, EAL, "TSC frequency is ~%" PRIu64 " KHz\n", freq / 1000);
	eal_tsc_resolution_hz = freq;
}

void rte_delay_us_callback_register(void (*userfunc)(unsigned int))
{
	rte_delay_us = userfunc;
}

static void __attribute__((constructor))
rte_timer_init(void)
{
	/* set rte_delay_us_block as a delay function */
	rte_delay_us_callback_register(rte_delay_us_block);
}
