/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
		__rte_unused void *key_mask,
		__rte_unused uint32_t key_size,
		__rte_unused uint64_t seed)
{
	uint32_t *k32 = key;
	uint32_t ip_dst = rte_be_to_cpu_32(k32[0]);
	uint64_t signature = ip_dst;

	return signature;
}

uint32_t pipeline_test_hash_cuckoo(const void *key,
		__rte_unused uint32_t key_size,
		__rte_unused uint32_t seed)
{
	const uint32_t *k32 = key;
	uint32_t ip_dst = rte_be_to_cpu_32(k32[0]);
	uint32_t signature = ip_dst;

	return signature;
}

static void
app_free_resources(void) {
	int i;
	for (i = 0; i < N_PORTS; i++)
		rte_ring_free(rings_rx[i]);
	rte_mempool_free(pool);
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
	int status, ret;
	unsigned i;

	ret = TEST_SUCCESS;

	app_init_rings();
	app_init_mbuf_pools();

	printf("\n\n\n\n************Pipeline tests************\n");

	if (test_table_pipeline() < 0) {
		ret = TEST_FAILED;
		goto end;
	}

	printf("\n\n\n\n************Port tests************\n");
	for (i = 0; i < n_port_tests; i++) {
		status = port_tests[i]();
		if (status < 0) {
			printf("\nPort test number %d failed (%d).\n", i,
				status);
			ret = TEST_FAILED;
			goto end;
		}
	}

	printf("\n\n\n\n************Table tests************\n");
	for (i = 0; i < n_table_tests; i++) {
		status = table_tests[i]();
		if (status < 0) {
			printf("\nTable test number %d failed (%d).\n", i,
				status);
			ret = TEST_FAILED;
			goto end;
		}
	}

	printf("\n\n\n\n************Table tests************\n");
	for (i = 0; i < n_table_tests_combined; i++) {
		status = table_tests_combined[i]();
		if (status < 0) {
			printf("\nCombined table test number %d failed with "
				"reason number %d.\n", i, status);
			ret = TEST_FAILED;
			goto end;
		}
	}

#ifdef RTE_LIBRTE_ACL
	printf("\n\n\n\n************ACL tests************\n");
	if (test_table_acl() < 0) {
		ret = TEST_FAILED;
		goto end;
	}
#endif

end:
	app_free_resources();

	return ret;
}

REGISTER_TEST_COMMAND(table_autotest, test_table);
