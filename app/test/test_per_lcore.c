/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_cycles.h>

#include "test.h"

/*
 * Per-lcore variables and lcore launch
 * ====================================
 *
 * - Use ``rte_eal_mp_remote_launch()`` to call ``assign_vars()`` on
 *   every available lcore. In this function, a per-lcore variable is
 *   assigned to the lcore_id.
 *
 * - Use ``rte_eal_mp_remote_launch()`` to call ``display_vars()`` on
 *   every available lcore. The function checks that the variable is
 *   correctly set, or returns -1.
 *
 * - If at least one per-core variable was not correct, the test function
 *   returns -1.
 */

static RTE_DEFINE_PER_LCORE(unsigned, test) = 0x12345678;

static int
assign_vars(__rte_unused void *arg)
{
	if (RTE_PER_LCORE(test) != 0x12345678)
		return -1;
	RTE_PER_LCORE(test) = rte_lcore_id();
	return 0;
}

static int
display_vars(__rte_unused void *arg)
{
	unsigned lcore_id = rte_lcore_id();
	unsigned var = RTE_PER_LCORE(test);
	unsigned socket_id = rte_lcore_to_socket_id(lcore_id);

	printf("on socket %u, on core %u, variable is %u\n", socket_id, lcore_id, var);
	if (lcore_id != var)
		return -1;

	RTE_PER_LCORE(test) = 0x12345678;
	return 0;
}

static int
test_per_lcore_delay(__rte_unused void *arg)
{
	rte_delay_ms(100);
	printf("wait 100ms on lcore %u\n", rte_lcore_id());

	return 0;
}

static int
test_per_lcore(void)
{
	unsigned lcore_id;
	int ret;

	rte_eal_mp_remote_launch(assign_vars, NULL, SKIP_MAIN);
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	rte_eal_mp_remote_launch(display_vars, NULL, SKIP_MAIN);
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	/* test if it could do remote launch twice at the same time or not */
	ret = rte_eal_mp_remote_launch(test_per_lcore_delay, NULL, SKIP_MAIN);
	if (ret < 0) {
		printf("It fails to do remote launch but it should able to do\n");
		return -1;
	}
	/* it should not be able to launch a lcore which is running */
	ret = rte_eal_mp_remote_launch(test_per_lcore_delay, NULL, SKIP_MAIN);
	if (ret == 0) {
		printf("It does remote launch successfully but it should not at this time\n");
		return -1;
	}
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}

REGISTER_TEST_COMMAND(per_lcore_autotest, test_per_lcore);
