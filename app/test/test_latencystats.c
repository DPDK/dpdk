/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <rte_ethdev.h>
#include <rte_latencystats.h>
#include "rte_lcore.h"
#include "rte_metrics.h"

#include "sample_packet_forward.h"
#include "test.h"

#define NUM_STATS 5
#define LATENCY_NUM_PACKETS 10
#define QUEUE_ID 0

static uint16_t portid;
static struct rte_ring *ring;

static struct rte_metric_name lat_stats_strings[NUM_STATS] = {
	{"min_latency_ns"},
	{"avg_latency_ns"},
	{"max_latency_ns"},
	{"jitter_ns"},
	{"samples"},
};

/* Test case for latency init with metrics init */
static int test_latency_init(void)
{
	int ret = 0;

	/* Metrics Initialization */
	rte_metrics_init(rte_socket_id());

	ret = rte_latencystats_init(1, NULL);
	TEST_ASSERT(ret >= 0, "Test Failed: rte_latencystats_init failed");

	return TEST_SUCCESS;
}

/* Test case to update the latency stats */
static int test_latency_update(void)
{
	int ret = 0;

	ret = rte_latencystats_update();
	TEST_ASSERT(ret >= 0, "Test Failed: rte_latencystats_update failed");

	return TEST_SUCCESS;
}

/* Test case to uninit latency stats */
static int test_latency_uninit(void)
{
	int ret = 0;

	ret = rte_latencystats_uninit();
	TEST_ASSERT(ret >= 0, "Test Failed: rte_latencystats_uninit failed");

	ret = rte_metrics_deinit();
	TEST_ASSERT(ret >= 0, "Test Failed: rte_metrics_deinit failed");

	return TEST_SUCCESS;
}

/* Test case to get names of latency stats */
static int test_latencystats_get_names(void)
{
	int ret, i;
	uint16_t size;
	struct rte_metric_name names[NUM_STATS] = { };

	/* Success Test: Valid names and size */
	size = NUM_STATS;
	ret = rte_latencystats_get_names(names, size);
	for (i = 0; i < NUM_STATS; i++) {
		if (strcmp(lat_stats_strings[i].name, names[i].name) == 0)
			printf(" %s\n", names[i].name);
		else
			printf("Failed: Names are not matched\n");
	}
	TEST_ASSERT((ret == NUM_STATS), "Test Failed to get metrics names");

	/* Failure Test: Invalid names and valid size */
	ret = rte_latencystats_get_names(NULL, size);
	TEST_ASSERT((ret == NUM_STATS), "Test Failed to get the metrics count,"
		    "Actual: %d Expected: %d", ret, NUM_STATS);

	/* Failure Test: Valid names and invalid size */
	size = 0;
	ret = rte_latencystats_get_names(names, size);
	TEST_ASSERT((ret == NUM_STATS), "Test Failed to get the metrics count,"
		    "Actual: %d Expected: %d", ret, NUM_STATS);

	return TEST_SUCCESS;
}

/* Test case to get latency stats values */
static int test_latencystats_get(void)
{
	int ret;
	uint16_t size;
	struct rte_metric_value values[NUM_STATS] = { };

	/* Success Test: Valid values and valid size */
	size = NUM_STATS;
	ret = rte_latencystats_get(values, size);
	TEST_ASSERT((ret == NUM_STATS), "Test Failed to get latency metrics"
			" values");

	/* Failure Test: Invalid values and valid size */
	ret = rte_latencystats_get(NULL, size);
	TEST_ASSERT((ret == NUM_STATS), "Test Failed to get the stats count,"
		    "Actual: %d Expected: %d", ret, NUM_STATS);

	/* Failure Test: Valid values and invalid size */
	size = 0;
	ret = rte_latencystats_get(values, size);
	TEST_ASSERT((ret == NUM_STATS), "Test Failed to get the stats count,"
		    "Actual: %d Expected: %d", ret, NUM_STATS);

	return TEST_SUCCESS;
}

static int test_latency_ring_setup(void)
{
	test_ring_setup(&ring, &portid);

	return TEST_SUCCESS;
}

static void test_latency_ring_free(void)
{
	test_ring_free(ring);
	test_vdev_uninit("net_ring_net_ringa");
}

static int test_latency_packet_forward(void)
{
	unsigned int i;
	int ret;
	struct rte_mbuf *pbuf[LATENCY_NUM_PACKETS] = { };
	struct rte_mempool *mp;
	char poolname[] = "mbuf_pool";
	uint64_t end_cycles;
	struct rte_metric_value values[NUM_STATS] = { };
	struct rte_metric_name names[NUM_STATS] = { };

	ret = test_get_mbuf_from_pool(&mp, pbuf, poolname);
	if (ret < 0) {
		printf("allocate mbuf pool Failed\n");
		return TEST_FAILED;
	}
	ret = test_dev_start(portid, mp);
	if (ret < 0) {
		printf("test_dev_start(%hu, %p) failed, error code: %d\n",
			portid, mp, ret);
		return TEST_FAILED;
	}

	ret = rte_latencystats_get_names(names, NUM_STATS);
	TEST_ASSERT((ret == NUM_STATS), "Test Failed to get metrics names");

	ret = rte_latencystats_get(values, NUM_STATS);
	TEST_ASSERT(ret == NUM_STATS, "Test failed to get results before forwarding");
	TEST_ASSERT(values[4].value == 0, "Samples not zero at start of test");

	/*
	 * Want test to run long enough to collect sufficient samples
	 * but not so long that scheduler decides to reschedule it (1000 hz).
	 */
	end_cycles = rte_rdtsc() + rte_get_tsc_hz() / 2000;
	do {
		ret = test_packet_forward(pbuf, portid, QUEUE_ID);
		if (ret < 0)
			printf("send pkts Failed\n");
	} while (rte_rdtsc() < end_cycles);

	ret = rte_latencystats_get(values, NUM_STATS);
	TEST_ASSERT(ret == NUM_STATS, "Test failed to get results after forwarding");

	for (i = 0; i < NUM_STATS; i++) {
		uint16_t k = values[i].key;

		printf("%s = %"PRIu64"\n",
		       names[k].name, values[i].value);
	}

	TEST_ASSERT(values[4].value > 0, "No samples taken");
	TEST_ASSERT(values[0].value > 0, "Min latency should not be zero");
	TEST_ASSERT(values[1].value > 0, "Avg latency should not be zero");
	TEST_ASSERT(values[2].value > 0, "Max latency should not be zero");
	TEST_ASSERT(values[0].value < values[1].value, "Min latency > Avg latency");
	TEST_ASSERT(values[0].value < values[2].value, "Min latency > Max latency");
	TEST_ASSERT(values[1].value < values[2].value, "Avg latency > Max latency");

	rte_eth_dev_stop(portid);
	test_put_mbuf_to_pool(mp, pbuf);

	return (ret >= 0) ? TEST_SUCCESS : TEST_FAILED;
}

static struct
unit_test_suite latencystats_testsuite = {
	.suite_name = "Latency Stats Unit Test Suite",
	.setup = test_latency_ring_setup,
	.teardown = test_latency_ring_free,
	.unit_test_cases = {

		/* Test Case 1: To check latency init with
		 * metrics init
		 */
		TEST_CASE_ST(NULL, NULL, test_latency_init),

		/* Test Case 2: To check whether latency stats names
		 * are retrieved
		 */
		TEST_CASE_ST(NULL, NULL, test_latencystats_get_names),

		/* Test Case 3: To check whether latency stats
		 * values are retrieved
		 */
		TEST_CASE_ST(NULL, NULL, test_latencystats_get),

		/* Test Case 4: Do packet forwarding for metrics
		 * calculation and check the latency metrics values
		 * are updated
		 */
		TEST_CASE_ST(test_latency_packet_forward, NULL,
				test_latency_update),

		/* Test Case 5: To check uninit of latency test */
		TEST_CASE_ST(NULL, NULL, test_latency_uninit),

		TEST_CASES_END()
	}
};

static int test_latencystats(void)
{
	return unit_test_suite_runner(&latencystats_testsuite);
}

REGISTER_FAST_TEST(latencystats_autotest, true, true, test_latencystats);
