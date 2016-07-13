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

#include <rte_byteorder.h>
#include <rte_hexdump.h>
#include <rte_string_fns.h>
#include <string.h>
#include "test.h"
#include "test_table.h"
#include "test_table_pipeline.h"
#include "test_table_ports.h"
#include "test_table_tables.h"
#include "test_table_combined.h"
#include "test_table_acl.h"

/* Global variables */
struct rte_pipeline *p;
struct rte_ring *rings_rx[N_PORTS];
struct rte_ring *rings_tx[N_PORTS];
struct rte_mempool *pool = NULL;

uint32_t port_in_id[N_PORTS];
uint32_t port_out_id[N_PORTS];
uint32_t port_out_id_type[3];
uint32_t table_id[N_PORTS*2];
uint64_t override_hit_mask = 0xFFFFFFFF;
uint64_t override_miss_mask = 0xFFFFFFFF;
uint64_t non_reserved_actions_hit = 0;
uint64_t non_reserved_actions_miss = 0;
uint8_t connect_miss_action_to_port_out = 0;
uint8_t connect_miss_action_to_table = 0;
uint32_t table_entry_default_action = RTE_PIPELINE_ACTION_DROP;
uint32_t table_entry_hit_action = RTE_PIPELINE_ACTION_PORT;
uint32_t table_entry_miss_action = RTE_PIPELINE_ACTION_DROP;
rte_pipeline_port_in_action_handler port_in_action = NULL;
rte_pipeline_port_out_action_handler port_out_action = NULL;
rte_pipeline_table_action_handler_hit action_handler_hit = NULL;
rte_pipeline_table_action_handler_miss action_handler_miss = NULL;

/* Function prototypes */
static void app_init_rings(void);
static void app_init_mbuf_pools(void);

uint64_t pipeline_test_hash(void *key,
		__attribute__((unused)) uint32_t key_size,
		__attribute__((unused)) uint64_t seed)
{
	uint32_t *k32 = (uint32_t *) key;
	uint32_t ip_dst = rte_be_to_cpu_32(k32[0]);
	uint64_t signature = ip_dst;

	return signature;
}

static void
app_init_mbuf_pools(void)
{
	/* Init the buffer pool */
	printf("Getting/Creating the mempool ...\n");
	pool = rte_mempool_lookup("mempool");
	if (!pool) {
		pool = rte_pktmbuf_pool_create(
			"mempool",
			POOL_SIZE,
			POOL_CACHE_SIZE, 0, POOL_BUFFER_SIZE,
			0);
		if (pool == NULL)
			rte_panic("Cannot create mbuf pool\n");
	}
}

static void
app_init_rings(void)
{
	uint32_t i;

	for (i = 0; i < N_PORTS; i++) {
		char name[32];

		snprintf(name, sizeof(name), "app_ring_rx_%u", i);
		rings_rx[i] = rte_ring_lookup(name);
		if (rings_rx[i] == NULL) {
			rings_rx[i] = rte_ring_create(
				name,
				RING_RX_SIZE,
				0,
				RING_F_SP_ENQ | RING_F_SC_DEQ);
		}
		if (rings_rx[i] == NULL)
			rte_panic("Cannot create RX ring %u\n", i);
	}

	for (i = 0; i < N_PORTS; i++) {
		char name[32];

		snprintf(name, sizeof(name), "app_ring_tx_%u", i);
		rings_tx[i] = rte_ring_lookup(name);
		if (rings_tx[i] == NULL) {
			rings_tx[i] = rte_ring_create(
				name,
				RING_TX_SIZE,
				0,
				RING_F_SP_ENQ | RING_F_SC_DEQ);
		}
		if (rings_tx[i] == NULL)
			rte_panic("Cannot create TX ring %u\n", i);
	}

}

static int
test_table(void)
{
	int status, failures;
	unsigned i;

	failures = 0;

	app_init_rings();
	app_init_mbuf_pools();

	printf("\n\n\n\n************Pipeline tests************\n");

	if (test_table_pipeline() < 0)
		return -1;

	printf("\n\n\n\n************Port tests************\n");
	for (i = 0; i < n_port_tests; i++) {
		status = port_tests[i]();
		if (status < 0) {
			printf("\nPort test number %d failed (%d).\n", i,
				status);
			failures++;
			return -1;
		}
	}

	printf("\n\n\n\n************Table tests************\n");
	for (i = 0; i < n_table_tests; i++) {
		status = table_tests[i]();
		if (status < 0) {
			printf("\nTable test number %d failed (%d).\n", i,
				status);
			failures++;
			return -1;
		}
	}

	printf("\n\n\n\n************Table tests************\n");
	for (i = 0; i < n_table_tests_combined; i++) {
		status = table_tests_combined[i]();
		if (status < 0) {
			printf("\nCombined table test number %d failed with "
				"reason number %d.\n", i, status);
			failures++;
			return -1;
		}
	}

	if (failures)
		return -1;

#ifdef RTE_LIBRTE_ACL
	printf("\n\n\n\n************ACL tests************\n");
	if (test_table_acl() < 0)
		return -1;
#endif

	return 0;
}

REGISTER_TEST_COMMAND(table_autotest, test_table);
