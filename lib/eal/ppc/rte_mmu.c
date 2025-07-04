/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) IBM Corporation 2024
 */

#include <stdio.h>
#include <string.h>

#include "eal_private.h"

bool
eal_mmu_supported(void)
{
#ifdef RTE_EXEC_ENV_LINUX
	static const char proc_cpuinfo[] = "/proc/cpuinfo";
	static const char str_mmu[] = "MMU";
	static const char str_radix[] = "Radix";
	char buf[512];
	char *ret = NULL;
	FILE *f = fopen(proc_cpuinfo, "r");

	if (f == NULL) {
		EAL_LOG(ERR, "Cannot open %s", proc_cpuinfo);
		return false;
	}

	/*
	 * Example "MMU" in /proc/cpuinfo:
	 * ...
	 * model	: 8335-GTW
	 * machine	: PowerNV 8335-GTW
	 * firmware	: OPAL
	 * MMU		: Radix
	 * ... or ...
	 * model        : IBM,9009-22A
	 * machine      : CHRP IBM,9009-22A
	 * MMU          : Hash
	 */
	while (fgets(buf, sizeof(buf), f) != NULL) {
		ret = strstr(buf, str_mmu);
		if (ret == NULL)
			continue;
		ret += sizeof(str_mmu) - 1;
		ret = strchr(ret, ':');
		if (ret == NULL)
			continue;
		ret = strstr(ret, str_radix);
		break;
	}
	fclose(f);

	if (ret == NULL)
		EAL_LOG(ERR, "DPDK on PPC64 requires radix-mmu");

	return (ret != NULL);
#elif RTE_EXEC_ENV_FREEBSD
	/*
	 * Method to detect MMU type in FreeBSD not known at the moment.
	 * Return true for now to emulate previous behavior and
	 * avoid unnecessary failures.
	 */
	return true;
#else
	/* Force false for other execution environments. */
	return false;
#endif
}
