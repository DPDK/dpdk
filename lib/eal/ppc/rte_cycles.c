/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) IBM Corporation 2019.
 */

#include <features.h>
#ifdef __GLIBC__
#include <sys/platform/ppc.h>
#elif RTE_EXEC_ENV_LINUX
#include <string.h>
#include <stdio.h>
#endif

#include "eal_private.h"

uint64_t
get_tsc_freq_arch(void)
{
#ifdef __GLIBC__
	return __ppc_get_timebase_freq();
#elif RTE_EXEC_ENV_LINUX
	static unsigned long base;
	char buf[512];
	ssize_t nr;
	FILE *f;

	if (base != 0)
		goto out;

	f = fopen("/proc/cpuinfo", "rb");
	if (f == NULL)
		goto out;

	while (fgets(buf, sizeof(buf), f) != NULL) {
		char *ret = strstr(buf, "timebase");

		if (ret == NULL)
			continue;
		ret += sizeof("timebase") - 1;
		ret = strchr(ret, ':');
		if (ret == NULL)
			continue;
		base = strtoul(ret + 1, NULL, 10);
		break;
	}
	fclose(f);
out:
	return (uint64_t) base;
#else
	return 0;
#endif

}
