/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>
#include <rte_lcore.h>
#include <rte_memzone.h>
#include <rte_metrics.h>
#include <rte_bitrate.h>

#include "sample_packet_forward.h"
#include "test.h"

#define BIT_NUM_PACKETS 10
#define QUEUE_ID 0

static uint16_t portid;
static struct rte_stats_bitrates *bitrate_data;
static struct rte_ring *ring;

/* To test whether rte_stats_bitrate_create is successful */
static int
test_stats_bitrate_create(void)
{
	bitrate_data = rte_stats_bitrate_create();
	TEST_ASSERT(bitrate_data != NULL, "rte_stats_bitrate_create failed");

	return TEST_SUCCESS;
}

/* To test free the resources from bitrate_reg test */
static int
test_stats_bitrate_free(void)
{
	int ret = 0;

	ret = rte_metrics_deinit();
	TEST_ASSERT(ret >= 0, "Test Failed: rte_metrics_deinit failed");

	return TEST_SUCCESS;
}

/* To test bit rate registration */
static int
test_stats_bitrate_reg(void)
{
	int ret = 0;

	/* Test to register bit rate without metrics init */
	ret = rte_stats_bitrate_reg(bitrate_data);
	TEST_ASSERT(ret < 0, "Test Failed: rte_stats_bitrate_reg succeeded "
			"without metrics init, ret:%d", ret);

	/* Metrics initialization */
	rte_metrics_init(rte_socket_id());
	/* Test to register bit rate after metrics init */
	ret = rte_stats_bitrate_reg(bitrate_data);
	TEST_ASSERT((ret >= 0), "Test Failed: rte_stats_bitrate_reg %d", ret);

	return TEST_SUCCESS;
}

/* To test the bit rate registration with invalid pointer */
static int
test_stats_bitrate_reg_invalidpointer(void)
{
	int ret = 0;

	ret = rte_stats_bitrate_reg(NULL);
	TEST_ASSERT(ret < 0, "Test Failed: Expected failure < 0 but "
			"got %d", ret);

	return TEST_SUCCESS;
}

/* To test bit rate calculation with invalid bit rate data pointer */
static int
test_stats_bitrate_calc_invalid_bitrate_data(void)
{
	int ret = 0;

	ret = rte_stats_bitrate_calc(NULL, portid);
	TEST_ASSERT(ret < 0, "Test Failed: rte_stats_bitrate_calc "
			"ret:%d", ret);

	return TEST_SUCCESS;
}

/* To test the bit rate calculation with invalid portid
 * (higher than max ports)
 */
static int
test_stats_bitrate_calc_invalid_portid_1(void)
{
	int ret = 0;

	ret = rte_stats_bitrate_calc(bitrate_data, 33);
	TEST_ASSERT(ret == -EINVAL, "Test Failed: Expected -%d for higher "
			"portid rte_stats_bitrate_calc ret:%d", EINVAL, ret);

	return TEST_SUCCESS;
}

/* To test the bit rate calculation with invalid portid (lesser than 0) */
static int
test_stats_bitrate_calc_invalid_portid_2(void)
{
	int ret = 0;

	ret = rte_stats_bitrate_calc(bitrate_data, -1);
	TEST_ASSERT(ret == -EINVAL, "Test Failed: Expected -%d for invalid "
			"portid rte_stats_bitrate_calc ret:%d", EINVAL, ret);

	return TEST_SUCCESS;
}

/* To test the bit rate calculation with non-existing portid */
static int
test_stats_bitrate_calc_non_existing_portid(void)
{
	int ret = 0;

	ret = rte_stats_bitrate_calc(bitrate_data, 31);
	TEST_ASSERT(ret ==  -EINVAL, "Test Failed: Expected -%d for "
			"non-existing portid rte_stats_bitrate_calc ret:%d",
			EINVAL, ret);

	return TEST_SUCCESS;
}

/* To test the bit rate calculation with valid bit rate data, valid portid */
static int
test_stats_bitrate_calc(void)
{
	int ret = 0;

	ret = rte_stats_bitrate_calc(bitrate_data, portid);
	TEST_ASSERT(ret >= 0, "Test Failed: Expected >=0 for valid portid "
			"rte_stats_bitrate_calc ret:%d", ret);

	return TEST_SUCCESS;
}

static int
test_bit_packet_forward(void)
{
	int ret;
	struct rte_mbuf *pbuf[BIT_NUM_PACKETS] = { };
	struct rte_mempool *mp;
	char poolname[] = "mbuf_pool";
	ret = test_get_mbuf_from_pool(&mp, pbuf, poolname);
	if (ret < 0) {
		printf("allocate mbuf pool Failed\n");
		return TEST_FAILED;
	}
	ret = test_packet_forward(pbuf, portid, QUEUE_ID);
	if (ret < 0)
		printf("send pkts Failed\n");
	test_put_mbuf_to_pool(mp, pbuf);

	return TEST_SUCCESS;
}

static int
test_bit_ring_setup(void)
{
	test_ring_setup(&ring, &portid);
	printf("port in ring setup : %d\n", portid);

	return TEST_SUCCESS;
}

static void
test_bit_ring_free(void)
{
	test_ring_free(ring);
	test_vdev_uninit("net_ring_net_ringa");
	rte_memzone_free(rte_memzone_lookup("RTE_METRICS"));
}

static struct
unit_test_suite bitratestats_testsuite  = {
	.suite_name = "BitRate Stats Unit Test Suite",
	.setup = test_bit_ring_setup,
	.teardown = test_bit_ring_free,
	.unit_test_cases = {
		/* TEST CASE 1: Test to create bit rate data */
		TEST_CASE(test_stats_bitrate_create),

		/* TEST CASE 2: Test to register bit rate metrics
		 * without metrics init and after metrics init
		 */
		TEST_CASE(test_stats_bitrate_reg),

		/* TEST CASE 3: Test to register bit rate metrics
		 * with invalid bit rate data
		 */
		TEST_CASE(test_stats_bitrate_reg_invalidpointer),

		/* TEST CASE 4: Test to calculate bit rate data metrics
		 * with invalid bit rate data
		 */
		TEST_CASE(test_stats_bitrate_calc_invalid_bitrate_data),

		/* TEST CASE 5: Test to calculate bit rate data metrics
		 * with portid exceeding the max ports
		 */
		TEST_CASE(test_stats_bitrate_calc_invalid_portid_1),

		/* TEST CASE 6: Test to calculate bit rate data metrics
		 * with portid less than 0
		 */
		TEST_CASE(test_stats_bitrate_calc_invalid_portid_2),

		/* TEST CASE 7: Test to calculate bit rate data metrics
		 * with non-existing portid
		 */
		TEST_CASE(test_stats_bitrate_calc_non_existing_portid),

		/* TEST CASE 8: Test to calculate bit rate data metrics
		 * with valid portid, valid bit rate data
		 */
		TEST_CASE_ST(test_bit_packet_forward, NULL,
				test_stats_bitrate_calc),
		/* TEST CASE 9: Test to do the cleanup w.r.t create */
		TEST_CASE(test_stats_bitrate_free),
		TEST_CASES_END()
	}
};

static int
test_bitratestats(void)
{
	return unit_test_suite_runner(&bitratestats_testsuite);
}
REGISTER_TEST_COMMAND(bitratestats_autotest, test_bitratestats);
