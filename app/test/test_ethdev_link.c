/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 */

#include <rte_log.h>
#include <rte_ethdev.h>

#include <rte_test.h>
#include "test.h"


static int32_t
test_link_status_up_default(void)
{
	int ret = 0;
	struct rte_eth_link link_status = {
		.link_speed = RTE_ETH_SPEED_NUM_2_5G,
		.link_status = RTE_ETH_LINK_UP,
		.link_autoneg = RTE_ETH_LINK_AUTONEG,
		.link_duplex = RTE_ETH_LINK_FULL_DUPLEX
	};
	char text[RTE_ETH_LINK_MAX_STR_LEN];

	ret = rte_eth_link_to_str(text, sizeof(text), &link_status);
	RTE_TEST_ASSERT(ret > 0, "Failed to format default string\n");
	printf("Default link up #1: %s\n", text);
	TEST_ASSERT_BUFFERS_ARE_EQUAL("Link up at 2.5 Gbps FDX Autoneg",
		text, strlen(text), "Invalid default link status string");

	link_status.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
	link_status.link_autoneg = RTE_ETH_LINK_FIXED;
	link_status.link_speed = RTE_ETH_SPEED_NUM_10M;
	ret = rte_eth_link_to_str(text, sizeof(text), &link_status);
	printf("Default link up #2: %s\n", text);
	RTE_TEST_ASSERT(ret > 0, "Failed to format default string\n");
	TEST_ASSERT_BUFFERS_ARE_EQUAL("Link up at 10 Mbps HDX Fixed",
		text, strlen(text), "Invalid default link status "
		"string with HDX");

	link_status.link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
	ret = rte_eth_link_to_str(text, sizeof(text), &link_status);
	printf("Default link up #3: %s\n", text);
	RTE_TEST_ASSERT(ret > 0, "Failed to format default string\n");
	TEST_ASSERT_BUFFERS_ARE_EQUAL("Link up at Unknown HDX Fixed",
		text, strlen(text), "Invalid default link status "
		"string with HDX");

	link_status.link_speed = RTE_ETH_SPEED_NUM_NONE;
	ret = rte_eth_link_to_str(text, sizeof(text), &link_status);
	printf("Default link up #3: %s\n", text);
	RTE_TEST_ASSERT(ret > 0, "Failed to format default string\n");
	TEST_ASSERT_BUFFERS_ARE_EQUAL("Link up at None HDX Fixed",
		text, strlen(text), "Invalid default link status "
		"string with HDX");

	/* test max str len */
	link_status.link_speed = RTE_ETH_SPEED_NUM_200G;
	link_status.link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
	link_status.link_autoneg = RTE_ETH_LINK_AUTONEG;
	ret = rte_eth_link_to_str(text, sizeof(text), &link_status);
	printf("Default link up #4:len = %d, %s\n", ret, text);
	RTE_TEST_ASSERT(ret < RTE_ETH_LINK_MAX_STR_LEN,
		"String length exceeds max allowed value\n");
	return TEST_SUCCESS;
}

static int32_t
test_link_status_down_default(void)
{
	int ret = 0;
	struct rte_eth_link link_status = {
		.link_speed = RTE_ETH_SPEED_NUM_2_5G,
		.link_status = RTE_ETH_LINK_DOWN,
		.link_autoneg = RTE_ETH_LINK_AUTONEG,
		.link_duplex = RTE_ETH_LINK_FULL_DUPLEX
	};
	char text[RTE_ETH_LINK_MAX_STR_LEN];

	ret = rte_eth_link_to_str(text, sizeof(text), &link_status);
	RTE_TEST_ASSERT(ret > 0, "Failed to format default string\n");
	TEST_ASSERT_BUFFERS_ARE_EQUAL("Link down",
		text, strlen(text), "Invalid default link status string");

	return TEST_SUCCESS;
}

static int32_t
test_link_status_invalid(void)
{
	int ret = 0;
	struct rte_eth_link link_status = {
		.link_speed = 55555,
		.link_status = RTE_ETH_LINK_UP,
		.link_autoneg = RTE_ETH_LINK_AUTONEG,
		.link_duplex = RTE_ETH_LINK_FULL_DUPLEX
	};
	char text[RTE_ETH_LINK_MAX_STR_LEN];

	ret = rte_eth_link_to_str(text, sizeof(text), &link_status);
	RTE_TEST_ASSERT(ret < RTE_ETH_LINK_MAX_STR_LEN,
		"Failed to format invalid string\n");
	printf("invalid link up #1: len=%d %s\n", ret, text);
	TEST_ASSERT_BUFFERS_ARE_EQUAL("Link up at Invalid FDX Autoneg",
		text, strlen(text), "Incorrect invalid link status string");

	return TEST_SUCCESS;
}


static int32_t
test_link_speed_all_values(void)
{
	const char *speed;
	uint32_t i;
	struct link_speed_t {
		const char *value;
		uint32_t link_speed;
	} speed_str_map[] = {
		{ "None",   RTE_ETH_SPEED_NUM_NONE },
		{ "10 Mbps",  RTE_ETH_SPEED_NUM_10M },
		{ "100 Mbps", RTE_ETH_SPEED_NUM_100M },
		{ "1 Gbps",   RTE_ETH_SPEED_NUM_1G },
		{ "2.5 Gbps", RTE_ETH_SPEED_NUM_2_5G },
		{ "5 Gbps",   RTE_ETH_SPEED_NUM_5G },
		{ "10 Gbps",  RTE_ETH_SPEED_NUM_10G },
		{ "20 Gbps",  RTE_ETH_SPEED_NUM_20G },
		{ "25 Gbps",  RTE_ETH_SPEED_NUM_25G },
		{ "40 Gbps",  RTE_ETH_SPEED_NUM_40G },
		{ "50 Gbps",  RTE_ETH_SPEED_NUM_50G },
		{ "56 Gbps",  RTE_ETH_SPEED_NUM_56G },
		{ "100 Gbps", RTE_ETH_SPEED_NUM_100G },
		{ "200 Gbps", RTE_ETH_SPEED_NUM_200G },
		{ "Unknown",  RTE_ETH_SPEED_NUM_UNKNOWN },
		{ "Invalid",   50505 }
	};

	for (i = 0; i < sizeof(speed_str_map) / sizeof(struct link_speed_t);
			i++) {
		speed = rte_eth_link_speed_to_str(speed_str_map[i].link_speed);
		TEST_ASSERT_BUFFERS_ARE_EQUAL(speed_str_map[i].value,
			speed, strlen(speed_str_map[i].value),
			"Invalid link speed string");
	}
	return TEST_SUCCESS;
}

static struct unit_test_suite link_status_testsuite = {
	.suite_name = "link status formatting",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
		TEST_CASE(test_link_status_up_default),
		TEST_CASE(test_link_status_down_default),
		TEST_CASE(test_link_speed_all_values),
		TEST_CASE(test_link_status_invalid),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_link_status(void)
{
	rte_log_set_global_level(RTE_LOG_DEBUG);
	rte_log_set_level(RTE_LOGTYPE_EAL, RTE_LOG_DEBUG);

	return unit_test_suite_runner(&link_status_testsuite);
}

REGISTER_TEST_COMMAND(ethdev_link_status, test_link_status);
