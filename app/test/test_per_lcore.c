/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
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
assign_vars(__attribute__((unused)) void *arg)
{
	if (RTE_PER_LCORE(test) != 0x12345678)
		return -1;
	RTE_PER_LCORE(test) = rte_lcore_id();
	return 0;
}

static int
display_vars(__attribute__((unused)) void *arg)
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
test_per_lcore_delay(__attribute__((unused)) void *arg)
{
	rte_delay_ms(5000);
	printf("wait 5000ms on lcore %u\n", rte_lcore_id());

	return 0;
}

int
test_per_lcore(void)
{
	unsigned lcore_id;
	int ret;

	rte_eal_mp_remote_launch(assign_vars, NULL, SKIP_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	rte_eal_mp_remote_launch(display_vars, NULL, SKIP_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	/* test if it could do remote launch twice at the same time or not */
	ret = rte_eal_mp_remote_launch(test_per_lcore_delay, NULL, SKIP_MASTER);
	if (ret < 0) {
		printf("It fails to do remote launch but it should able to do\n");
		return -1;
	}
	/* it should not be able to launch a lcore which is running */
	ret = rte_eal_mp_remote_launch(test_per_lcore_delay, NULL, SKIP_MASTER);
	if (ret == 0) {
		printf("It does remote launch successfully but it should not at this time\n");
		return -1;
	}
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
