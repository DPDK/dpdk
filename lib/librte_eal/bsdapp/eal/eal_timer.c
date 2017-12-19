/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
