/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2012-2013 6WIND S.A.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_thread.h>

#include <eal_export.h>
#include "eal_private.h"

RTE_EXPORT_SYMBOL(eal_timer_source)
enum timer_source eal_timer_source = EAL_TIMER_HPET;

/* Check if the kernel deems the arch provided TSC frequency trustworthy. */

static bool
is_tsc_known_freq(void)
{
	bool ret = true; /* Assume tsc_known_freq */

#if defined(RTE_ARCH_X86)
	char line[2048];
	FILE *stream;

	stream = fopen("/proc/cpuinfo", "r");
	if (!stream) {
		EAL_LOG(WARNING, "Unable to open /proc/cpuinfo");
		return ret;
	}

	while (fgets(line, sizeof(line), stream)) {
		if (strncmp(line, "flags", 5) != 0)
			continue;

		if (!strstr(line, "tsc_known_freq"))
			ret = false;

		break;
	}

	fclose(stream);
#endif

	return ret;
}

uint64_t
get_tsc_freq(uint64_t arch_hz)
{
#ifdef CLOCK_MONOTONIC_RAW
#define NS_PER_SEC 1E9
#define CYC_PER_100KHZ 1E5

	struct timespec sleeptime = {.tv_nsec = NS_PER_SEC / 10 }; /* 1/10 second */

	struct timespec t_start, t_end;
	uint64_t tsc_hz;

	if (arch_hz && is_tsc_known_freq())
		return arch_hz;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &t_start) == 0) {
		uint64_t ns, end, start = rte_rdtsc();
		nanosleep(&sleeptime,NULL);
		clock_gettime(CLOCK_MONOTONIC_RAW, &t_end);
		end = rte_rdtsc();
		ns = ((t_end.tv_sec - t_start.tv_sec) * NS_PER_SEC);
		ns += (t_end.tv_nsec - t_start.tv_nsec);

		double secs = (double)ns/NS_PER_SEC;
		tsc_hz = (uint64_t)((end - start)/secs);

		if (arch_hz) {
			/* Make sure we're within 1% for sanity check */
			if (RTE_MAX(arch_hz, tsc_hz) - RTE_MIN(arch_hz, tsc_hz) > arch_hz / 100)
				return arch_hz;

			EAL_LOG(DEBUG,
				"Refined arch frequency %"PRIu64" to measured frequency %"PRIu64,
				arch_hz, tsc_hz);
		}

		/* Round up to 100Khz. 1E5 ~ 100Khz */
		return RTE_ALIGN_MUL_NEAR(tsc_hz, CYC_PER_100KHZ);
	}
#endif
	return arch_hz;
}

int
rte_eal_timer_init(void)
{

	eal_timer_source = EAL_TIMER_TSC;

	set_tsc_freq();
	return 0;
}
