/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_common.h>

 /* Get the cpu core id value */
unsigned int
eal_cpu_core_id(unsigned int lcore_id)
{
	/* TODO */
	/* This is a stub, not the expected result */
	return lcore_id;
}

/* Check if a cpu is present by the presence of the cpu information for it */
int
eal_cpu_detected(unsigned int lcore_id __rte_unused)
{
	/* TODO */
	/* This is a stub, not the expected result */
	return 1;
}

/* Get CPU socket id (NUMA node) for a logical core */
unsigned int
eal_cpu_socket_id(unsigned int cpu_id __rte_unused)
{
	/* TODO */
	/* This is a stub, not the expected result */
	return 0;
}
