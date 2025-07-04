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

#include <eal_export.h>
#include "eal_private.h"
#include "eal_internal_cfg.h"

#ifdef RTE_LIBEAL_USE_HPET
#warning HPET is not supported in FreeBSD
#endif

RTE_EXPORT_SYMBOL(eal_timer_source)
enum timer_source eal_timer_source = EAL_TIMER_TSC;

uint64_t
get_tsc_freq(uint64_t arch_hz)
{
	size_t sz;
	int tmp;
	uint64_t tsc_hz;

	sz = sizeof(tmp);
	tmp = 0;

	if (sysctlbyname("kern.timecounter.smp_tsc", &tmp, &sz, NULL, 0))
		EAL_LOG(WARNING, "%s", strerror(errno));
	else if (tmp != 1)
		EAL_LOG(WARNING, "TSC is not safe to use in SMP mode");

	tmp = 0;

	if (sysctlbyname("kern.timecounter.invariant_tsc", &tmp, &sz, NULL, 0))
		EAL_LOG(WARNING, "%s", strerror(errno));
	else if (tmp != 1)
		EAL_LOG(WARNING, "TSC is not invariant");

	sz = sizeof(tsc_hz);
	if (sysctlbyname("machdep.tsc_freq", &tsc_hz, &sz, NULL, 0)) {
		EAL_LOG(WARNING, "%s", strerror(errno));
		return arch_hz;
	}

	if (arch_hz && RTE_MAX(arch_hz, tsc_hz) - RTE_MIN(arch_hz, tsc_hz) > arch_hz / 100)
		EAL_LOG(WARNING, "Host tsc_freq %"PRIu64" at odds with cpu value %"PRIu64,
			tsc_hz, arch_hz);

	return tsc_hz;
}

int
rte_eal_timer_init(void)
{
	set_tsc_freq();
	return 0;
}
