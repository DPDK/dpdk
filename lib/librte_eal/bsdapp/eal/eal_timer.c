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
#include <rte_eal.h>
#include <rte_debug.h>

#include "eal_private.h"
#include "eal_internal_cfg.h"

#ifdef RTE_LIBEAL_USE_HPET
#warning HPET is not supported in FreeBSD
#endif

enum timer_source eal_timer_source = EAL_TIMER_TSC;

uint64_t
get_tsc_freq(void)
{
	size_t sz;
	int tmp;
	uint64_t tsc_hz;

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

	sz = sizeof(tsc_hz);
	if (sysctlbyname("machdep.tsc_freq", &tsc_hz, &sz, NULL, 0)) {
		RTE_LOG(WARNING, EAL, "%s\n", strerror(errno));
		return 0;
	}

	return tsc_hz;
}

int
rte_eal_timer_init(void)
{
	set_tsc_freq();
	return 0;
}
