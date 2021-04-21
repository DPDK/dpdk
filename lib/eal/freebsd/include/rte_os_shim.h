/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _RTE_OS_SHIM_
#define _RTE_OS_SHIM_

#include <time.h>

#include <rte_os.h>

/**
 * @file
 * @internal
 * Provides semi-standard OS facilities by convenient names.
 */

#ifndef TIME_UTC

#define TIME_UTC 1

static inline int
rte_timespec_get(struct timespec *now, int base)
{
	if (base != TIME_UTC || clock_gettime(CLOCK_REALTIME, now) < 0)
		return 0;
	return base;
}

#define timespec_get(ts, base) rte_timespec_get(ts, base)

#endif /* !defined TIME_UTC */

#endif /* _RTE_OS_SHIM_ */
