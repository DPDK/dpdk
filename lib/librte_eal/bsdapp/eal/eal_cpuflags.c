/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <rte_common.h>
#include <rte_cpuflags.h>

unsigned long
rte_cpu_getauxval(unsigned long type __rte_unused)
{
	/* not implemented */
	return 0;
}

int
rte_cpu_strcmp_auxval(unsigned long type __rte_unused,
		const char *str __rte_unused)
{
	/* not implemented */
	return -1;
}
