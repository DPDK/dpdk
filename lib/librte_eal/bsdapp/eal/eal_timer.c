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
#include <sys/sysctl.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_debug.h>

#include "eal_private.h"
#include "eal_internal_cfg.h"

#ifdef RTE_LIBEAL_USE_HPET
#warning HPET is not supported in FreeBSD
#endif

enum timer_source eal_timer_source = EAL_TIMER_TSC;

/* The frequency of the RDTSC timer resolution */
static uint64_t eal_tsc_resolution_hz = 0;

void
rte_delay_us(unsigned us)
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

static int
set_tsc_freq_from_sysctl(void)
{
	size_t sz;
	int tmp;

	sz = sizeof(tmp);
	tmp = 0;

	if (sysctlbyname("kern.timecounter.smp_tsc", &tmp, &sz, NULL, 0))
		RTE_LOG(WARNING, EAL, "%s\n", strerror(errno));
	else if (tmp != 1)
		RTE_LOG(WARNING, EAL, "TSC is not safe to use in SMP mode\n");

	tmp = 0;

	if (sysctlbyname("kern.timecounter.invariant_tsc", &tmp, &sz, NULL, 0))
		RTE_LOG(WARNING, EAL, "%s\n", strerror(errno));
	else if (tmp != 1)
		RTE_LOG(WARNING, EAL, "TSC is not invariant\n");

	sz = sizeof(eal_tsc_resolution_hz);
	if (sysctlbyname("machdep.tsc_freq", &eal_tsc_resolution_hz, &sz, NULL, 0)) {
		RTE_LOG(WARNING, EAL, "%s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static void
set_tsc_freq_fallback(void)
{
	RTE_LOG(WARNING, EAL, "WARNING: clock_gettime cannot use "
			"CLOCK_MONOTONIC_RAW and HPET is not available"
			" - clock timings may be less accurate.\n");
	/* assume that the sleep(1) will sleep for 1 second */
	uint64_t start = rte_rdtsc();
	sleep(1);
	eal_tsc_resolution_hz = rte_rdtsc() - start;
}

/*
 * This function measures the TSC frequency. It uses a variety of approaches.
 *
 * 1. Read the TSC frequency value provided by the kernel
 * 2. If above does not work, just sleep for 1 second and tune off that,
 *    printing a warning about inaccuracy of timing
 */
static void
set_tsc_freq(void)
{
	if (set_tsc_freq_from_sysctl() < 0)
		set_tsc_freq_fallback();

	RTE_LOG(INFO, EAL, "TSC frequency is ~%"PRIu64" KHz\n",
			eal_tsc_resolution_hz/1000);
}

int
rte_eal_timer_init(void)
{
	set_tsc_freq();
	return 0;
}
