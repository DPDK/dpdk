/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <unistd.h>
#include <sys/sysctl.h>

#include <rte_log.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_debug.h>

#include "eal_private.h"
#include "eal_thread.h"

/* No topology information available on FreeBSD including NUMA info */
unsigned
eal_cpu_core_id(__rte_unused unsigned lcore_id)
{
	return 0;
}

static int
eal_get_ncpus(void)
{
	static int ncpu = -1;
	int mib[2] = {CTL_HW, HW_NCPU};
	size_t len = sizeof(ncpu);

	if (ncpu < 0) {
		sysctl(mib, 2, &ncpu, &len, NULL, 0);
		RTE_LOG(INFO, EAL, "Sysctl reports %d cpus\n", ncpu);
	}
	return ncpu;
}

unsigned
eal_cpu_socket_id(__rte_unused unsigned cpu_id)
{
	return 0;
}

/* Check if a cpu is present by the presence of the
 * cpu information for it.
 */
int
eal_cpu_detected(unsigned lcore_id)
{
	const unsigned ncpus = eal_get_ncpus();
	return lcore_id < ncpus;
}
