/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <rte_cycles.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <rte_log.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_string_fns.h>
#include <rte_eth_bond.h>

#include "virtual_pmd.h"
#include "packet_burst_generator.h"

#include "test.h"

#define TEST_MAX_NUMBER_OF_PORTS (6)

#define RX_RING_SIZE 1024
#define RX_FREE_THRESH 32
#define RX_PTHRESH 8
#define RX_HTHRESH 8
#define RX_WTHRESH 0

#define TX_RING_SIZE 1024
#define TX_FREE_THRESH 32
#define TX_PTHRESH 32
#define TX_HTHRESH 0
#define TX_WTHRESH 0
#define TX_RSBIT_THRESH 32

#define MBUF_CACHE_SIZE (250)
#define BURST_SIZE (32)

#define RX_DESC_MAX	(2048)
#define TX_DESC_MAX	(2048)
#define MAX_PKT_BURST			(512)
#define DEF_PKT_BURST			(16)

#define BONDING_DEV_NAME			("net_bonding_ut")

#define INVALID_SOCKET_ID		(-1)
#define INVALID_PORT_ID			(-1)
#define INVALID_BONDING_MODE	(-1)


uint8_t member_mac[] = {0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00 };
uint8_t bonding_mac[] = {0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF };

struct link_bonding_unittest_params {
	int16_t bonding_port_id;
	int16_t member_port_ids[TEST_MAX_NUMBER_OF_PORTS];
	uint16_t bonding_member_count;
	uint8_t bonding_mode;

	uint16_t nb_rx_q;
	uint16_t nb_tx_q;

	struct rte_mempool *mbuf_pool;

	struct rte_ether_addr *default_member_mac;
	struct rte_ether_addr *default_bonding_mac;

	/* Packet Headers */
	struct rte_ether_hdr *pkt_eth_hdr;
	struct rte_ipv4_hdr *pkt_ipv4_hdr;
	struct rte_ipv6_hdr *pkt_ipv6_hdr;
	struct rte_udp_hdr *pkt_udp_hdr;

};

static struct rte_ipv4_hdr pkt_ipv4_hdr;
static struct rte_ipv6_hdr pkt_ipv6_hdr;
static struct rte_udp_hdr pkt_udp_hdr;

static struct link_bonding_unittest_params default_params  = {
	.bonding_port_id = -1,
	.member_port_ids = { -1 },
	.bonding_member_count = 0,
	.bonding_mode = BONDING_MODE_ROUND_ROBIN,

	.nb_rx_q = 1,
	.nb_tx_q = 1,

	.mbuf_pool = NULL,

	.default_member_mac = (struct rte_ether_addr *)member_mac,
	.default_bonding_mac = (struct rte_ether_addr *)bonding_mac,

	.pkt_eth_hdr = NULL,
	.pkt_ipv4_hdr = &pkt_ipv4_hdr,
	.pkt_ipv6_hdr = &pkt_ipv6_hdr,
	.pkt_udp_hdr = &pkt_udp_hdr

};

static struct link_bonding_unittest_params *test_params = &default_params;

static uint8_t src_mac[] = { 0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA };
static uint8_t dst_mac_0[] = { 0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA };
static uint8_t dst_mac_1[] = { 0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAB };

static uint32_t src_addr = IPV4_ADDR(192, 168, 1, 98);
static uint32_t dst_addr_0 = IPV4_ADDR(192, 168, 1, 98);
static uint32_t dst_addr_1 = IPV4_ADDR(193, 166, 10, 97);

static uint8_t src_ipv6_addr[] = { 0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF,
		0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA , 0xFF, 0xAA  };
static uint8_t dst_ipv6_addr_0[] = { 0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF,
		0xAA, 0xFF, 0xAA,  0xFF, 0xAA , 0xFF, 0xAA, 0xFF, 0xAA  };
static uint8_t dst_ipv6_addr_1[] = { 0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF,
		0xAA, 0xFF, 0xAA, 0xFF, 0xAA , 0xFF, 0xAA , 0xFF, 0xAB  };

static uint16_t src_port = 1024;
static uint16_t dst_port_0 = 1024;
static uint16_t dst_port_1 = 2024;

static uint16_t vlan_id = 0x100;

static struct rte_eth_conf default_pmd_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_NONE,
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
	.lpbk_mode = 0,
};

static const struct rte_eth_rxconf rx_conf_default = {
	.rx_thresh = {
		.pthresh = RX_PTHRESH,
		.hthresh = RX_HTHRESH,
		.wthresh = RX_WTHRESH,
	},
	.rx_free_thresh = RX_FREE_THRESH,
	.rx_drop_en = 0,
};

static struct rte_eth_txconf tx_conf_default = {
	.tx_thresh = {
		.pthresh = TX_PTHRESH,
		.hthresh = TX_HTHRESH,
		.wthresh = TX_WTHRESH,
	},
	.tx_free_thresh = TX_FREE_THRESH,
	.tx_rs_thresh = TX_RSBIT_THRESH,
};

static void free_virtualpmd_tx_queue(void);



static int
configure_ethdev(uint16_t port_id, uint8_t start, uint8_t en_isr)
{
	int q_id;

	if (en_isr)
		default_pmd_conf.intr_conf.lsc = 1;
	else
		default_pmd_conf.intr_conf.lsc = 0;

	TEST_ASSERT_SUCCESS(rte_eth_dev_configure(port_id, test_params->nb_rx_q,
			test_params->nb_tx_q, &default_pmd_conf),
			"rte_eth_dev_configure for port %d failed", port_id);

	int ret = rte_eth_dev_set_mtu(port_id, 1550);
	RTE_TEST_ASSERT(ret == 0 || ret == -ENOTSUP,
			"rte_eth_dev_set_mtu for port %d failed", port_id);

	for (q_id = 0; q_id < test_params->nb_rx_q; q_id++)
		TEST_ASSERT_SUCCESS(rte_eth_rx_queue_setup(port_id, q_id, RX_RING_SIZE,
				rte_eth_dev_socket_id(port_id), &rx_conf_default,
				test_params->mbuf_pool) ,
				"rte_eth_rx_queue_setup for port %d failed", port_id);

	for (q_id = 0; q_id < test_params->nb_tx_q; q_id++)
		TEST_ASSERT_SUCCESS(rte_eth_tx_queue_setup(port_id, q_id, TX_RING_SIZE,
				rte_eth_dev_socket_id(port_id), &tx_conf_default),
				"rte_eth_tx_queue_setup for port %d failed", port_id);

	if (start)
		TEST_ASSERT_SUCCESS(rte_eth_dev_start(port_id),
				"rte_eth_dev_start for port %d failed", port_id);

	return 0;
}

static int members_initialized;
static int mac_members_initialized;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cvar = PTHREAD_COND_INITIALIZER;


static int
test_setup(void)
{
	int i, nb_mbuf_per_pool;
	struct rte_ether_addr *mac_addr = (struct rte_ether_addr *)member_mac;

	/* Allocate ethernet packet header with space for VLAN header */
	if (test_params->pkt_eth_hdr == NULL) {
		test_params->pkt_eth_hdr = malloc(sizeof(struct rte_ether_hdr) +
				sizeof(struct rte_vlan_hdr));

		TEST_ASSERT_NOT_NULL(test_params->pkt_eth_hdr,
				"Ethernet header struct allocation failed!");
	}

	nb_mbuf_per_pool = RX_DESC_MAX + DEF_PKT_BURST +
			TX_DESC_MAX + MAX_PKT_BURST;
	if (test_params->mbuf_pool == NULL) {
		test_params->mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
			nb_mbuf_per_pool, MBUF_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
		TEST_ASSERT_NOT_NULL(test_params->mbuf_pool,
				"rte_mempool_create failed");
	}

	/* Create / Initialize virtual eth devs */
	if (!members_initialized) {
		for (i = 0; i < TEST_MAX_NUMBER_OF_PORTS; i++) {
			char pmd_name[RTE_ETH_NAME_MAX_LEN];

			mac_addr->addr_bytes[RTE_ETHER_ADDR_LEN-1] = i;

			snprintf(pmd_name, RTE_ETH_NAME_MAX_LEN, "eth_virt_%d", i);

			test_params->member_port_ids[i] = virtual_ethdev_create(pmd_name,
					mac_addr, rte_socket_id(), 1);
			TEST_ASSERT(test_params->member_port_ids[i] >= 0,
					"Failed to create virtual virtual ethdev %s", pmd_name);

			TEST_ASSERT_SUCCESS(configure_ethdev(
					test_params->member_port_ids[i], 1, 0),
					"Failed to configure virtual ethdev %s", pmd_name);
		}
		members_initialized = 1;
	}

	return 0;
}

static int
test_create_bonding_device(void)
{
	int current_member_count;

	uint16_t members[RTE_MAX_ETHPORTS];

	/* Don't try to recreate bonding device if re-running test suite*/
	if (test_params->bonding_port_id == -1) {
		test_params->bonding_port_id = rte_eth_bond_create(BONDING_DEV_NAME,
				test_params->bonding_mode, rte_socket_id());

		TEST_ASSERT(test_params->bonding_port_id >= 0,
				"Failed to create bonding ethdev %s", BONDING_DEV_NAME);

		TEST_ASSERT_SUCCESS(configure_ethdev(test_params->bonding_port_id, 0, 0),
				"Failed to configure bonding ethdev %s", BONDING_DEV_NAME);
	}

	TEST_ASSERT_SUCCESS(rte_eth_bond_mode_set(test_params->bonding_port_id,
			test_params->bonding_mode), "Failed to set ethdev %d to mode %d",
			test_params->bonding_port_id, test_params->bonding_mode);

	current_member_count = rte_eth_bond_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(current_member_count, 0,
			"Number of members %d is great than expected %d.",
			current_member_count, 0);

	current_member_count = rte_eth_bond_active_members_get(
			test_params->bonding_port_id, members, RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(current_member_count, 0,
			"Number of active members %d is great than expected %d.",
			current_member_count, 0);

	return 0;
}


static int
test_create_bonding_device_with_invalid_params(void)
{
	int port_id;

	test_params->bonding_mode = BONDING_MODE_ROUND_ROBIN;

	/* Invalid name */
	port_id = rte_eth_bond_create(NULL, test_params->bonding_mode,
			rte_socket_id());
	TEST_ASSERT(port_id < 0, "Created bonding device unexpectedly");

	test_params->bonding_mode = INVALID_BONDING_MODE;

	/* Invalid bonding mode */
	port_id = rte_eth_bond_create(BONDING_DEV_NAME, test_params->bonding_mode,
			rte_socket_id());
	TEST_ASSERT(port_id < 0, "Created bonding device unexpectedly.");

	test_params->bonding_mode = BONDING_MODE_ROUND_ROBIN;

	/* Invalid socket id */
	port_id = rte_eth_bond_create(BONDING_DEV_NAME, test_params->bonding_mode,
			INVALID_SOCKET_ID);
	TEST_ASSERT(port_id < 0, "Created bonding device unexpectedly.");

	return 0;
}

static int
test_add_member_to_bonding_device(void)
{
	int current_member_count;

	uint16_t members[RTE_MAX_ETHPORTS];

	TEST_ASSERT_SUCCESS(rte_eth_bond_member_add(test_params->bonding_port_id,
			test_params->member_port_ids[test_params->bonding_member_count]),
			"Failed to add member (%d) to bonding port (%d).",
			test_params->member_port_ids[test_params->bonding_member_count],
			test_params->bonding_port_id);

	current_member_count = rte_eth_bond_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_member_count, test_params->bonding_member_count + 1,
			"Number of members (%d) is greater than expected (%d).",
			current_member_count, test_params->bonding_member_count + 1);

	current_member_count = rte_eth_bond_active_members_get(
			test_params->bonding_port_id, members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_member_count, 0,
					"Number of active members (%d) is not as expected (%d).\n",
					current_member_count, 0);

	test_params->bonding_member_count++;

	return 0;
}

static int
test_add_member_to_invalid_bonding_device(void)
{
	/* Invalid port ID */
	TEST_ASSERT_FAIL(rte_eth_bond_member_add(test_params->bonding_port_id + 5,
			test_params->member_port_ids[test_params->bonding_member_count]),
			"Expected call to failed as invalid port specified.");

	/* Non bonding device */
	TEST_ASSERT_FAIL(rte_eth_bond_member_add(test_params->member_port_ids[0],
			test_params->member_port_ids[test_params->bonding_member_count]),
			"Expected call to failed as invalid port specified.");

	return 0;
}


static int
test_remove_member_from_bonding_device(void)
{
	int current_member_count;
	struct rte_ether_addr read_mac_addr, *mac_addr;
	uint16_t members[RTE_MAX_ETHPORTS];

	TEST_ASSERT_SUCCESS(rte_eth_bond_member_remove(test_params->bonding_port_id,
			test_params->member_port_ids[test_params->bonding_member_count-1]),
			"Failed to remove member %d from bonding port (%d).",
			test_params->member_port_ids[test_params->bonding_member_count-1],
			test_params->bonding_port_id);


	current_member_count = rte_eth_bond_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(current_member_count, test_params->bonding_member_count - 1,
			"Number of members (%d) is great than expected (%d).\n",
			current_member_count, test_params->bonding_member_count - 1);


	mac_addr = (struct rte_ether_addr *)member_mac;
	mac_addr->addr_bytes[RTE_ETHER_ADDR_LEN-1] =
			test_params->bonding_member_count-1;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(
			test_params->member_port_ids[test_params->bonding_member_count-1],
			&read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[test_params->bonding_member_count-1]);
	TEST_ASSERT_SUCCESS(memcmp(mac_addr, &read_mac_addr, sizeof(read_mac_addr)),
			"bonding port mac address not set to that of primary port\n");

	rte_eth_stats_reset(
			test_params->member_port_ids[test_params->bonding_member_count-1]);

	virtual_ethdev_simulate_link_status_interrupt(test_params->bonding_port_id,
			0);

	test_params->bonding_member_count--;

	return 0;
}

static int
test_remove_member_from_invalid_bonding_device(void)
{
	/* Invalid port ID */
	TEST_ASSERT_FAIL(rte_eth_bond_member_remove(
			test_params->bonding_port_id + 5,
			test_params->member_port_ids[test_params->bonding_member_count - 1]),
			"Expected call to failed as invalid port specified.");

	/* Non bonding device */
	TEST_ASSERT_FAIL(rte_eth_bond_member_remove(
			test_params->member_port_ids[0],
			test_params->member_port_ids[test_params->bonding_member_count - 1]),
			"Expected call to failed as invalid port specified.");

	return 0;
}

static int bonding_id = 2;

static int
test_add_already_bonding_member_to_bonding_device(void)
{
	int port_id, current_member_count;
	uint16_t members[RTE_MAX_ETHPORTS];
	char pmd_name[RTE_ETH_NAME_MAX_LEN];

	TEST_ASSERT_SUCCESS(test_add_member_to_bonding_device(),
			"Failed to add member to bonding device");

	current_member_count = rte_eth_bond_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_member_count, 1,
			"Number of members (%d) is not that expected (%d).",
			current_member_count, 1);

	snprintf(pmd_name, RTE_ETH_NAME_MAX_LEN, "%s_%d", BONDING_DEV_NAME, ++bonding_id);

	port_id = rte_eth_bond_create(pmd_name, test_params->bonding_mode,
			rte_socket_id());
	TEST_ASSERT(port_id >= 0, "Failed to create bonding device.");

	TEST_ASSERT(rte_eth_bond_member_add(port_id,
			test_params->member_port_ids[test_params->bonding_member_count - 1])
			< 0,
			"Added member (%d) to bonding port (%d) unexpectedly.",
			test_params->member_port_ids[test_params->bonding_member_count-1],
			port_id);

	return test_remove_member_from_bonding_device();
}


static int
test_get_members_from_bonding_device(void)
{
	int current_member_count;
	uint16_t members[RTE_MAX_ETHPORTS];

	TEST_ASSERT_SUCCESS(test_add_member_to_bonding_device(),
			"Failed to add member to bonding device");

	/* Invalid port id */
	current_member_count = rte_eth_bond_members_get(INVALID_PORT_ID, members,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_member_count < 0,
			"Invalid port id unexpectedly succeeded");

	current_member_count = rte_eth_bond_active_members_get(INVALID_PORT_ID,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_member_count < 0,
			"Invalid port id unexpectedly succeeded");

	/* Invalid members pointer */
	current_member_count = rte_eth_bond_members_get(test_params->bonding_port_id,
			NULL, RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_member_count < 0,
			"Invalid member array unexpectedly succeeded");

	current_member_count = rte_eth_bond_active_members_get(
			test_params->bonding_port_id, NULL, RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_member_count < 0,
			"Invalid member array unexpectedly succeeded");

	/* non bonding device*/
	current_member_count = rte_eth_bond_members_get(
			test_params->member_port_ids[0], NULL, RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_member_count < 0,
			"Invalid port id unexpectedly succeeded");

	current_member_count = rte_eth_bond_active_members_get(
			test_params->member_port_ids[0],	NULL, RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_member_count < 0,
			"Invalid port id unexpectedly succeeded");

	TEST_ASSERT_SUCCESS(test_remove_member_from_bonding_device(),
			"Failed to remove members from bonding device");

	return 0;
}


static int
test_add_remove_multiple_members_to_from_bonding_device(void)
{
	int i;

	for (i = 0; i < TEST_MAX_NUMBER_OF_PORTS; i++)
		TEST_ASSERT_SUCCESS(test_add_member_to_bonding_device(),
				"Failed to add member to bonding device");

	for (i = 0; i < TEST_MAX_NUMBER_OF_PORTS; i++)
		TEST_ASSERT_SUCCESS(test_remove_member_from_bonding_device(),
				"Failed to remove members from bonding device");

	return 0;
}

static void
enable_bonding_members(void)
{
	int i;

	for (i = 0; i < test_params->bonding_member_count; i++) {
		virtual_ethdev_tx_burst_fn_set_success(test_params->member_port_ids[i],
				1);

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->member_port_ids[i], 1);
	}
}

static int
test_start_bonding_device(void)
{
	struct rte_eth_link link_status;

	int current_member_count, current_bonding_mode, primary_port;
	uint16_t members[RTE_MAX_ETHPORTS];
	int retval;

	/* Add member to bonding device*/
	TEST_ASSERT_SUCCESS(test_add_member_to_bonding_device(),
			"Failed to add member to bonding device");

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonding_port_id),
		"Failed to start bonding pmd eth device %d.",
		test_params->bonding_port_id);

	/*
	 * Change link status of virtual pmd so it will be added to the active
	 * member list of the bonding device.
	 */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[test_params->bonding_member_count-1], 1);

	current_member_count = rte_eth_bond_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_member_count, test_params->bonding_member_count,
			"Number of members (%d) is not expected value (%d).",
			current_member_count, test_params->bonding_member_count);

	current_member_count = rte_eth_bond_active_members_get(
			test_params->bonding_port_id, members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_member_count, test_params->bonding_member_count,
			"Number of active members (%d) is not expected value (%d).",
			current_member_count, test_params->bonding_member_count);

	current_bonding_mode = rte_eth_bond_mode_get(test_params->bonding_port_id);
	TEST_ASSERT_EQUAL(current_bonding_mode, test_params->bonding_mode,
			"Bonding device mode (%d) is not expected value (%d).\n",
			current_bonding_mode, test_params->bonding_mode);

	primary_port = rte_eth_bond_primary_get(test_params->bonding_port_id);
	TEST_ASSERT_EQUAL(primary_port, test_params->member_port_ids[0],
			"Primary port (%d) is not expected value (%d).",
			primary_port, test_params->member_port_ids[0]);

	retval = rte_eth_link_get(test_params->bonding_port_id, &link_status);
	TEST_ASSERT(retval >= 0,
			"Bonding port (%d) link get failed: %s\n",
			test_params->bonding_port_id, rte_strerror(-retval));
	TEST_ASSERT_EQUAL(link_status.link_status, 1,
			"Bonding port (%d) status (%d) is not expected value (%d).\n",
			test_params->bonding_port_id, link_status.link_status, 1);

	return 0;
}

static int
test_stop_bonding_device(void)
{
	int current_member_count;
	uint16_t members[RTE_MAX_ETHPORTS];

	struct rte_eth_link link_status;
	int retval;

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonding_port_id),
			"Failed to stop bonding port %u",
			test_params->bonding_port_id);

	retval = rte_eth_link_get(test_params->bonding_port_id, &link_status);
	TEST_ASSERT(retval >= 0,
			"Bonding port (%d) link get failed: %s\n",
			test_params->bonding_port_id, rte_strerror(-retval));
	TEST_ASSERT_EQUAL(link_status.link_status, 0,
			"Bonding port (%d) status (%d) is not expected value (%d).",
			test_params->bonding_port_id, link_status.link_status, 0);

	current_member_count = rte_eth_bond_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_member_count, test_params->bonding_member_count,
			"Number of members (%d) is not expected value (%d).",
			current_member_count, test_params->bonding_member_count);

	current_member_count = rte_eth_bond_active_members_get(
			test_params->bonding_port_id, members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_member_count, 0,
			"Number of active members (%d) is not expected value (%d).",
			current_member_count, 0);

	return 0;
}

static int
remove_members_and_stop_bonding_device(void)
{
	/* Clean up and remove members from bonding device */
	free_virtualpmd_tx_queue();
	while (test_params->bonding_member_count > 0)
		TEST_ASSERT_SUCCESS(test_remove_member_from_bonding_device(),
				"test_remove_member_from_bonding_device failed");

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonding_port_id),
			"Failed to stop bonding port %u",
			test_params->bonding_port_id);

	rte_eth_stats_reset(test_params->bonding_port_id);
	rte_eth_bond_mac_address_reset(test_params->bonding_port_id);

	return 0;
}

static int
test_set_bonding_mode(void)
{
	int i, bonding_mode;

	int bonding_modes[] = { BONDING_MODE_ROUND_ROBIN,
							BONDING_MODE_ACTIVE_BACKUP,
							BONDING_MODE_BALANCE,
							BONDING_MODE_BROADCAST
							};

	/* Test supported link bonding modes */
	for (i = 0; i < (int)RTE_DIM(bonding_modes);	i++) {
		/* Invalid port ID */
		TEST_ASSERT_FAIL(rte_eth_bond_mode_set(INVALID_PORT_ID,
				bonding_modes[i]),
				"Expected call to failed as invalid port (%d) specified.",
				INVALID_PORT_ID);

		/* Non bonding device */
		TEST_ASSERT_FAIL(rte_eth_bond_mode_set(test_params->member_port_ids[0],
				bonding_modes[i]),
				"Expected call to failed as invalid port (%d) specified.",
				test_params->member_port_ids[0]);

		TEST_ASSERT_SUCCESS(rte_eth_bond_mode_set(test_params->bonding_port_id,
				bonding_modes[i]),
				"Failed to set link bonding mode on port (%d) to (%d).",
				test_params->bonding_port_id, bonding_modes[i]);

		bonding_mode = rte_eth_bond_mode_get(test_params->bonding_port_id);
		TEST_ASSERT_EQUAL(bonding_mode, bonding_modes[i],
				"Link bonding mode (%d) of port (%d) is not expected value (%d).",
				bonding_mode, test_params->bonding_port_id,
				bonding_modes[i]);

		/* Invalid port ID */
		bonding_mode = rte_eth_bond_mode_get(INVALID_PORT_ID);
		TEST_ASSERT(bonding_mode < 0,
				"Expected call to failed as invalid port (%d) specified.",
				INVALID_PORT_ID);

		/* Non bonding device */
		bonding_mode = rte_eth_bond_mode_get(test_params->member_port_ids[0]);
		TEST_ASSERT(bonding_mode < 0,
				"Expected call to failed as invalid port (%d) specified.",
				test_params->member_port_ids[0]);
	}

	return remove_members_and_stop_bonding_device();
}

static int
test_set_primary_member(void)
{
	int i, j, retval;
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr *expected_mac_addr;

	/* Add 4 members to bonding device */
	for (i = test_params->bonding_member_count; i < 4; i++)
		TEST_ASSERT_SUCCESS(test_add_member_to_bonding_device(),
				"Failed to add member to bonding device.");

	TEST_ASSERT_SUCCESS(rte_eth_bond_mode_set(test_params->bonding_port_id,
			BONDING_MODE_ROUND_ROBIN),
			"Failed to set link bonding mode on port (%d) to (%d).",
			test_params->bonding_port_id, BONDING_MODE_ROUND_ROBIN);

	/* Invalid port ID */
	TEST_ASSERT_FAIL(rte_eth_bond_primary_set(INVALID_PORT_ID,
			test_params->member_port_ids[i]),
			"Expected call to failed as invalid port specified.");

	/* Non bonding device */
	TEST_ASSERT_FAIL(rte_eth_bond_primary_set(test_params->member_port_ids[i],
			test_params->member_port_ids[i]),
			"Expected call to failed as invalid port specified.");

	/* Set member as primary
	 * Verify member it is now primary member
	 * Verify that MAC address of bonding device is that of primary member
	 * Verify that MAC address of all bonding members are that of primary member
	 */
	for (i = 0; i < 4; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_bond_primary_set(test_params->bonding_port_id,
				test_params->member_port_ids[i]),
				"Failed to set bonding port (%d) primary port to (%d)",
				test_params->bonding_port_id, test_params->member_port_ids[i]);

		retval = rte_eth_bond_primary_get(test_params->bonding_port_id);
		TEST_ASSERT(retval >= 0,
				"Failed to read primary port from bonding port (%d)\n",
					test_params->bonding_port_id);

		TEST_ASSERT_EQUAL(retval, test_params->member_port_ids[i],
				"Bonding port (%d) primary port (%d) not expected value (%d)\n",
				test_params->bonding_port_id, retval,
				test_params->member_port_ids[i]);

		/* stop/start bonding eth dev to apply new MAC */
		TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonding_port_id),
				"Failed to stop bonding port %u",
				test_params->bonding_port_id);

		TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonding_port_id),
				"Failed to start bonding port %d",
				test_params->bonding_port_id);

		expected_mac_addr = (struct rte_ether_addr *)&member_mac;
		expected_mac_addr->addr_bytes[RTE_ETHER_ADDR_LEN-1] = i;

		/* Check primary member MAC */
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[i],
				&read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->member_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(expected_mac_addr, &read_mac_addr,
				sizeof(read_mac_addr)),
				"bonding port mac address not set to that of primary port\n");

		/* Check bonding MAC */
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id,
				&read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->bonding_port_id);
		TEST_ASSERT_SUCCESS(memcmp(&read_mac_addr, &read_mac_addr,
				sizeof(read_mac_addr)),
				"bonding port mac address not set to that of primary port\n");

		/* Check other members MACs */
		for (j = 0; j < 4; j++) {
			if (j != i) {
				TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(
						test_params->member_port_ids[j],
						&read_mac_addr),
						"Failed to get mac address (port %d)",
						test_params->member_port_ids[j]);
				TEST_ASSERT_SUCCESS(memcmp(expected_mac_addr, &read_mac_addr,
						sizeof(read_mac_addr)),
						"member port mac address not set to that of primary "
						"port");
			}
		}
	}


	/* Test with none existent port */
	TEST_ASSERT_FAIL(rte_eth_bond_primary_get(test_params->bonding_port_id + 10),
			"read primary port from expectedly");

	/* Test with member port */
	TEST_ASSERT_FAIL(rte_eth_bond_primary_get(test_params->member_port_ids[0]),
			"read primary port from expectedly\n");

	TEST_ASSERT_SUCCESS(remove_members_and_stop_bonding_device(),
			"Failed to stop and remove members from bonding device");

	/* No members  */
	TEST_ASSERT(rte_eth_bond_primary_get(test_params->bonding_port_id)  < 0,
			"read primary port from expectedly\n");

	return 0;
}

static int
test_set_explicit_bonding_mac(void)
{
	int i;
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr *mac_addr;

	uint8_t explicit_bonding_mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };

	mac_addr = (struct rte_ether_addr *)explicit_bonding_mac;

	/* Invalid port ID */
	TEST_ASSERT_FAIL(rte_eth_bond_mac_address_set(INVALID_PORT_ID, mac_addr),
			"Expected call to failed as invalid port specified.");

	/* Non bonding device */
	TEST_ASSERT_FAIL(rte_eth_bond_mac_address_set(
			test_params->member_port_ids[0],	mac_addr),
			"Expected call to failed as invalid port specified.");

	/* NULL MAC address */
	TEST_ASSERT_FAIL(rte_eth_bond_mac_address_set(
			test_params->bonding_port_id, NULL),
			"Expected call to failed as NULL MAC specified");

	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonding_port_id, mac_addr),
			"Failed to set MAC address on bonding port (%d)",
			test_params->bonding_port_id);

	/* Add 4 members to bonding device */
	for (i = test_params->bonding_member_count; i < 4; i++) {
		TEST_ASSERT_SUCCESS(test_add_member_to_bonding_device(),
				"Failed to add member to bonding device.\n");
	}

	/* Check bonding MAC */
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(mac_addr, &read_mac_addr, sizeof(read_mac_addr)),
			"bonding port mac address not set to that of primary port");

	/* Check other members MACs */
	for (i = 0; i < 4; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[i],
				&read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->member_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(mac_addr, &read_mac_addr,
				sizeof(read_mac_addr)),
				"member port mac address not set to that of primary port");
	}

	/* test resetting mac address on bonding device */
	TEST_ASSERT_SUCCESS(
			rte_eth_bond_mac_address_reset(test_params->bonding_port_id),
			"Failed to reset MAC address on bonding port (%d)",
			test_params->bonding_port_id);

	TEST_ASSERT_FAIL(
			rte_eth_bond_mac_address_reset(test_params->member_port_ids[0]),
			"Reset MAC address on bonding port (%d) unexpectedly",
			test_params->member_port_ids[1]);

	/* test resetting mac address on bonding device with no members */
	TEST_ASSERT_SUCCESS(remove_members_and_stop_bonding_device(),
			"Failed to remove members and stop bonding device");

	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_reset(test_params->bonding_port_id),
			"Failed to reset MAC address on bonding port (%d)",
				test_params->bonding_port_id);

	return 0;
}

#define BONDING_INIT_MAC_ASSIGNMENT_MEMBER_COUNT (3)

static int
test_set_bonding_port_initialization_mac_assignment(void)
{
	int i, member_count;

	uint16_t members[RTE_MAX_ETHPORTS];
	static int bonding_port_id = -1;
	static int member_port_ids[BONDING_INIT_MAC_ASSIGNMENT_MEMBER_COUNT];

	struct rte_ether_addr member_mac_addr, bonding_mac_addr, read_mac_addr;

	/* Initialize default values for MAC addresses */
	memcpy(&member_mac_addr, member_mac, sizeof(struct rte_ether_addr));
	memcpy(&bonding_mac_addr, member_mac, sizeof(struct rte_ether_addr));

	/*
	 * 1. a - Create / configure  bonding / member ethdevs
	 */
	if (bonding_port_id == -1) {
		bonding_port_id = rte_eth_bond_create("net_bonding_mac_ass_test",
				BONDING_MODE_ACTIVE_BACKUP, rte_socket_id());
		TEST_ASSERT(bonding_port_id > 0, "failed to create bonding device");

		TEST_ASSERT_SUCCESS(configure_ethdev(bonding_port_id, 0, 0),
					"Failed to configure bonding ethdev");
	}

	if (!mac_members_initialized) {
		for (i = 0; i < BONDING_INIT_MAC_ASSIGNMENT_MEMBER_COUNT; i++) {
			char pmd_name[RTE_ETH_NAME_MAX_LEN];

			member_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN - 1] =
				i + 100;

			snprintf(pmd_name, RTE_ETH_NAME_MAX_LEN,
				"eth_member_%d", i);

			member_port_ids[i] = virtual_ethdev_create(pmd_name,
					&member_mac_addr, rte_socket_id(), 1);

			TEST_ASSERT(member_port_ids[i] >= 0,
					"Failed to create member ethdev %s",
					pmd_name);

			TEST_ASSERT_SUCCESS(configure_ethdev(member_port_ids[i], 1, 0),
					"Failed to configure virtual ethdev %s",
					pmd_name);
		}
		mac_members_initialized = 1;
	}


	/*
	 * 2. Add member ethdevs to bonding device
	 */
	for (i = 0; i < BONDING_INIT_MAC_ASSIGNMENT_MEMBER_COUNT; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_bond_member_add(bonding_port_id,
				member_port_ids[i]),
				"Failed to add member (%d) to bonding port (%d).",
				member_port_ids[i], bonding_port_id);
	}

	member_count = rte_eth_bond_members_get(bonding_port_id, members,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(BONDING_INIT_MAC_ASSIGNMENT_MEMBER_COUNT, member_count,
			"Number of members (%d) is not as expected (%d)",
			member_count, BONDING_INIT_MAC_ASSIGNMENT_MEMBER_COUNT);


	/*
	 * 3. Set explicit MAC address on bonding ethdev
	 */
	bonding_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-2] = 0xFF;
	bonding_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 0xAA;

	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			bonding_port_id, &bonding_mac_addr),
			"Failed to set MAC address on bonding port (%d)",
			bonding_port_id);


	/* 4. a - Start bonding ethdev
	 *    b - Enable member devices
	 *    c - Verify bonding/members ethdev MAC addresses
	 */
	TEST_ASSERT_SUCCESS(rte_eth_dev_start(bonding_port_id),
			"Failed to start bonding pmd eth device %d.",
			bonding_port_id);

	for (i = 0; i < BONDING_INIT_MAC_ASSIGNMENT_MEMBER_COUNT; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				member_port_ids[i], 1);
	}

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&bonding_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port mac address not as expected");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&bonding_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port 0 mac address not as expected");

	member_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 1 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&member_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port 1 mac address not as expected");

	member_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 2 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(member_port_ids[2], &read_mac_addr),
			"Failed to get mac address (port %d)",
			member_port_ids[2]);
	TEST_ASSERT_SUCCESS(memcmp(&member_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port 2 mac address not as expected");


	/* 7. a - Change primary port
	 *    b - Stop / Start bonding port
	 *    d - Verify member ethdev MAC addresses
	 */
	TEST_ASSERT_SUCCESS(rte_eth_bond_primary_set(bonding_port_id,
			member_port_ids[2]),
			"failed to set primary port on bonding device.");

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(bonding_port_id),
			"Failed to stop bonding port %u",
			bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(bonding_port_id),
				"Failed to start bonding pmd eth device %d.",
				bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&bonding_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port mac address not as expected");

	member_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 0 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&member_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port 0 mac address not as expected");

	member_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 1 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&member_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port 1 mac address not as expected");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(member_port_ids[2], &read_mac_addr),
			"Failed to get mac address (port %d)",
			member_port_ids[2]);
	TEST_ASSERT_SUCCESS(memcmp(&bonding_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port 2 mac address not as expected");

	/* 6. a - Stop bonding ethdev
	 *    b - remove member ethdevs
	 *    c - Verify member ethdevs MACs are restored
	 */
	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(bonding_port_id),
			"Failed to stop bonding port %u",
			bonding_port_id);

	for (i = 0; i < BONDING_INIT_MAC_ASSIGNMENT_MEMBER_COUNT; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_bond_member_remove(bonding_port_id,
				member_port_ids[i]),
				"Failed to remove member %d from bonding port (%d).",
				member_port_ids[i], bonding_port_id);
	}

	member_count = rte_eth_bond_members_get(bonding_port_id, members,
			RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(member_count, 0,
			"Number of members (%d) is great than expected (%d).",
			member_count, 0);

	member_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 0 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&member_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port 0 mac address not as expected");

	member_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 1 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&member_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port 1 mac address not as expected");

	member_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 2 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(member_port_ids[2], &read_mac_addr),
			"Failed to get mac address (port %d)",
			member_port_ids[2]);
	TEST_ASSERT_SUCCESS(memcmp(&member_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port 2 mac address not as expected");

	return 0;
}


static int
initialize_bonding_device_with_members(uint8_t bonding_mode, uint8_t bond_en_isr,
		uint16_t number_of_members, uint8_t enable_member)
{
	/* Configure bonding device */
	TEST_ASSERT_SUCCESS(configure_ethdev(test_params->bonding_port_id, 0,
			bond_en_isr), "Failed to configure bonding port (%d) in mode %d "
			"with (%d) members.", test_params->bonding_port_id, bonding_mode,
			number_of_members);

	/* Add members to bonding device */
	while (number_of_members > test_params->bonding_member_count)
		TEST_ASSERT_SUCCESS(test_add_member_to_bonding_device(),
				"Failed to add member (%d to  bonding port (%d).",
				test_params->bonding_member_count - 1,
				test_params->bonding_port_id);

	/* Set link bonding mode  */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mode_set(test_params->bonding_port_id,
			bonding_mode),
			"Failed to set link bonding mode on port (%d) to (%d).",
			test_params->bonding_port_id, bonding_mode);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonding_port_id),
		"Failed to start bonding pmd eth device %d.",
		test_params->bonding_port_id);

	if (enable_member)
		enable_bonding_members();

	return 0;
}

static int
test_adding_member_after_bonding_device_started(void)
{
	int i;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ROUND_ROBIN, 0, 4, 0),
			"Failed to add members to bonding device");

	/* Enabled member devices */
	for (i = 0; i < test_params->bonding_member_count + 1; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				test_params->member_port_ids[i], 1);
	}

	TEST_ASSERT_SUCCESS(rte_eth_bond_member_add(test_params->bonding_port_id,
			test_params->member_port_ids[test_params->bonding_member_count]),
			"Failed to add member to bonding port.\n");

	rte_eth_stats_reset(
			test_params->member_port_ids[test_params->bonding_member_count]);

	test_params->bonding_member_count++;

	return remove_members_and_stop_bonding_device();
}

#define TEST_STATUS_INTERRUPT_MEMBER_COUNT	4
#define TEST_LSC_WAIT_TIMEOUT_US	500000

int test_lsc_interrupt_count;


static int
test_bonding_lsc_event_callback(uint16_t port_id __rte_unused,
		enum rte_eth_event_type type  __rte_unused,
		void *param __rte_unused,
		void *ret_param __rte_unused)
{
	pthread_mutex_lock(&mutex);
	test_lsc_interrupt_count++;

	pthread_cond_signal(&cvar);
	pthread_mutex_unlock(&mutex);

	return 0;
}

static inline int
lsc_timeout(int wait_us)
{
	int retval = 0;

	struct timespec ts;
	struct timeval tp;

	gettimeofday(&tp, NULL);

	/* Convert from timeval to timespec */
	ts.tv_sec = tp.tv_sec;
	ts.tv_nsec = tp.tv_usec * 1000;
	ts.tv_nsec += wait_us * 1000;
	/* Normalize tv_nsec to [0,999999999L] */
	while (ts.tv_nsec > 1000000000L) {
		ts.tv_nsec -= 1000000000L;
		ts.tv_sec += 1;
	}

	pthread_mutex_lock(&mutex);
	if (test_lsc_interrupt_count < 1)
		retval = pthread_cond_timedwait(&cvar, &mutex, &ts);

	pthread_mutex_unlock(&mutex);

	if (retval == 0 && test_lsc_interrupt_count < 1)
		return -1;

	return retval;
}

static int
test_status_interrupt(void)
{
	int member_count;
	uint16_t members[RTE_MAX_ETHPORTS];

	/* initialized bonding device with T members */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ROUND_ROBIN, 1,
			TEST_STATUS_INTERRUPT_MEMBER_COUNT, 1),
			"Failed to initialise bonding device");

	test_lsc_interrupt_count = 0;

	/* register link status change interrupt callback */
	rte_eth_dev_callback_register(test_params->bonding_port_id,
			RTE_ETH_EVENT_INTR_LSC, test_bonding_lsc_event_callback,
			&test_params->bonding_port_id);

	member_count = rte_eth_bond_active_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(member_count, TEST_STATUS_INTERRUPT_MEMBER_COUNT,
			"Number of active members (%d) is not as expected (%d)",
			member_count, TEST_STATUS_INTERRUPT_MEMBER_COUNT);

	/* Bring all 4 members link status to down and test that we have received a
	 * lsc interrupts */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[0], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[2], 0);

	TEST_ASSERT_EQUAL(test_lsc_interrupt_count, 0,
			"Received a link status change interrupt unexpectedly");

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[3], 0);

	TEST_ASSERT(lsc_timeout(TEST_LSC_WAIT_TIMEOUT_US) == 0,
			"timed out waiting for interrupt");

	TEST_ASSERT(test_lsc_interrupt_count > 0,
			"Did not receive link status change interrupt");

	member_count = rte_eth_bond_active_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(member_count, 0,
			"Number of active members (%d) is not as expected (%d)",
			member_count, 0);

	/* bring one member port up so link status will change */
	test_lsc_interrupt_count = 0;

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[0], 1);

	TEST_ASSERT(lsc_timeout(TEST_LSC_WAIT_TIMEOUT_US) == 0,
			"timed out waiting for interrupt");

	/* test that we have received another lsc interrupt */
	TEST_ASSERT(test_lsc_interrupt_count > 0,
			"Did not receive link status change interrupt");

	/*
	 * Verify that calling the same member lsc interrupt doesn't cause another
	 * lsc interrupt from bonding device.
	 */
	test_lsc_interrupt_count = 0;

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[0], 1);

	TEST_ASSERT(lsc_timeout(TEST_LSC_WAIT_TIMEOUT_US) != 0,
			"received unexpected interrupt");

	TEST_ASSERT_EQUAL(test_lsc_interrupt_count, 0,
			"Did not receive link status change interrupt");


	/* unregister lsc callback before exiting */
	rte_eth_dev_callback_unregister(test_params->bonding_port_id,
				RTE_ETH_EVENT_INTR_LSC, test_bonding_lsc_event_callback,
				&test_params->bonding_port_id);

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
generate_test_burst(struct rte_mbuf **pkts_burst, uint16_t burst_size,
		uint8_t vlan, uint8_t ipv4, uint8_t toggle_dst_mac,
		uint8_t toggle_ip_addr, uint16_t toggle_udp_port)
{
	uint16_t pktlen, generated_burst_size, ether_type;
	void *ip_hdr;

	if (ipv4)
		ether_type = RTE_ETHER_TYPE_IPV4;
	else
		ether_type = RTE_ETHER_TYPE_IPV6;

	if (toggle_dst_mac)
		initialize_eth_header(test_params->pkt_eth_hdr,
				(struct rte_ether_addr *)src_mac,
				(struct rte_ether_addr *)dst_mac_1,
				ether_type, vlan, vlan_id);
	else
		initialize_eth_header(test_params->pkt_eth_hdr,
				(struct rte_ether_addr *)src_mac,
				(struct rte_ether_addr *)dst_mac_0,
				ether_type, vlan, vlan_id);


	if (toggle_udp_port)
		pktlen = initialize_udp_header(test_params->pkt_udp_hdr, src_port,
				dst_port_1, 64);
	else
		pktlen = initialize_udp_header(test_params->pkt_udp_hdr, src_port,
				dst_port_0, 64);

	if (ipv4) {
		if (toggle_ip_addr)
			pktlen = initialize_ipv4_header(test_params->pkt_ipv4_hdr, src_addr,
					dst_addr_1, pktlen);
		else
			pktlen = initialize_ipv4_header(test_params->pkt_ipv4_hdr, src_addr,
					dst_addr_0, pktlen);

		ip_hdr = test_params->pkt_ipv4_hdr;
	} else {
		if (toggle_ip_addr)
			pktlen = initialize_ipv6_header(test_params->pkt_ipv6_hdr,
					(uint8_t *)src_ipv6_addr, (uint8_t *)dst_ipv6_addr_1,
					pktlen);
		else
			pktlen = initialize_ipv6_header(test_params->pkt_ipv6_hdr,
					(uint8_t *)src_ipv6_addr, (uint8_t *)dst_ipv6_addr_0,
					pktlen);

		ip_hdr = test_params->pkt_ipv6_hdr;
	}

	/* Generate burst of packets to transmit */
	generated_burst_size = generate_packet_burst(test_params->mbuf_pool,
			pkts_burst,	test_params->pkt_eth_hdr, vlan, ip_hdr, ipv4,
			test_params->pkt_udp_hdr, burst_size, PACKET_BURST_GEN_PKT_LEN_128,
			1);
	TEST_ASSERT_EQUAL(generated_burst_size, burst_size,
			"Failed to generate packet burst");

	return generated_burst_size;
}

/** Round Robin Mode Tests */

static int
test_roundrobin_tx_burst(void)
{
	int i, burst_size;
	struct rte_mbuf *pkt_burst[MAX_PKT_BURST];
	struct rte_eth_stats port_stats;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ROUND_ROBIN, 0, 2, 1),
			"Failed to initialise bonding device");

	burst_size = 20 * test_params->bonding_member_count;

	TEST_ASSERT(burst_size <= MAX_PKT_BURST,
			"Burst size specified is greater than supported.");

	/* Generate test bursts of packets to transmit */
	TEST_ASSERT_EQUAL(generate_test_burst(pkt_burst, burst_size, 0, 1, 0, 0, 0),
			burst_size, "failed to generate test burst");

	/* Send burst on bonding port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonding_port_id, 0, pkt_burst, burst_size), burst_size,
			"tx burst failed");

	/* Verify bonding port tx stats */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"Bonding Port (%d) opackets value (%u) not as expected (%d)\n",
			test_params->bonding_port_id, (unsigned int)port_stats.opackets,
			burst_size);

	/* Verify member ports tx stats */
	for (i = 0; i < test_params->bonding_member_count; i++) {
		rte_eth_stats_get(test_params->member_port_ids[i], &port_stats);
		TEST_ASSERT_EQUAL(port_stats.opackets,
				(uint64_t)burst_size / test_params->bonding_member_count,
				"Member Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->bonding_port_id, (unsigned int)port_stats.opackets,
				burst_size / test_params->bonding_member_count);
	}

	/* Put all members down and try and transmit */
	for (i = 0; i < test_params->bonding_member_count; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				test_params->member_port_ids[i], 0);
	}

	/* Send burst on bonding port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonding_port_id, 0,
			pkt_burst, burst_size), 0,
			"tx burst return unexpected value");

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
verify_mbufs_ref_count(struct rte_mbuf **mbufs, int nb_mbufs, int val)
{
	int i, refcnt;

	for (i = 0; i < nb_mbufs; i++) {
		refcnt = rte_mbuf_refcnt_read(mbufs[i]);
		TEST_ASSERT_EQUAL(refcnt, val,
			"mbuf ref count (%d)is not the expected value (%d)",
			refcnt, val);
	}
	return 0;
}

static void
free_mbufs(struct rte_mbuf **mbufs, int nb_mbufs)
{
	int i;

	for (i = 0; i < nb_mbufs; i++)
		rte_pktmbuf_free(mbufs[i]);
}

#define TEST_RR_MEMBER_TX_FAIL_MEMBER_COUNT		(2)
#define TEST_RR_MEMBER_TX_FAIL_BURST_SIZE		(64)
#define TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT		(22)
#define TEST_RR_MEMBER_TX_FAIL_FAILING_MEMBER_IDX	(1)

static int
test_roundrobin_tx_burst_member_tx_fail(void)
{
	struct rte_mbuf *pkt_burst[MAX_PKT_BURST];
	struct rte_mbuf *expected_tx_fail_pkts[MAX_PKT_BURST];

	struct rte_eth_stats port_stats;

	int i, first_fail_idx, tx_count;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ROUND_ROBIN, 0,
			TEST_RR_MEMBER_TX_FAIL_MEMBER_COUNT, 1),
			"Failed to initialise bonding device");

	/* Generate test bursts of packets to transmit */
	TEST_ASSERT_EQUAL(generate_test_burst(pkt_burst,
			TEST_RR_MEMBER_TX_FAIL_BURST_SIZE, 0, 1, 0, 0, 0),
			TEST_RR_MEMBER_TX_FAIL_BURST_SIZE,
			"Failed to generate test packet burst");

	/* Copy references to packets which we expect not to be transmitted */
	first_fail_idx = (TEST_RR_MEMBER_TX_FAIL_BURST_SIZE -
			(TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT *
			TEST_RR_MEMBER_TX_FAIL_MEMBER_COUNT)) +
			TEST_RR_MEMBER_TX_FAIL_FAILING_MEMBER_IDX;

	for (i = 0; i < TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT; i++) {
		expected_tx_fail_pkts[i] = pkt_burst[first_fail_idx +
				(i * TEST_RR_MEMBER_TX_FAIL_MEMBER_COUNT)];
	}

	/*
	 * Set virtual member to only fail transmission of
	 * TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT packets in burst.
	 */
	virtual_ethdev_tx_burst_fn_set_success(
			test_params->member_port_ids[TEST_RR_MEMBER_TX_FAIL_FAILING_MEMBER_IDX],
			0);

	virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(
			test_params->member_port_ids[TEST_RR_MEMBER_TX_FAIL_FAILING_MEMBER_IDX],
			TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT);

	tx_count = rte_eth_tx_burst(test_params->bonding_port_id, 0, pkt_burst,
			TEST_RR_MEMBER_TX_FAIL_BURST_SIZE);

	TEST_ASSERT_EQUAL(tx_count, TEST_RR_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT,
			"Transmitted (%d) an unexpected (%d) number of packets", tx_count,
			TEST_RR_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT);

	/* Verify that failed packet are expected failed packets */
	for (i = 0; i < TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT; i++) {
		TEST_ASSERT_EQUAL(expected_tx_fail_pkts[i], pkt_burst[i + tx_count],
				"expected mbuf (%d) pointer %p not expected pointer %p",
				i, expected_tx_fail_pkts[i], pkt_burst[i + tx_count]);
	}

	/* Verify bonding port tx stats */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)TEST_RR_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT,
			"Bonding Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.opackets,
			TEST_RR_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT);

	/* Verify member ports tx stats */
	for (i = 0; i < test_params->bonding_member_count; i++) {
		int member_expected_tx_count;

		rte_eth_stats_get(test_params->member_port_ids[i], &port_stats);

		member_expected_tx_count = TEST_RR_MEMBER_TX_FAIL_BURST_SIZE /
				test_params->bonding_member_count;

		if (i == TEST_RR_MEMBER_TX_FAIL_FAILING_MEMBER_IDX)
			member_expected_tx_count = member_expected_tx_count -
					TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT;

		TEST_ASSERT_EQUAL(port_stats.opackets,
				(uint64_t)member_expected_tx_count,
				"Member Port (%d) opackets value (%u) not as expected (%d)",
				test_params->member_port_ids[i],
				(unsigned int)port_stats.opackets, member_expected_tx_count);
	}

	/* Verify that all mbufs have a ref value of zero */
	TEST_ASSERT_SUCCESS(verify_mbufs_ref_count(&pkt_burst[tx_count],
			TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT, 1),
			"mbufs refcnts not as expected");
	free_mbufs(&pkt_burst[tx_count], TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT);

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_roundrobin_rx_burst_on_single_member(void)
{
	struct rte_mbuf *gen_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };

	struct rte_eth_stats port_stats;

	int i, j, burst_size = 25;

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ROUND_ROBIN, 0, 4, 1),
			"Failed to initialize bonding device with members");

	/* Generate test bursts of packets to transmit */
	TEST_ASSERT_EQUAL(generate_test_burst(
			gen_pkt_burst, burst_size, 0, 1, 0, 0, 0), burst_size,
			"burst generation failed");

	for (i = 0; i < test_params->bonding_member_count; i++) {
		/* Add rx data to member */
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[i],
				&gen_pkt_burst[0], burst_size);

		/* Call rx burst on bonding device */
		/* Send burst on bonding port */
		TEST_ASSERT_EQUAL(rte_eth_rx_burst(
				test_params->bonding_port_id, 0, rx_pkt_burst,
				MAX_PKT_BURST), burst_size,
				"round-robin rx burst failed");

		/* Verify bonding device rx count */
		rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
		TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
				"Bonding Port (%d) ipackets value (%u) not as expected (%d)",
				test_params->bonding_port_id,
				(unsigned int)port_stats.ipackets, burst_size);



		/* Verify bonding member devices rx count */
		/* Verify member ports tx stats */
		for (j = 0; j < test_params->bonding_member_count; j++) {
			rte_eth_stats_get(test_params->member_port_ids[j], &port_stats);

			if (i == j) {
				TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
						"Member Port (%d) ipackets value (%u) not as expected"
						" (%d)", test_params->member_port_ids[i],
						(unsigned int)port_stats.ipackets, burst_size);
			} else {
				TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
						"Member Port (%d) ipackets value (%u) not as expected"
						" (%d)", test_params->member_port_ids[i],
						(unsigned int)port_stats.ipackets, 0);
			}

			/* Reset bonding members stats */
			rte_eth_stats_reset(test_params->member_port_ids[j]);
		}
		/* reset bonding device stats */
		rte_eth_stats_reset(test_params->bonding_port_id);
	}

	/* free mbufs */
	for (i = 0; i < MAX_PKT_BURST; i++) {
		rte_pktmbuf_free(rx_pkt_burst[i]);
	}


	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

#define TEST_ROUNDROBIN_TX_BURST_MEMBER_COUNT (3)

static int
test_roundrobin_rx_burst_on_multiple_members(void)
{
	struct rte_mbuf *gen_pkt_burst[TEST_ROUNDROBIN_TX_BURST_MEMBER_COUNT][MAX_PKT_BURST];

	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	int burst_size[TEST_ROUNDROBIN_TX_BURST_MEMBER_COUNT] = { 15, 13, 36 };
	int i, nb_rx;

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ROUND_ROBIN, 0, 4, 1),
			"Failed to initialize bonding device with members");

	/* Generate test bursts of packets to transmit */
	for (i = 0; i < TEST_ROUNDROBIN_TX_BURST_MEMBER_COUNT; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[i][0], burst_size[i], 0, 1, 0, 0, 0),
				burst_size[i], "burst generation failed");
	}

	/* Add rx data to members */
	for (i = 0; i < TEST_ROUNDROBIN_TX_BURST_MEMBER_COUNT; i++) {
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[i],
				&gen_pkt_burst[i][0], burst_size[i]);
	}

	/* Call rx burst on bonding device */
	/* Send burst on bonding port */
	nb_rx = rte_eth_rx_burst(test_params->bonding_port_id, 0, rx_pkt_burst,
			MAX_PKT_BURST);
	TEST_ASSERT_EQUAL(nb_rx , burst_size[0] + burst_size[1] + burst_size[2],
			"round-robin rx burst failed (%d != %d)\n", nb_rx,
			burst_size[0] + burst_size[1] + burst_size[2]);

	/* Verify bonding device rx count */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets,
			(uint64_t)(burst_size[0] + burst_size[1] + burst_size[2]),
			"Bonding Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.ipackets,
			burst_size[0] + burst_size[1] + burst_size[2]);

	/* Verify bonding member devices rx counts */
	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[0],
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->member_port_ids[0],
			(unsigned int)port_stats.ipackets, burst_size[0]);

	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[1],
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->member_port_ids[1], (unsigned int)port_stats.ipackets,
			burst_size[1]);

	rte_eth_stats_get(test_params->member_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[2],
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
				test_params->member_port_ids[2],
				(unsigned int)port_stats.ipackets, burst_size[2]);

	rte_eth_stats_get(test_params->member_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->member_port_ids[3],
			(unsigned int)port_stats.ipackets, 0);

	/* free mbufs */
	for (i = 0; i < MAX_PKT_BURST; i++) {
		rte_pktmbuf_free(rx_pkt_burst[i]);
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_roundrobin_verify_mac_assignment(void)
{
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr expected_mac_addr_0, expected_mac_addr_2;

	int i;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0],
			&expected_mac_addr_0),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[2],
			&expected_mac_addr_2),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[2]);

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
				BONDING_MODE_ROUND_ROBIN, 0, 4, 1),
				"Failed to initialize bonding device with members");

	/* Verify that all MACs are the same as first member added to bonding dev */
	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[i],
				&read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->member_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr)),
				"member port (%d) mac address not set to that of primary port",
				test_params->member_port_ids[i]);
	}

	/* change primary and verify that MAC addresses haven't changed */
	TEST_ASSERT_SUCCESS(rte_eth_bond_primary_set(test_params->bonding_port_id,
			test_params->member_port_ids[2]),
			"Failed to set bonding port (%d) primary port to (%d)",
			test_params->bonding_port_id, test_params->member_port_ids[i]);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[i],
				&read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->member_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr)),
				"member port (%d) mac address has changed to that of primary"
				" port without stop/start toggle of bonding device",
				test_params->member_port_ids[i]);
	}

	/*
	 * stop / start bonding device and verify that primary MAC address is
	 * propagate to bonding device and members.
	 */
	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonding_port_id),
			"Failed to stop bonding port %u",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonding_port_id),
			"Failed to start bonding device");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(
			memcmp(&expected_mac_addr_2, &read_mac_addr, sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of new primary port",
			test_params->member_port_ids[i]);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[i],
				&read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->member_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_2, &read_mac_addr,
				sizeof(read_mac_addr)),
				"member port (%d) mac address not set to that of new primary"
				" port", test_params->member_port_ids[i]);
	}

	/* Set explicit MAC address */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonding_port_id,
			(struct rte_ether_addr *)bonding_mac),
			"Failed to set MAC");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(bonding_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of new primary port",
				test_params->member_port_ids[i]);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[i],
				&read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->member_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(bonding_mac, &read_mac_addr,
				sizeof(read_mac_addr)), "member port (%d) mac address not set to"
				" that of new primary port\n", test_params->member_port_ids[i]);
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_roundrobin_verify_promiscuous_enable_disable(void)
{
	int i, promiscuous_en;
	int ret;

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ROUND_ROBIN, 0, 4, 1),
			"Failed to initialize bonding device with members");

	ret = rte_eth_promiscuous_enable(test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to enable promiscuous mode for port %d: %s",
		test_params->bonding_port_id, rte_strerror(-ret));

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonding_port_id);
	TEST_ASSERT_EQUAL(promiscuous_en, 1,
			"Port (%d) promiscuous mode not enabled",
			test_params->bonding_port_id);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->member_port_ids[i]);
		TEST_ASSERT_EQUAL(promiscuous_en, 1,
				"member port (%d) promiscuous mode not enabled",
				test_params->member_port_ids[i]);
	}

	ret = rte_eth_promiscuous_disable(test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to disable promiscuous mode for port %d: %s",
		test_params->bonding_port_id, rte_strerror(-ret));

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonding_port_id);
	TEST_ASSERT_EQUAL(promiscuous_en, 0,
			"Port (%d) promiscuous mode not disabled\n",
			test_params->bonding_port_id);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->member_port_ids[i]);
		TEST_ASSERT_EQUAL(promiscuous_en, 0,
				"Port (%d) promiscuous mode not disabled\n",
				test_params->member_port_ids[i]);
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

#define TEST_RR_LINK_STATUS_MEMBER_COUNT (4)
#define TEST_RR_LINK_STATUS_EXPECTED_ACTIVE_MEMBER_COUNT (2)

static int
test_roundrobin_verify_member_link_status_change_behaviour(void)
{
	struct rte_mbuf *tx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_mbuf *gen_pkt_burst[TEST_RR_LINK_STATUS_MEMBER_COUNT][MAX_PKT_BURST];
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };

	struct rte_eth_stats port_stats;
	uint16_t members[RTE_MAX_ETHPORTS];

	int i, burst_size, member_count;

	/* NULL all pointers in array to simplify cleanup */
	memset(gen_pkt_burst, 0, sizeof(gen_pkt_burst));

	/* Initialize bonding device with TEST_RR_LINK_STATUS_MEMBER_COUNT members
	 * in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ROUND_ROBIN, 0, TEST_RR_LINK_STATUS_MEMBER_COUNT, 1),
			"Failed to initialize bonding device with members");

	/* Verify Current Members Count /Active Member Count is */
	member_count = rte_eth_bond_members_get(test_params->bonding_port_id, members,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count, TEST_RR_LINK_STATUS_MEMBER_COUNT,
			"Number of members (%d) is not as expected (%d).",
			member_count, TEST_RR_LINK_STATUS_MEMBER_COUNT);

	member_count = rte_eth_bond_active_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count, TEST_RR_LINK_STATUS_MEMBER_COUNT,
			"Number of active members (%d) is not as expected (%d).",
			member_count, TEST_RR_LINK_STATUS_MEMBER_COUNT);

	/* Set 2 members eth_devs link status to down */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[3], 0);

	member_count = rte_eth_bond_active_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count,
			TEST_RR_LINK_STATUS_EXPECTED_ACTIVE_MEMBER_COUNT,
			"Number of active members (%d) is not as expected (%d).\n",
			member_count, TEST_RR_LINK_STATUS_EXPECTED_ACTIVE_MEMBER_COUNT);

	burst_size = 20;

	/* Verify that pkts are not sent on members with link status down:
	 *
	 * 1. Generate test burst of traffic
	 * 2. Transmit burst on bonding eth_dev
	 * 3. Verify stats for bonding eth_dev (opackets = burst_size)
	 * 4. Verify stats for member eth_devs (s0 = 10, s1 = 0, s2 = 10, s3 = 0)
	 */
	TEST_ASSERT_EQUAL(
			generate_test_burst(tx_pkt_burst, burst_size, 0, 1, 0, 0, 0),
			burst_size, "generate_test_burst failed");

	rte_eth_stats_reset(test_params->bonding_port_id);


	TEST_ASSERT_EQUAL(
			rte_eth_tx_burst(test_params->bonding_port_id, 0, tx_pkt_burst,
			burst_size), burst_size, "rte_eth_tx_burst failed");

	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"Port (%d) opackets stats (%d) not expected (%d) value",
			test_params->bonding_port_id, (int)port_stats.opackets,
			burst_size);

	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)10,
			"Port (%d) opackets stats (%d) not expected (%d) value",
			test_params->member_port_ids[0], (int)port_stats.opackets, 10);

	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)0,
			"Port (%d) opackets stats (%d) not expected (%d) value",
			test_params->member_port_ids[1], (int)port_stats.opackets, 0);

	rte_eth_stats_get(test_params->member_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)10,
			"Port (%d) opackets stats (%d) not expected (%d) value",
			test_params->member_port_ids[2], (int)port_stats.opackets, 10);

	rte_eth_stats_get(test_params->member_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)0,
			"Port (%d) opackets stats (%d) not expected (%d) value",
			test_params->member_port_ids[3], (int)port_stats.opackets, 0);

	/* Verify that pkts are not sent on members with link status down:
	 *
	 * 1. Generate test bursts of traffic
	 * 2. Add bursts on to virtual eth_devs
	 * 3. Rx burst on bonding eth_dev, expected (burst_ size *
	 *    TEST_RR_LINK_STATUS_EXPECTED_ACTIVE_MEMBER_COUNT) received
	 * 4. Verify stats for bonding eth_dev
	 * 6. Verify stats for member eth_devs (s0 = 10, s1 = 0, s2 = 10, s3 = 0)
	 */
	for (i = 0; i < TEST_RR_LINK_STATUS_MEMBER_COUNT; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[i][0], burst_size, 0, 1, 0, 0, 0),
				burst_size, "failed to generate packet burst");

		virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[i],
				&gen_pkt_burst[i][0], burst_size);
	}

	TEST_ASSERT_EQUAL(rte_eth_rx_burst(
			test_params->bonding_port_id, 0, rx_pkt_burst, MAX_PKT_BURST),
			burst_size + burst_size,
			"rte_eth_rx_burst failed");

	/* Verify bonding device rx count */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets , (uint64_t)(burst_size + burst_size),
			"(%d) port_stats.ipackets not as expected\n",
			test_params->bonding_port_id);

	/* free mbufs */
	for (i = 0; i < MAX_PKT_BURST; i++) {
		rte_pktmbuf_free(rx_pkt_burst[i]);
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

#define TEST_RR_POLLING_LINK_STATUS_MEMBER_COUNT (2)

uint8_t polling_member_mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00 };


int polling_test_members[TEST_RR_POLLING_LINK_STATUS_MEMBER_COUNT] = { -1, -1 };

static int
test_roundrobin_verify_polling_member_link_status_change(void)
{
	struct rte_ether_addr *mac_addr =
		(struct rte_ether_addr *)polling_member_mac;
	char member_name[RTE_ETH_NAME_MAX_LEN];

	int i;

	for (i = 0; i < TEST_RR_POLLING_LINK_STATUS_MEMBER_COUNT; i++) {
		/* Generate member name / MAC address */
		snprintf(member_name, RTE_ETH_NAME_MAX_LEN, "eth_virt_poll_%d", i);
		mac_addr->addr_bytes[RTE_ETHER_ADDR_LEN-1] = i;

		/* Create member devices with no ISR Support */
		if (polling_test_members[i] == -1) {
			polling_test_members[i] = virtual_ethdev_create(member_name, mac_addr,
					rte_socket_id(), 0);
			TEST_ASSERT(polling_test_members[i] >= 0,
					"Failed to create virtual ethdev %s\n", member_name);

			/* Configure member */
			TEST_ASSERT_SUCCESS(configure_ethdev(polling_test_members[i], 0, 0),
					"Failed to configure virtual ethdev %s(%d)", member_name,
					polling_test_members[i]);
		}

		/* Add member to bonding device */
		TEST_ASSERT_SUCCESS(rte_eth_bond_member_add(test_params->bonding_port_id,
				polling_test_members[i]),
				"Failed to add member %s(%d) to bonding device %d",
				member_name, polling_test_members[i],
				test_params->bonding_port_id);
	}

	/* Initialize bonding device */
	TEST_ASSERT_SUCCESS(configure_ethdev(test_params->bonding_port_id, 1, 1),
			"Failed to configure bonding device %d",
			test_params->bonding_port_id);


	/* Register link status change interrupt callback */
	rte_eth_dev_callback_register(test_params->bonding_port_id,
			RTE_ETH_EVENT_INTR_LSC, test_bonding_lsc_event_callback,
			&test_params->bonding_port_id);

	/* link status change callback for first member link up */
	test_lsc_interrupt_count = 0;

	virtual_ethdev_set_link_status(polling_test_members[0], 1);

	TEST_ASSERT_SUCCESS(lsc_timeout(15000), "timed out waiting for interrupt");


	/* no link status change callback for second member link up */
	test_lsc_interrupt_count = 0;

	virtual_ethdev_set_link_status(polling_test_members[1], 1);

	TEST_ASSERT_FAIL(lsc_timeout(15000), "unexpectedly succeeded");

	/* link status change callback for both member links down */
	test_lsc_interrupt_count = 0;

	virtual_ethdev_set_link_status(polling_test_members[0], 0);
	virtual_ethdev_set_link_status(polling_test_members[1], 0);

	TEST_ASSERT_SUCCESS(lsc_timeout(20000), "timed out waiting for interrupt");

	/* Un-Register link status change interrupt callback */
	rte_eth_dev_callback_unregister(test_params->bonding_port_id,
			RTE_ETH_EVENT_INTR_LSC, test_bonding_lsc_event_callback,
			&test_params->bonding_port_id);


	/* Clean up and remove members from bonding device */
	for (i = 0; i < TEST_RR_POLLING_LINK_STATUS_MEMBER_COUNT; i++) {

		TEST_ASSERT_SUCCESS(
				rte_eth_bond_member_remove(test_params->bonding_port_id,
						polling_test_members[i]),
				"Failed to remove member %d from bonding port (%d)",
				polling_test_members[i], test_params->bonding_port_id);
	}

	return remove_members_and_stop_bonding_device();
}


/** Active Backup Mode Tests */

static int
test_activebackup_tx_burst(void)
{
	int i, pktlen, primary_port, burst_size;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_eth_stats port_stats;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ACTIVE_BACKUP, 0, 1, 1),
			"Failed to initialize bonding device with members");

	initialize_eth_header(test_params->pkt_eth_hdr,
			(struct rte_ether_addr *)src_mac,
			(struct rte_ether_addr *)dst_mac_0,
			RTE_ETHER_TYPE_IPV4,  0, 0);
	pktlen = initialize_udp_header(test_params->pkt_udp_hdr, src_port,
			dst_port_0, 16);
	pktlen = initialize_ipv4_header(test_params->pkt_ipv4_hdr, src_addr,
			dst_addr_0, pktlen);

	burst_size = 20 * test_params->bonding_member_count;

	TEST_ASSERT(burst_size < MAX_PKT_BURST,
			"Burst size specified is greater than supported.");

	/* Generate a burst of packets to transmit */
	TEST_ASSERT_EQUAL(generate_packet_burst(test_params->mbuf_pool, pkts_burst,
			test_params->pkt_eth_hdr, 0, test_params->pkt_ipv4_hdr, 1,
			test_params->pkt_udp_hdr, burst_size, PACKET_BURST_GEN_PKT_LEN, 1),
			burst_size,	"failed to generate burst correctly");

	/* Send burst on bonding port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonding_port_id, 0, pkts_burst,
			burst_size),  burst_size, "tx burst failed");

	/* Verify bonding port tx stats */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"Bonding Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.opackets,
			burst_size);

	primary_port = rte_eth_bond_primary_get(test_params->bonding_port_id);

	/* Verify member ports tx stats */
	for (i = 0; i < test_params->bonding_member_count; i++) {
		rte_eth_stats_get(test_params->member_port_ids[i], &port_stats);
		if (test_params->member_port_ids[i] == primary_port) {
			TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
					"Member Port (%d) opackets value (%u) not as expected (%d)",
					test_params->bonding_port_id,
					(unsigned int)port_stats.opackets,
					burst_size / test_params->bonding_member_count);
		} else {
			TEST_ASSERT_EQUAL(port_stats.opackets, 0,
					"Member Port (%d) opackets value (%u) not as expected (%d)",
					test_params->bonding_port_id,
					(unsigned int)port_stats.opackets, 0);
		}
	}

	/* Put all members down and try and transmit */
	for (i = 0; i < test_params->bonding_member_count; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				test_params->member_port_ids[i], 0);
	}

	/* Send burst on bonding port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonding_port_id, 0,
			pkts_burst, burst_size), 0, "Sending empty burst failed");

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

#define TEST_ACTIVE_BACKUP_RX_BURST_MEMBER_COUNT (4)

static int
test_activebackup_rx_burst(void)
{
	struct rte_mbuf *gen_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };

	struct rte_eth_stats port_stats;

	int primary_port;

	int i, j, burst_size = 17;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ACTIVE_BACKUP, 0,
			TEST_ACTIVE_BACKUP_RX_BURST_MEMBER_COUNT, 1),
			"Failed to initialize bonding device with members");

	primary_port = rte_eth_bond_primary_get(test_params->bonding_port_id);
	TEST_ASSERT(primary_port >= 0,
			"failed to get primary member for bonding port (%d)",
			test_params->bonding_port_id);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		/* Generate test bursts of packets to transmit */
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[0], burst_size, 0, 1, 0, 0, 0),
				burst_size, "burst generation failed");

		/* Add rx data to member */
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[i],
				&gen_pkt_burst[0], burst_size);

		/* Call rx burst on bonding device */
		TEST_ASSERT_EQUAL(rte_eth_rx_burst(test_params->bonding_port_id, 0,
				&rx_pkt_burst[0], MAX_PKT_BURST), burst_size,
				"rte_eth_rx_burst failed");

		if (test_params->member_port_ids[i] == primary_port) {
			/* Verify bonding device rx count */
			rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
			TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
					"Bonding Port (%d) ipackets value (%u) not as expected (%d)",
					test_params->bonding_port_id,
					(unsigned int)port_stats.ipackets, burst_size);

			/* Verify bonding member devices rx count */
			for (j = 0; j < test_params->bonding_member_count; j++) {
				rte_eth_stats_get(test_params->member_port_ids[j], &port_stats);
				if (i == j) {
					TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
							"Member Port (%d) ipackets value (%u) not as "
							"expected (%d)",
							test_params->member_port_ids[i],
							(unsigned int)port_stats.ipackets,
							burst_size);
				} else {
					TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
							"Member Port (%d) ipackets value (%u) not as "
							"expected (%d)\n",
							test_params->member_port_ids[i],
							(unsigned int)port_stats.ipackets, 0);
				}
			}
		} else {
			for (j = 0; j < test_params->bonding_member_count; j++) {
				rte_eth_stats_get(test_params->member_port_ids[j], &port_stats);
				TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
						"Member Port (%d) ipackets value (%u) not as expected "
						"(%d)", test_params->member_port_ids[i],
						(unsigned int)port_stats.ipackets, 0);
			}
		}

		/* free mbufs */
		for (i = 0; i < MAX_PKT_BURST; i++) {
			if (rx_pkt_burst[i] != NULL) {
				rte_pktmbuf_free(rx_pkt_burst[i]);
				rx_pkt_burst[i] = NULL;
			}
		}

		/* reset bonding device stats */
		rte_eth_stats_reset(test_params->bonding_port_id);
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_activebackup_verify_promiscuous_enable_disable(void)
{
	int i, primary_port, promiscuous_en;
	int ret;

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ACTIVE_BACKUP, 0, 4, 1),
			"Failed to initialize bonding device with members");

	primary_port = rte_eth_bond_primary_get(test_params->bonding_port_id);
	TEST_ASSERT(primary_port >= 0,
			"failed to get primary member for bonding port (%d)",
			test_params->bonding_port_id);

	ret = rte_eth_promiscuous_enable(test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to enable promiscuous mode for port %d: %s",
		test_params->bonding_port_id, rte_strerror(-ret));

	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonding_port_id), 1,
			"Port (%d) promiscuous mode not enabled",
			test_params->bonding_port_id);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->member_port_ids[i]);
		if (primary_port == test_params->member_port_ids[i]) {
			TEST_ASSERT_EQUAL(promiscuous_en, 1,
					"member port (%d) promiscuous mode not enabled",
					test_params->member_port_ids[i]);
		} else {
			TEST_ASSERT_EQUAL(promiscuous_en, 0,
					"member port (%d) promiscuous mode enabled",
					test_params->member_port_ids[i]);
		}

	}

	ret = rte_eth_promiscuous_disable(test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to disable promiscuous mode for port %d: %s",
		test_params->bonding_port_id, rte_strerror(-ret));

	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonding_port_id), 0,
			"Port (%d) promiscuous mode not disabled\n",
			test_params->bonding_port_id);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->member_port_ids[i]);
		TEST_ASSERT_EQUAL(promiscuous_en, 0,
				"member port (%d) promiscuous mode not disabled\n",
				test_params->member_port_ids[i]);
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_activebackup_verify_mac_assignment(void)
{
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr expected_mac_addr_0, expected_mac_addr_1;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0],
			&expected_mac_addr_0),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1],
			&expected_mac_addr_1),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);

	/* Initialize bonding device with 2 members in active backup mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ACTIVE_BACKUP, 0, 2, 1),
			"Failed to initialize bonding device with members");

	/* Verify that bonding MACs is that of first member and that the other member
	 * MAC hasn't been changed */
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of primary port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not as expected",
			test_params->member_port_ids[1]);

	/* change primary and verify that MAC addresses haven't changed */
	TEST_ASSERT_EQUAL(rte_eth_bond_primary_set(test_params->bonding_port_id,
			test_params->member_port_ids[1]), 0,
			"Failed to set bonding port (%d) primary port to (%d)",
			test_params->bonding_port_id, test_params->member_port_ids[1]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of primary port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not as expected",
			test_params->member_port_ids[1]);

	/*
	 * stop / start bonding device and verify that primary MAC address is
	 * propagated to bonding device and members.
	 */

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonding_port_id),
			"Failed to stop bonding port %u",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonding_port_id),
			"Failed to start device");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of primary port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not as expected",
			test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[1]);

	/* Set explicit MAC address */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonding_port_id,
			(struct rte_ether_addr *)bonding_mac),
			"failed to set MAC address");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&bonding_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of bonding port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not as expected",
			test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&bonding_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of bonding port",
			test_params->member_port_ids[1]);

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_activebackup_verify_member_link_status_change_failover(void)
{
	struct rte_mbuf *pkt_burst[TEST_ACTIVE_BACKUP_RX_BURST_MEMBER_COUNT][MAX_PKT_BURST];
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	uint16_t members[RTE_MAX_ETHPORTS];

	int i, burst_size, member_count, primary_port;

	burst_size = 21;

	memset(pkt_burst, 0, sizeof(pkt_burst));

	/* Generate packet burst for testing */
	TEST_ASSERT_EQUAL(generate_test_burst(
			&pkt_burst[0][0], burst_size, 0, 1, 0, 0, 0), burst_size,
			"generate_test_burst failed");

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ACTIVE_BACKUP, 0,
			TEST_ACTIVE_BACKUP_RX_BURST_MEMBER_COUNT, 1),
			"Failed to initialize bonding device with members");

	/* Verify Current Members Count /Active Member Count is */
	member_count = rte_eth_bond_members_get(test_params->bonding_port_id, members,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count, 4,
			"Number of members (%d) is not as expected (%d).",
			member_count, 4);

	member_count = rte_eth_bond_active_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count, 4,
			"Number of active members (%d) is not as expected (%d).",
			member_count, 4);

	primary_port = rte_eth_bond_primary_get(test_params->bonding_port_id);
	TEST_ASSERT_EQUAL(primary_port, test_params->member_port_ids[0],
			"Primary port not as expected");

	/* Bring 2 members down and verify active member count */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[3], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_members_get(
			test_params->bonding_port_id, members, RTE_MAX_ETHPORTS), 2,
			"Number of active members (%d) is not as expected (%d).",
			member_count, 2);

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[1], 1);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[3], 1);


	/* Bring primary port down, verify that active member count is 3 and primary
	 *  has changed */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[0], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_members_get(
			test_params->bonding_port_id, members, RTE_MAX_ETHPORTS),
			3,
			"Number of active members (%d) is not as expected (%d).",
			member_count, 3);

	primary_port = rte_eth_bond_primary_get(test_params->bonding_port_id);
	TEST_ASSERT_EQUAL(primary_port, test_params->member_port_ids[2],
			"Primary port not as expected");

	/* Verify that pkts are sent on new primary member */

	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonding_port_id, 0, &pkt_burst[0][0],
			burst_size), burst_size, "rte_eth_tx_burst failed");

	rte_eth_stats_get(test_params->member_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets not as expected",
			test_params->member_port_ids[2]);

	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->member_port_ids[0]);

	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->member_port_ids[1]);

	rte_eth_stats_get(test_params->member_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->member_port_ids[3]);

	/* Generate packet burst for testing */

	for (i = 0; i < TEST_ACTIVE_BACKUP_RX_BURST_MEMBER_COUNT; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&pkt_burst[i][0], burst_size, 0, 1, 0, 0, 0), burst_size,
				"generate_test_burst failed");

		virtual_ethdev_add_mbufs_to_rx_queue(
			test_params->member_port_ids[i], &pkt_burst[i][0], burst_size);
	}

	TEST_ASSERT_EQUAL(rte_eth_rx_burst(
			test_params->bonding_port_id, 0, rx_pkt_burst, MAX_PKT_BURST),
			burst_size, "rte_eth_rx_burst\n");

	/* Verify bonding device rx count */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
			"(%d) port_stats.ipackets not as expected",
			test_params->bonding_port_id);

	rte_eth_stats_get(test_params->member_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets not as expected",
			test_params->member_port_ids[2]);

	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected",
			test_params->member_port_ids[0]);

	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected",
			test_params->member_port_ids[1]);

	rte_eth_stats_get(test_params->member_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected",
			test_params->member_port_ids[3]);

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

/** Balance Mode Tests */

static int
test_balance_xmit_policy_configuration(void)
{
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_ACTIVE_BACKUP, 0, 2, 1),
			"Failed to initialize_bonding_device_with_members.");

	/* Invalid port id */
	TEST_ASSERT_FAIL(rte_eth_bond_xmit_policy_set(
			INVALID_PORT_ID, BALANCE_XMIT_POLICY_LAYER2),
			"Expected call to failed as invalid port specified.");

	/* Set xmit policy on non bonding device */
	TEST_ASSERT_FAIL(rte_eth_bond_xmit_policy_set(
			test_params->member_port_ids[0],	BALANCE_XMIT_POLICY_LAYER2),
			"Expected call to failed as invalid port specified.");


	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonding_port_id, BALANCE_XMIT_POLICY_LAYER2),
			"Failed to set balance xmit policy.");

	TEST_ASSERT_EQUAL(rte_eth_bond_xmit_policy_get(test_params->bonding_port_id),
			BALANCE_XMIT_POLICY_LAYER2, "balance xmit policy not as expected.");


	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonding_port_id, BALANCE_XMIT_POLICY_LAYER23),
			"Failed to set balance xmit policy.");

	TEST_ASSERT_EQUAL(rte_eth_bond_xmit_policy_get(test_params->bonding_port_id),
			BALANCE_XMIT_POLICY_LAYER23,
			"balance xmit policy not as expected.");


	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonding_port_id, BALANCE_XMIT_POLICY_LAYER34),
			"Failed to set balance xmit policy.");

	TEST_ASSERT_EQUAL(rte_eth_bond_xmit_policy_get(test_params->bonding_port_id),
			BALANCE_XMIT_POLICY_LAYER34,
			"balance xmit policy not as expected.");

	/* Invalid port id */
	TEST_ASSERT_FAIL(rte_eth_bond_xmit_policy_get(INVALID_PORT_ID),
			"Expected call to failed as invalid port specified.");

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

#define TEST_BALANCE_L2_TX_BURST_MEMBER_COUNT (2)

static int
test_balance_l2_tx_burst(void)
{
	struct rte_mbuf *pkts_burst[TEST_BALANCE_L2_TX_BURST_MEMBER_COUNT][MAX_PKT_BURST];
	int burst_size[TEST_BALANCE_L2_TX_BURST_MEMBER_COUNT] = { 10, 15 };

	uint16_t pktlen;
	int i;
	struct rte_eth_stats port_stats;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BALANCE, 0, TEST_BALANCE_L2_TX_BURST_MEMBER_COUNT, 1),
			"Failed to initialize_bonding_device_with_members.");

	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonding_port_id, BALANCE_XMIT_POLICY_LAYER2),
			"Failed to set balance xmit policy.");

	initialize_eth_header(test_params->pkt_eth_hdr,
			(struct rte_ether_addr *)src_mac,
			(struct rte_ether_addr *)dst_mac_0,
			RTE_ETHER_TYPE_IPV4, 0, 0);
	pktlen = initialize_udp_header(test_params->pkt_udp_hdr, src_port,
			dst_port_0, 16);
	pktlen = initialize_ipv4_header(test_params->pkt_ipv4_hdr, src_addr,
			dst_addr_0, pktlen);

	/* Generate a burst 1 of packets to transmit */
	TEST_ASSERT_EQUAL(generate_packet_burst(test_params->mbuf_pool, &pkts_burst[0][0],
			test_params->pkt_eth_hdr, 0, test_params->pkt_ipv4_hdr, 1,
			test_params->pkt_udp_hdr, burst_size[0],
			PACKET_BURST_GEN_PKT_LEN, 1), burst_size[0],
			"failed to generate packet burst");

	initialize_eth_header(test_params->pkt_eth_hdr,
			(struct rte_ether_addr *)src_mac,
			(struct rte_ether_addr *)dst_mac_1,
			RTE_ETHER_TYPE_IPV4, 0, 0);

	/* Generate a burst 2 of packets to transmit */
	TEST_ASSERT_EQUAL(generate_packet_burst(test_params->mbuf_pool, &pkts_burst[1][0],
			test_params->pkt_eth_hdr, 0, test_params->pkt_ipv4_hdr, 1,
			test_params->pkt_udp_hdr, burst_size[1],
			PACKET_BURST_GEN_PKT_LEN, 1), burst_size[1],
			"failed to generate packet burst");

	/* Send burst 1 on bonding port */
	for (i = 0; i < TEST_BALANCE_L2_TX_BURST_MEMBER_COUNT; i++) {
		TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonding_port_id, 0,
				&pkts_burst[i][0], burst_size[i]),
				burst_size[i], "Failed to transmit packet burst");
	}

	/* Verify bonding port tx stats */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)(burst_size[0] + burst_size[1]),
			"Bonding Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.opackets,
			burst_size[0] + burst_size[1]);


	/* Verify member ports tx stats */
	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size[0],
			"Member Port (%d) opackets value (%u) not as expected (%d)",
			test_params->member_port_ids[0], (unsigned int)port_stats.opackets,
			burst_size[0]);

	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size[1],
			"Member Port (%d) opackets value (%u) not as expected (%d)\n",
			test_params->member_port_ids[1], (unsigned int)port_stats.opackets,
			burst_size[1]);

	/* Put all members down and try and transmit */
	for (i = 0; i < test_params->bonding_member_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->member_port_ids[i], 0);
	}

	/* Send burst on bonding port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonding_port_id, 0, &pkts_burst[0][0], burst_size[0]),
			0, "Expected zero packet");

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
balance_l23_tx_burst(uint8_t vlan_enabled, uint8_t ipv4,
		uint8_t toggle_mac_addr, uint8_t toggle_ip_addr)
{
	int i, burst_size_1, burst_size_2, nb_tx_1, nb_tx_2;

	struct rte_mbuf *pkts_burst_1[MAX_PKT_BURST];
	struct rte_mbuf *pkts_burst_2[MAX_PKT_BURST];

	struct rte_eth_stats port_stats;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BALANCE, 0, 2, 1),
			"Failed to initialize_bonding_device_with_members.");

	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonding_port_id, BALANCE_XMIT_POLICY_LAYER23),
			"Failed to set balance xmit policy.");

	burst_size_1 = 20;
	burst_size_2 = 10;

	TEST_ASSERT(burst_size_1 < MAX_PKT_BURST || burst_size_2 < MAX_PKT_BURST,
			"Burst size specified is greater than supported.");

	/* Generate test bursts of packets to transmit */
	TEST_ASSERT_EQUAL(generate_test_burst(
			pkts_burst_1, burst_size_1, vlan_enabled, ipv4, 0, 0, 0),
			burst_size_1, "failed to generate packet burst");

	TEST_ASSERT_EQUAL(generate_test_burst(pkts_burst_2, burst_size_2, vlan_enabled, ipv4,
			toggle_mac_addr, toggle_ip_addr, 0), burst_size_2,
			"failed to generate packet burst");

	/* Send burst 1 on bonding port */
	nb_tx_1 = rte_eth_tx_burst(test_params->bonding_port_id, 0, pkts_burst_1,
			burst_size_1);
	TEST_ASSERT_EQUAL(nb_tx_1, burst_size_1, "tx burst failed");

	/* Send burst 2 on bonding port */
	nb_tx_2 = rte_eth_tx_burst(test_params->bonding_port_id, 0, pkts_burst_2,
			burst_size_2);
	TEST_ASSERT_EQUAL(nb_tx_2, burst_size_2, "tx burst failed");

	/* Verify bonding port tx stats */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)(nb_tx_1 + nb_tx_2),
			"Bonding Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.opackets,
			nb_tx_1 + nb_tx_2);

	/* Verify member ports tx stats */
	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)nb_tx_1,
			"Member Port (%d) opackets value (%u) not as expected (%d)",
			test_params->member_port_ids[0], (unsigned int)port_stats.opackets,
			nb_tx_1);

	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)nb_tx_2,
			"Member Port (%d) opackets value (%u) not as expected (%d)",
			test_params->member_port_ids[1], (unsigned int)port_stats.opackets,
			nb_tx_2);

	/* Put all members down and try and transmit */
	for (i = 0; i < test_params->bonding_member_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->member_port_ids[i], 0);
	}

	/* Send burst on bonding port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonding_port_id, 0, pkts_burst_1,
			burst_size_1), 0, "Expected zero packet");


	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_balance_l23_tx_burst_ipv4_toggle_ip_addr(void)
{
	return balance_l23_tx_burst(0, 1, 0, 1);
}

static int
test_balance_l23_tx_burst_vlan_ipv4_toggle_ip_addr(void)
{
	return balance_l23_tx_burst(1, 1, 0, 1);
}

static int
test_balance_l23_tx_burst_ipv6_toggle_ip_addr(void)
{
	return balance_l23_tx_burst(0, 0, 0, 1);
}

static int
test_balance_l23_tx_burst_vlan_ipv6_toggle_ip_addr(void)
{
	return balance_l23_tx_burst(1, 0, 0, 1);
}

static int
test_balance_l23_tx_burst_toggle_mac_addr(void)
{
	return balance_l23_tx_burst(0, 0, 1, 0);
}

static int
balance_l34_tx_burst(uint8_t vlan_enabled, uint8_t ipv4,
		uint8_t toggle_mac_addr, uint8_t toggle_ip_addr,
		uint8_t toggle_udp_port)
{
	int i, burst_size_1, burst_size_2, nb_tx_1, nb_tx_2;

	struct rte_mbuf *pkts_burst_1[MAX_PKT_BURST];
	struct rte_mbuf *pkts_burst_2[MAX_PKT_BURST];

	struct rte_eth_stats port_stats;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BALANCE, 0, 2, 1),
			"Failed to initialize_bonding_device_with_members.");

	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonding_port_id, BALANCE_XMIT_POLICY_LAYER34),
			"Failed to set balance xmit policy.");

	burst_size_1 = 20;
	burst_size_2 = 10;

	TEST_ASSERT(burst_size_1 < MAX_PKT_BURST || burst_size_2 < MAX_PKT_BURST,
			"Burst size specified is greater than supported.");

	/* Generate test bursts of packets to transmit */
	TEST_ASSERT_EQUAL(generate_test_burst(
			pkts_burst_1, burst_size_1, vlan_enabled, ipv4, 0, 0, 0),
			burst_size_1, "failed to generate burst");

	TEST_ASSERT_EQUAL(generate_test_burst(pkts_burst_2, burst_size_2,
			vlan_enabled, ipv4, toggle_mac_addr, toggle_ip_addr,
			toggle_udp_port), burst_size_2, "failed to generate burst");

	/* Send burst 1 on bonding port */
	nb_tx_1 = rte_eth_tx_burst(test_params->bonding_port_id, 0, pkts_burst_1,
			burst_size_1);
	TEST_ASSERT_EQUAL(nb_tx_1, burst_size_1, "tx burst failed");

	/* Send burst 2 on bonding port */
	nb_tx_2 = rte_eth_tx_burst(test_params->bonding_port_id, 0, pkts_burst_2,
			burst_size_2);
	TEST_ASSERT_EQUAL(nb_tx_2, burst_size_2, "tx burst failed");


	/* Verify bonding port tx stats */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)(nb_tx_1 + nb_tx_2),
			"Bonding Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.opackets,
			nb_tx_1 + nb_tx_2);

	/* Verify member ports tx stats */
	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)nb_tx_1,
			"Member Port (%d) opackets value (%u) not as expected (%d)",
			test_params->member_port_ids[0], (unsigned int)port_stats.opackets,
			nb_tx_1);

	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)nb_tx_2,
			"Member Port (%d) opackets value (%u) not as expected (%d)",
			test_params->member_port_ids[1], (unsigned int)port_stats.opackets,
			nb_tx_2);

	/* Put all members down and try and transmit */
	for (i = 0; i < test_params->bonding_member_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->member_port_ids[i], 0);
	}

	/* Send burst on bonding port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonding_port_id, 0, pkts_burst_1,
			burst_size_1), 0, "Expected zero packet");

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_balance_l34_tx_burst_ipv4_toggle_ip_addr(void)
{
	return balance_l34_tx_burst(0, 1, 0, 1, 0);
}

static int
test_balance_l34_tx_burst_ipv4_toggle_udp_port(void)
{
	return balance_l34_tx_burst(0, 1, 0, 0, 1);
}

static int
test_balance_l34_tx_burst_vlan_ipv4_toggle_ip_addr(void)
{
	return balance_l34_tx_burst(1, 1, 0, 1, 0);
}

static int
test_balance_l34_tx_burst_ipv6_toggle_ip_addr(void)
{
	return balance_l34_tx_burst(0, 0, 0, 1, 0);
}

static int
test_balance_l34_tx_burst_vlan_ipv6_toggle_ip_addr(void)
{
	return balance_l34_tx_burst(1, 0, 0, 1, 0);
}

static int
test_balance_l34_tx_burst_ipv6_toggle_udp_port(void)
{
	return balance_l34_tx_burst(0, 0, 0, 0, 1);
}

#define TEST_BAL_MEMBER_TX_FAIL_MEMBER_COUNT			(2)
#define TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1			(40)
#define TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_2			(20)
#define TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT		(25)
#define TEST_BAL_MEMBER_TX_FAIL_FAILING_MEMBER_IDX	(0)

static int
test_balance_tx_burst_member_tx_fail(void)
{
	struct rte_mbuf *pkts_burst_1[TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1];
	struct rte_mbuf *pkts_burst_2[TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_2];

	struct rte_mbuf *expected_fail_pkts[TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT];

	struct rte_eth_stats port_stats;

	int i, first_tx_fail_idx, tx_count_1, tx_count_2;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BALANCE, 0,
			TEST_BAL_MEMBER_TX_FAIL_MEMBER_COUNT, 1),
			"Failed to initialise bonding device");

	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonding_port_id, BALANCE_XMIT_POLICY_LAYER2),
			"Failed to set balance xmit policy.");


	/* Generate test bursts for transmission */
	TEST_ASSERT_EQUAL(generate_test_burst(pkts_burst_1,
			TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1, 0, 0, 0, 0, 0),
			TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1,
			"Failed to generate test packet burst 1");

	first_tx_fail_idx = TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1 -
			TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT;

	/* copy mbuf references for expected transmission failures */
	for (i = 0; i < TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT; i++)
		expected_fail_pkts[i] = pkts_burst_1[i + first_tx_fail_idx];

	TEST_ASSERT_EQUAL(generate_test_burst(pkts_burst_2,
			TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_2, 0, 0, 1, 0, 0),
			TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_2,
			"Failed to generate test packet burst 2");


	/*
	 * Set virtual member TEST_BAL_MEMBER_TX_FAIL_FAILING_MEMBER_IDX to only fail
	 * transmission of TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT packets of burst.
	 */
	virtual_ethdev_tx_burst_fn_set_success(
			test_params->member_port_ids[TEST_BAL_MEMBER_TX_FAIL_FAILING_MEMBER_IDX],
			0);

	virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(
			test_params->member_port_ids[TEST_BAL_MEMBER_TX_FAIL_FAILING_MEMBER_IDX],
			TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT);


	/* Transmit burst 1 */
	tx_count_1 = rte_eth_tx_burst(test_params->bonding_port_id, 0, pkts_burst_1,
			TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1);

	TEST_ASSERT_EQUAL(tx_count_1, TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1 -
			TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT,
			"Transmitted (%d) packets, expected to transmit (%d) packets",
			tx_count_1, TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1 -
			TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT);

	/* Verify that failed packet are expected failed packets */
	for (i = 0; i < TEST_RR_MEMBER_TX_FAIL_PACKETS_COUNT; i++) {
		TEST_ASSERT_EQUAL(expected_fail_pkts[i], pkts_burst_1[i + tx_count_1],
				"expected mbuf (%d) pointer %p not expected pointer %p",
				i, expected_fail_pkts[i], pkts_burst_1[i + tx_count_1]);
	}

	/* Transmit burst 2 */
	tx_count_2 = rte_eth_tx_burst(test_params->bonding_port_id, 0, pkts_burst_2,
			TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_2);

	TEST_ASSERT_EQUAL(tx_count_2, TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_2,
			"Transmitted (%d) packets, expected to transmit (%d) packets",
			tx_count_2, TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_2);


	/* Verify bonding port tx stats */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)((TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1 -
			TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT) +
			TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_2),
			"Bonding Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.opackets,
			(TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1 -
			TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT) +
			TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_2);

	/* Verify member ports tx stats */

	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)
				TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1 -
				TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT,
				"Member Port (%d) opackets value (%u) not as expected (%d)",
				test_params->member_port_ids[0],
				(unsigned int)port_stats.opackets,
				TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_1 -
				TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT);




	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
				(uint64_t)TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_2,
				"Member Port (%d) opackets value (%u) not as expected (%d)",
				test_params->member_port_ids[1],
				(unsigned int)port_stats.opackets,
				TEST_BAL_MEMBER_TX_FAIL_BURST_SIZE_2);

	/* Verify that all mbufs have a ref value of zero */
	TEST_ASSERT_SUCCESS(verify_mbufs_ref_count(&pkts_burst_1[tx_count_1],
			TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT, 1),
			"mbufs refcnts not as expected");

	free_mbufs(&pkts_burst_1[tx_count_1],
			TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT);

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

#define TEST_BALANCE_RX_BURST_MEMBER_COUNT (3)

static int
test_balance_rx_burst(void)
{
	struct rte_mbuf *gen_pkt_burst[TEST_BALANCE_RX_BURST_MEMBER_COUNT][MAX_PKT_BURST];

	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	int burst_size[TEST_BALANCE_RX_BURST_MEMBER_COUNT] = { 10, 5, 30 };
	int i, j;

	memset(gen_pkt_burst, 0, sizeof(gen_pkt_burst));

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BALANCE, 0, 3, 1),
			"Failed to initialise bonding device");

	/* Generate test bursts of packets to transmit */
	for (i = 0; i < TEST_BALANCE_RX_BURST_MEMBER_COUNT; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[i][0], burst_size[i], 0, 0, 1,
				0, 0), burst_size[i],
				"failed to generate packet burst");
	}

	/* Add rx data to members */
	for (i = 0; i < TEST_BALANCE_RX_BURST_MEMBER_COUNT; i++) {
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[i],
				&gen_pkt_burst[i][0], burst_size[i]);
	}

	/* Call rx burst on bonding device */
	/* Send burst on bonding port */
	TEST_ASSERT_EQUAL(rte_eth_rx_burst(test_params->bonding_port_id, 0,
			rx_pkt_burst, MAX_PKT_BURST),
			burst_size[0] + burst_size[1] + burst_size[2],
			"balance rx burst failed\n");

	/* Verify bonding device rx count */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets,
			(uint64_t)(burst_size[0] + burst_size[1] + burst_size[2]),
			"Bonding Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.ipackets,
			burst_size[0] + burst_size[1] + burst_size[2]);


	/* Verify bonding member devices rx counts */
	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[0],
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
				test_params->member_port_ids[0],
				(unsigned int)port_stats.ipackets, burst_size[0]);

	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[1],
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->member_port_ids[1], (unsigned int)port_stats.ipackets,
			burst_size[1]);

	rte_eth_stats_get(test_params->member_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[2],
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->member_port_ids[2], (unsigned int)port_stats.ipackets,
			burst_size[2]);

	rte_eth_stats_get(test_params->member_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->member_port_ids[3],	(unsigned int)port_stats.ipackets,
			0);

	/* free mbufs */
	for (i = 0; i < TEST_BALANCE_RX_BURST_MEMBER_COUNT; i++) {
		for (j = 0; j < MAX_PKT_BURST; j++) {
			if (gen_pkt_burst[i][j] != NULL) {
				rte_pktmbuf_free(gen_pkt_burst[i][j]);
				gen_pkt_burst[i][j] = NULL;
			}
		}
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_balance_verify_promiscuous_enable_disable(void)
{
	int i;
	int ret;

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BALANCE, 0, 4, 1),
			"Failed to initialise bonding device");

	ret = rte_eth_promiscuous_enable(test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to enable promiscuous mode for port %d: %s",
		test_params->bonding_port_id, rte_strerror(-ret));

	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonding_port_id), 1,
			"Port (%d) promiscuous mode not enabled",
			test_params->bonding_port_id);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(
				test_params->member_port_ids[i]), 1,
				"Port (%d) promiscuous mode not enabled",
				test_params->member_port_ids[i]);
	}

	ret = rte_eth_promiscuous_disable(test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to disable promiscuous mode for port %d: %s",
		test_params->bonding_port_id, rte_strerror(-ret));

	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonding_port_id), 0,
			"Port (%d) promiscuous mode not disabled",
			test_params->bonding_port_id);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(
				test_params->member_port_ids[i]), 0,
				"Port (%d) promiscuous mode not disabled",
				test_params->member_port_ids[i]);
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_balance_verify_mac_assignment(void)
{
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr expected_mac_addr_0, expected_mac_addr_1;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0],
			&expected_mac_addr_0),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1],
			&expected_mac_addr_1),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);

	/* Initialize bonding device with 2 members in active backup mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BALANCE, 0, 2, 1),
			"Failed to initialise bonding device");

	/* Verify that bonding MACs is that of first member and that the other member
	 * MAC hasn't been changed */
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of primary port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[1]);

	/* change primary and verify that MAC addresses haven't changed */
	TEST_ASSERT_SUCCESS(rte_eth_bond_primary_set(test_params->bonding_port_id,
			test_params->member_port_ids[1]),
			"Failed to set bonding port (%d) primary port to (%d)\n",
			test_params->bonding_port_id, test_params->member_port_ids[1]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of primary port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[1]);

	/*
	 * stop / start bonding device and verify that primary MAC address is
	 * propagated to bonding device and members.
	 */

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonding_port_id),
			"Failed to stop bonding port %u",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonding_port_id),
			"Failed to start bonding device");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of primary port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[1]);

	/* Set explicit MAC address */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonding_port_id,
			(struct rte_ether_addr *)bonding_mac),
			"failed to set MAC");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&bonding_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of bonding port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&bonding_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not as expected\n",
				test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&bonding_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of bonding port",
			test_params->member_port_ids[1]);

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

#define TEST_BALANCE_LINK_STATUS_MEMBER_COUNT (4)

static int
test_balance_verify_member_link_status_change_behaviour(void)
{
	struct rte_mbuf *pkt_burst[TEST_BALANCE_LINK_STATUS_MEMBER_COUNT][MAX_PKT_BURST];
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	uint16_t members[RTE_MAX_ETHPORTS];

	int i, burst_size, member_count;

	memset(pkt_burst, 0, sizeof(pkt_burst));

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BALANCE, 0, TEST_BALANCE_LINK_STATUS_MEMBER_COUNT, 1),
			"Failed to initialise bonding device");

	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonding_port_id, BALANCE_XMIT_POLICY_LAYER2),
			"Failed to set balance xmit policy.");


	/* Verify Current Members Count /Active Member Count is */
	member_count = rte_eth_bond_members_get(test_params->bonding_port_id, members,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count, TEST_BALANCE_LINK_STATUS_MEMBER_COUNT,
			"Number of members (%d) is not as expected (%d).",
			member_count, TEST_BALANCE_LINK_STATUS_MEMBER_COUNT);

	member_count = rte_eth_bond_active_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count, TEST_BALANCE_LINK_STATUS_MEMBER_COUNT,
			"Number of active members (%d) is not as expected (%d).",
			member_count, TEST_BALANCE_LINK_STATUS_MEMBER_COUNT);

	/* Set 2 members link status to down */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[3], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_members_get(
			test_params->bonding_port_id, members, RTE_MAX_ETHPORTS), 2,
			"Number of active members (%d) is not as expected (%d).",
			member_count, 2);

	/*
	 * Send to sets of packet burst and verify that they are balanced across
	 *  members.
	 */
	burst_size = 21;

	TEST_ASSERT_EQUAL(generate_test_burst(
			&pkt_burst[0][0], burst_size, 0, 1, 0, 0, 0), burst_size,
			"generate_test_burst failed");

	TEST_ASSERT_EQUAL(generate_test_burst(
			&pkt_burst[1][0], burst_size, 0, 1, 1, 0, 0), burst_size,
			"generate_test_burst failed");

	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonding_port_id, 0, &pkt_burst[0][0], burst_size),
			burst_size, "rte_eth_tx_burst failed");

	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonding_port_id, 0, &pkt_burst[1][0], burst_size),
			burst_size, "rte_eth_tx_burst failed");


	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)(burst_size + burst_size),
			"(%d) port_stats.opackets (%d) not as expected (%d).",
			test_params->bonding_port_id, (int)port_stats.opackets,
			burst_size + burst_size);

	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets (%d) not as expected (%d).",
			test_params->member_port_ids[0], (int)port_stats.opackets,
			burst_size);

	rte_eth_stats_get(test_params->member_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets (%d) not as expected (%d).",
			test_params->member_port_ids[2], (int)port_stats.opackets,
			burst_size);

	/* verify that all packets get send on primary member when no other members
	 * are available */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[2], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_members_get(
			test_params->bonding_port_id, members, RTE_MAX_ETHPORTS), 1,
			"Number of active members (%d) is not as expected (%d).",
			member_count, 1);

	TEST_ASSERT_EQUAL(generate_test_burst(
			&pkt_burst[1][0], burst_size, 0, 1, 1, 0, 0), burst_size,
			"generate_test_burst failed");

	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonding_port_id, 0, &pkt_burst[1][0], burst_size),
			burst_size, "rte_eth_tx_burst failed");

	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)(burst_size + burst_size + burst_size),
			"(%d) port_stats.opackets (%d) not as expected (%d).\n",
			test_params->bonding_port_id, (int)port_stats.opackets,
			burst_size + burst_size + burst_size);

	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)(burst_size + burst_size),
			"(%d) port_stats.opackets (%d) not as expected (%d).",
			test_params->member_port_ids[0], (int)port_stats.opackets,
			burst_size + burst_size);

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[0], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[1], 1);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[2], 1);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[3], 1);

	for (i = 0; i < TEST_BALANCE_LINK_STATUS_MEMBER_COUNT; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&pkt_burst[i][0], burst_size, 0, 1, 0, 0, 0), burst_size,
				"Failed to generate packet burst");

		virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[i],
				&pkt_burst[i][0], burst_size);
	}

	/* Verify that pkts are not received on members with link status down */

	rte_eth_rx_burst(test_params->bonding_port_id, 0, rx_pkt_burst,
			MAX_PKT_BURST);

	/* Verify bonding device rx count */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)(burst_size * 3),
			"(%d) port_stats.ipackets (%d) not as expected (%d)\n",
			test_params->bonding_port_id, (int)port_stats.ipackets,
			burst_size * 3);

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_broadcast_tx_burst(void)
{
	int i, pktlen, burst_size;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	struct rte_eth_stats port_stats;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BROADCAST, 0, 2, 1),
			"Failed to initialise bonding device");

	initialize_eth_header(test_params->pkt_eth_hdr,
			(struct rte_ether_addr *)src_mac,
			(struct rte_ether_addr *)dst_mac_0,
			RTE_ETHER_TYPE_IPV4, 0, 0);

	pktlen = initialize_udp_header(test_params->pkt_udp_hdr, src_port,
			dst_port_0, 16);
	pktlen = initialize_ipv4_header(test_params->pkt_ipv4_hdr, src_addr,
			dst_addr_0, pktlen);

	burst_size = 20 * test_params->bonding_member_count;

	TEST_ASSERT(burst_size < MAX_PKT_BURST,
			"Burst size specified is greater than supported.");

	/* Generate a burst of packets to transmit */
	TEST_ASSERT_EQUAL(generate_packet_burst(test_params->mbuf_pool,
			pkts_burst,	test_params->pkt_eth_hdr, 0, test_params->pkt_ipv4_hdr,
			1, test_params->pkt_udp_hdr, burst_size, PACKET_BURST_GEN_PKT_LEN,
			1), burst_size, "Failed to generate packet burst");

	/* Send burst on bonding port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonding_port_id, 0,
			pkts_burst, burst_size), burst_size,
			"Bonding Port (%d) rx burst failed, packets transmitted value "
			"not as expected (%d)",
			test_params->bonding_port_id, burst_size);

	/* Verify bonding port tx stats */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)burst_size * test_params->bonding_member_count,
			"Bonding Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.opackets,
			burst_size);

	/* Verify member ports tx stats */
	for (i = 0; i < test_params->bonding_member_count; i++) {
		rte_eth_stats_get(test_params->member_port_ids[i], &port_stats);
		TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
				"Member Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->bonding_port_id,
				(unsigned int)port_stats.opackets, burst_size);
	}

	/* Put all members down and try and transmit */
	for (i = 0; i < test_params->bonding_member_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->member_port_ids[i], 0);
	}

	/* Send burst on bonding port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonding_port_id, 0, pkts_burst, burst_size),  0,
			"transmitted an unexpected number of packets");

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}


#define TEST_BCAST_MEMBER_TX_FAIL_MEMBER_COUNT		(3)
#define TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE			(40)
#define TEST_BCAST_MEMBER_TX_FAIL_MAX_PACKETS_COUNT	(15)
#define TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT	(10)

static int
test_broadcast_tx_burst_member_tx_fail(void)
{
	struct rte_mbuf *pkts_burst[TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE];
	struct rte_mbuf *expected_fail_pkts[TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT];

	struct rte_eth_stats port_stats;

	int i, tx_count;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BROADCAST, 0,
			TEST_BCAST_MEMBER_TX_FAIL_MEMBER_COUNT, 1),
			"Failed to initialise bonding device");

	/* Generate test bursts for transmission */
	TEST_ASSERT_EQUAL(generate_test_burst(pkts_burst,
			TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE, 0, 0, 0, 0, 0),
			TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE,
			"Failed to generate test packet burst");

	for (i = 0; i < TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT; i++) {
		expected_fail_pkts[i] = pkts_burst[TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT + i];
	}

	/*
	 * Set virtual member TEST_BAL_MEMBER_TX_FAIL_FAILING_MEMBER_IDX to only fail
	 * transmission of TEST_BAL_MEMBER_TX_FAIL_PACKETS_COUNT packets of burst.
	 */
	virtual_ethdev_tx_burst_fn_set_success(
			test_params->member_port_ids[0],
			0);
	virtual_ethdev_tx_burst_fn_set_success(
			test_params->member_port_ids[1],
			0);
	virtual_ethdev_tx_burst_fn_set_success(
			test_params->member_port_ids[2],
			0);

	virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(
			test_params->member_port_ids[0],
			TEST_BCAST_MEMBER_TX_FAIL_MAX_PACKETS_COUNT);

	virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(
			test_params->member_port_ids[1],
			TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT);

	virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(
			test_params->member_port_ids[2],
			TEST_BCAST_MEMBER_TX_FAIL_MAX_PACKETS_COUNT);

	/* Transmit burst */
	tx_count = rte_eth_tx_burst(test_params->bonding_port_id, 0, pkts_burst,
			TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE);

	TEST_ASSERT_EQUAL(tx_count, TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT,
			"Transmitted (%d) packets, expected to transmit (%d) packets",
			tx_count, TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT);

	/* Verify that failed packet are expected failed packets */
	for (i = 0; i < TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT; i++) {
		TEST_ASSERT_EQUAL(expected_fail_pkts[i], pkts_burst[i + tx_count],
				"expected mbuf (%d) pointer %p not expected pointer %p",
				i, expected_fail_pkts[i], pkts_burst[i + tx_count]);
	}

	/* Verify member ports tx stats */

	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_BCAST_MEMBER_TX_FAIL_MAX_PACKETS_COUNT,
			"Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.opackets,
			TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_BCAST_MEMBER_TX_FAIL_MAX_PACKETS_COUNT);


	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT,
			"Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.opackets,
			TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT);

	rte_eth_stats_get(test_params->member_port_ids[2], &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_BCAST_MEMBER_TX_FAIL_MAX_PACKETS_COUNT,
			"Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.opackets,
			TEST_BCAST_MEMBER_TX_FAIL_BURST_SIZE -
			TEST_BCAST_MEMBER_TX_FAIL_MAX_PACKETS_COUNT);


	/* Verify that all mbufs who transmission failed have a ref value of one */
	TEST_ASSERT_SUCCESS(verify_mbufs_ref_count(&pkts_burst[tx_count],
			TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT, 1),
			"mbufs refcnts not as expected");

	free_mbufs(&pkts_burst[tx_count],
		TEST_BCAST_MEMBER_TX_FAIL_MIN_PACKETS_COUNT);

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

#define BROADCAST_RX_BURST_NUM_OF_MEMBERS (3)

static int
test_broadcast_rx_burst(void)
{
	struct rte_mbuf *gen_pkt_burst[BROADCAST_RX_BURST_NUM_OF_MEMBERS][MAX_PKT_BURST];

	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	int burst_size[BROADCAST_RX_BURST_NUM_OF_MEMBERS] = { 10, 5, 30 };
	int i, j;

	memset(gen_pkt_burst, 0, sizeof(gen_pkt_burst));

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BROADCAST, 0, 3, 1),
			"Failed to initialise bonding device");

	/* Generate test bursts of packets to transmit */
	for (i = 0; i < BROADCAST_RX_BURST_NUM_OF_MEMBERS; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[i][0], burst_size[i], 0, 0, 1, 0, 0),
				burst_size[i], "failed to generate packet burst");
	}

	/* Add rx data to member 0 */
	for (i = 0; i < BROADCAST_RX_BURST_NUM_OF_MEMBERS; i++) {
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[i],
				&gen_pkt_burst[i][0], burst_size[i]);
	}


	/* Call rx burst on bonding device */
	/* Send burst on bonding port */
	TEST_ASSERT_EQUAL(rte_eth_rx_burst(
			test_params->bonding_port_id, 0, rx_pkt_burst, MAX_PKT_BURST),
			burst_size[0] + burst_size[1] + burst_size[2],
			"rx burst failed");

	/* Verify bonding device rx count */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets,
			(uint64_t)(burst_size[0] + burst_size[1] + burst_size[2]),
			"Bonding Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->bonding_port_id, (unsigned int)port_stats.ipackets,
			burst_size[0] + burst_size[1] + burst_size[2]);


	/* Verify bonding member devices rx counts */
	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[0],
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->member_port_ids[0], (unsigned int)port_stats.ipackets,
			burst_size[0]);

	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[1],
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->member_port_ids[0], (unsigned int)port_stats.ipackets,
			burst_size[1]);

	rte_eth_stats_get(test_params->member_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[2],
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->member_port_ids[2], (unsigned int)port_stats.ipackets,
			burst_size[2]);

	rte_eth_stats_get(test_params->member_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
			"Member Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->member_port_ids[3], (unsigned int)port_stats.ipackets,
			0);

	/* free mbufs allocate for rx testing */
	for (i = 0; i < BROADCAST_RX_BURST_NUM_OF_MEMBERS; i++) {
		for (j = 0; j < MAX_PKT_BURST; j++) {
			if (gen_pkt_burst[i][j] != NULL) {
				rte_pktmbuf_free(gen_pkt_burst[i][j]);
				gen_pkt_burst[i][j] = NULL;
			}
		}
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_broadcast_verify_promiscuous_enable_disable(void)
{
	int i;
	int ret;

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BROADCAST, 0, 4, 1),
			"Failed to initialise bonding device");

	ret = rte_eth_promiscuous_enable(test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to enable promiscuous mode for port %d: %s",
		test_params->bonding_port_id, rte_strerror(-ret));


	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonding_port_id), 1,
			"Port (%d) promiscuous mode not enabled",
			test_params->bonding_port_id);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(
				test_params->member_port_ids[i]), 1,
				"Port (%d) promiscuous mode not enabled",
				test_params->member_port_ids[i]);
	}

	ret = rte_eth_promiscuous_disable(test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to disable promiscuous mode for port %d: %s",
		test_params->bonding_port_id, rte_strerror(-ret));

	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonding_port_id), 0,
			"Port (%d) promiscuous mode not disabled",
			test_params->bonding_port_id);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(
				test_params->member_port_ids[i]), 0,
				"Port (%d) promiscuous mode not disabled",
				test_params->member_port_ids[i]);
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_broadcast_verify_mac_assignment(void)
{
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr expected_mac_addr_0, expected_mac_addr_1;

	int i;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0],
			&expected_mac_addr_0),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[2],
			&expected_mac_addr_1),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[2]);

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_BROADCAST, 0, 4, 1),
			"Failed to initialise bonding device");

	/* Verify that all MACs are the same as first member added to bonding
	 * device */
	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[i],
				&read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->member_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr)),
				"member port (%d) mac address not set to that of primary port",
				test_params->member_port_ids[i]);
	}

	/* change primary and verify that MAC addresses haven't changed */
	TEST_ASSERT_SUCCESS(rte_eth_bond_primary_set(test_params->bonding_port_id,
			test_params->member_port_ids[2]),
			"Failed to set bonding port (%d) primary port to (%d)",
			test_params->bonding_port_id, test_params->member_port_ids[i]);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[i],
				&read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->member_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr)),
				"member port (%d) mac address has changed to that of primary "
				"port without stop/start toggle of bonding device",
				test_params->member_port_ids[i]);
	}

	/*
	 * stop / start bonding device and verify that primary MAC address is
	 * propagated to bonding device and members.
	 */

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonding_port_id),
			"Failed to stop bonding port %u",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonding_port_id),
			"Failed to start bonding device");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of new primary  port",
			test_params->member_port_ids[i]);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[i],
				&read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->member_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
				sizeof(read_mac_addr)),
				"member port (%d) mac address not set to that of new primary "
				"port", test_params->member_port_ids[i]);
	}

	/* Set explicit MAC address */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonding_port_id,
			(struct rte_ether_addr *)bonding_mac),
			"Failed to set MAC address");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(bonding_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of new primary port",
			test_params->member_port_ids[i]);


	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[i],
				&read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->member_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(bonding_mac, &read_mac_addr,
				sizeof(read_mac_addr)),
				"member port (%d) mac address not set to that of new primary "
				"port", test_params->member_port_ids[i]);
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

#define BROADCAST_LINK_STATUS_NUM_OF_MEMBERS (4)
static int
test_broadcast_verify_member_link_status_change_behaviour(void)
{
	struct rte_mbuf *pkt_burst[BROADCAST_LINK_STATUS_NUM_OF_MEMBERS][MAX_PKT_BURST];
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	uint16_t members[RTE_MAX_ETHPORTS];

	int i, burst_size, member_count;

	memset(pkt_burst, 0, sizeof(pkt_burst));

	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
				BONDING_MODE_BROADCAST, 0, BROADCAST_LINK_STATUS_NUM_OF_MEMBERS,
				1), "Failed to initialise bonding device");

	/* Verify Current Members Count /Active Member Count is */
	member_count = rte_eth_bond_members_get(test_params->bonding_port_id, members,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count, 4,
			"Number of members (%d) is not as expected (%d).",
			member_count, 4);

	member_count = rte_eth_bond_active_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count, 4,
			"Number of active members (%d) is not as expected (%d).",
			member_count, 4);

	/* Set 2 members link status to down */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[3], 0);

	member_count = rte_eth_bond_active_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count, 2,
			"Number of active members (%d) is not as expected (%d).",
			member_count, 2);

	for (i = 0; i < test_params->bonding_member_count; i++)
		rte_eth_stats_reset(test_params->member_port_ids[i]);

	/* Verify that pkts are not sent on members with link status down */
	burst_size = 21;

	TEST_ASSERT_EQUAL(generate_test_burst(
			&pkt_burst[0][0], burst_size, 0, 0, 1, 0, 0), burst_size,
			"generate_test_burst failed");

	TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonding_port_id, 0,
			&pkt_burst[0][0], burst_size), burst_size,
			"rte_eth_tx_burst failed\n");

	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)(burst_size * member_count),
			"(%d) port_stats.opackets (%d) not as expected (%d)\n",
			test_params->bonding_port_id, (int)port_stats.opackets,
			burst_size * member_count);

	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets not as expected",
			test_params->member_port_ids[0]);

	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected",
				test_params->member_port_ids[1]);

	rte_eth_stats_get(test_params->member_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets not as expected",
				test_params->member_port_ids[2]);


	rte_eth_stats_get(test_params->member_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected",
			test_params->member_port_ids[3]);


	for (i = 0; i < BROADCAST_LINK_STATUS_NUM_OF_MEMBERS; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&pkt_burst[i][0], burst_size, 0, 0, 1, 0, 0),
				burst_size, "failed to generate packet burst");

		virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[i],
				&pkt_burst[i][0], burst_size);
	}

	/* Verify that pkts are not received on members with link status down */
	TEST_ASSERT_EQUAL(rte_eth_rx_burst(
			test_params->bonding_port_id, 0, rx_pkt_burst, MAX_PKT_BURST),
			burst_size + burst_size, "rte_eth_rx_burst failed");


	/* Verify bonding device rx count */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)(burst_size + burst_size),
			"(%d) port_stats.ipackets not as expected\n",
			test_params->bonding_port_id);

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_reconfigure_bonding_device(void)
{
	test_params->nb_rx_q = 4;
	test_params->nb_tx_q = 4;

	TEST_ASSERT_SUCCESS(configure_ethdev(test_params->bonding_port_id, 0, 0),
			"failed to reconfigure bonding device");

	test_params->nb_rx_q = 2;
	test_params->nb_tx_q = 2;

	TEST_ASSERT_SUCCESS(configure_ethdev(test_params->bonding_port_id, 0, 0),
			"failed to reconfigure bonding device with less rx/tx queues");

	return 0;
}


static int
test_close_bonding_device(void)
{
	rte_eth_dev_close(test_params->bonding_port_id);
	return 0;
}

static void
testsuite_teardown(void)
{
	free(test_params->pkt_eth_hdr);
	test_params->pkt_eth_hdr = NULL;

	/* Clean up and remove members from bonding device */
	remove_members_and_stop_bonding_device();
}

static void
free_virtualpmd_tx_queue(void)
{
	int i, member_port, to_free_cnt;
	struct rte_mbuf *pkts_to_free[MAX_PKT_BURST];

	/* Free tx queue of virtual pmd */
	for (member_port = 0; member_port < test_params->bonding_member_count;
			member_port++) {
		to_free_cnt = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->member_port_ids[member_port],
				pkts_to_free, MAX_PKT_BURST);
		for (i = 0; i < to_free_cnt; i++)
			rte_pktmbuf_free(pkts_to_free[i]);
	}
}

static int
test_tlb_tx_burst(void)
{
	int i, burst_size, nb_tx;
	uint64_t nb_tx2 = 0;
	struct rte_mbuf *pkt_burst[MAX_PKT_BURST];
	struct rte_eth_stats port_stats[32];
	uint64_t sum_ports_opackets = 0, all_bond_opackets = 0, all_bond_obytes = 0;
	uint16_t pktlen;

	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members
			(BONDING_MODE_TLB, 1, 3, 1),
			"Failed to initialise bonding device");

	burst_size = 20 * test_params->bonding_member_count;

	TEST_ASSERT(burst_size < MAX_PKT_BURST,
			"Burst size specified is greater than supported.\n");


	/* Generate bursts of packets */
	for (i = 0; i < 400000; i++) {
		/*test two types of mac src own(bonding) and others */
		if (i % 2 == 0) {
			initialize_eth_header(test_params->pkt_eth_hdr,
					(struct rte_ether_addr *)src_mac,
					(struct rte_ether_addr *)dst_mac_0,
					RTE_ETHER_TYPE_IPV4, 0, 0);
		} else {
			initialize_eth_header(test_params->pkt_eth_hdr,
					(struct rte_ether_addr *)test_params->default_member_mac,
					(struct rte_ether_addr *)dst_mac_0,
					RTE_ETHER_TYPE_IPV4, 0, 0);
		}
		pktlen = initialize_udp_header(test_params->pkt_udp_hdr, src_port,
				dst_port_0, 16);
		pktlen = initialize_ipv4_header(test_params->pkt_ipv4_hdr, src_addr,
				dst_addr_0, pktlen);
		generate_packet_burst(test_params->mbuf_pool, pkt_burst,
				test_params->pkt_eth_hdr, 0, test_params->pkt_ipv4_hdr,
				1, test_params->pkt_udp_hdr, burst_size, 60, 1);
		/* Send burst on bonding port */
		nb_tx = rte_eth_tx_burst(test_params->bonding_port_id, 0, pkt_burst,
				burst_size);
		nb_tx2 += nb_tx;

		free_virtualpmd_tx_queue();

		TEST_ASSERT_EQUAL(nb_tx, burst_size,
				"number of packet not equal burst size");

		rte_delay_us(5);
	}


	/* Verify bonding port tx stats */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats[0]);

	all_bond_opackets = port_stats[0].opackets;
	all_bond_obytes = port_stats[0].obytes;

	TEST_ASSERT_EQUAL(port_stats[0].opackets, (uint64_t)nb_tx2,
			"Bonding Port (%d) opackets value (%u) not as expected (%d)\n",
			test_params->bonding_port_id, (unsigned int)port_stats[0].opackets,
			burst_size);


	/* Verify member ports tx stats */
	for (i = 0; i < test_params->bonding_member_count; i++) {
		rte_eth_stats_get(test_params->member_port_ids[i], &port_stats[i]);
		sum_ports_opackets += port_stats[i].opackets;
	}

	TEST_ASSERT_EQUAL(sum_ports_opackets, (uint64_t)all_bond_opackets,
			"Total packets sent by members is not equal to packets sent by bond interface");

	/* checking if distribution of packets is balanced over members */
	for (i = 0; i < test_params->bonding_member_count; i++) {
		TEST_ASSERT(port_stats[i].obytes > 0 &&
				port_stats[i].obytes < all_bond_obytes,
						"Packets are not balanced over members");
	}

	/* Put all members down and try and transmit */
	for (i = 0; i < test_params->bonding_member_count; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				test_params->member_port_ids[i], 0);
	}

	/* Send burst on bonding port */
	nb_tx = rte_eth_tx_burst(test_params->bonding_port_id, 0, pkt_burst,
			burst_size);
	TEST_ASSERT_EQUAL(nb_tx, 0, " bad number of packet in burst");

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

#define TEST_ADAPTIVE_TLB_RX_BURST_MEMBER_COUNT (4)

static int
test_tlb_rx_burst(void)
{
	struct rte_mbuf *gen_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };

	struct rte_eth_stats port_stats;

	int primary_port;

	uint16_t i, j, nb_rx, burst_size = 17;

	/* Initialize bonding device with 4 members in transmit load balancing mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_TLB,
			TEST_ADAPTIVE_TLB_RX_BURST_MEMBER_COUNT, 1, 1),
			"Failed to initialize bonding device");


	primary_port = rte_eth_bond_primary_get(test_params->bonding_port_id);
	TEST_ASSERT(primary_port >= 0,
			"failed to get primary member for bonding port (%d)",
			test_params->bonding_port_id);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		/* Generate test bursts of packets to transmit */
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[0], burst_size, 0, 1, 0, 0, 0), burst_size,
				"burst generation failed");

		/* Add rx data to member */
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[i],
				&gen_pkt_burst[0], burst_size);

		/* Call rx burst on bonding device */
		nb_rx = rte_eth_rx_burst(test_params->bonding_port_id, 0,
				&rx_pkt_burst[0], MAX_PKT_BURST);

		TEST_ASSERT_EQUAL(nb_rx, burst_size, "rte_eth_rx_burst failed\n");

		if (test_params->member_port_ids[i] == primary_port) {
			/* Verify bonding device rx count */
			rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
			TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
					"Bonding Port (%d) ipackets value (%u) not as expected (%d)\n",
					test_params->bonding_port_id,
					(unsigned int)port_stats.ipackets, burst_size);

			/* Verify bonding member devices rx count */
			for (j = 0; j < test_params->bonding_member_count; j++) {
				rte_eth_stats_get(test_params->member_port_ids[j], &port_stats);
				if (i == j) {
					TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
							"Member Port (%d) ipackets value (%u) not as expected (%d)\n",
							test_params->member_port_ids[i],
							(unsigned int)port_stats.ipackets, burst_size);
				} else {
					TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)0,
							"Member Port (%d) ipackets value (%u) not as expected (%d)\n",
							test_params->member_port_ids[i],
							(unsigned int)port_stats.ipackets, 0);
				}
			}
		} else {
			for (j = 0; j < test_params->bonding_member_count; j++) {
				rte_eth_stats_get(test_params->member_port_ids[j], &port_stats);
				TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)0,
						"Member Port (%d) ipackets value (%u) not as expected (%d)\n",
						test_params->member_port_ids[i],
						(unsigned int)port_stats.ipackets, 0);
			}
		}

		/* free mbufs */
		for (i = 0; i < burst_size; i++)
			rte_pktmbuf_free(rx_pkt_burst[i]);

		/* reset bonding device stats */
		rte_eth_stats_reset(test_params->bonding_port_id);
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_tlb_verify_promiscuous_enable_disable(void)
{
	int i, primary_port, promiscuous_en;
	int ret;

	/* Initialize bonding device with 4 members in transmit load balancing mode */
	TEST_ASSERT_SUCCESS( initialize_bonding_device_with_members(
			BONDING_MODE_TLB, 0, 4, 1),
			"Failed to initialize bonding device");

	primary_port = rte_eth_bond_primary_get(test_params->bonding_port_id);
	TEST_ASSERT(primary_port >= 0,
			"failed to get primary member for bonding port (%d)",
			test_params->bonding_port_id);

	ret = rte_eth_promiscuous_enable(test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to enable promiscuous mode for port %d: %s",
		test_params->bonding_port_id, rte_strerror(-ret));

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonding_port_id);
	TEST_ASSERT_EQUAL(promiscuous_en, (int)1,
			"Port (%d) promiscuous mode not enabled\n",
			test_params->bonding_port_id);
	for (i = 0; i < test_params->bonding_member_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->member_port_ids[i]);
		if (primary_port == test_params->member_port_ids[i]) {
			TEST_ASSERT_EQUAL(promiscuous_en, (int)1,
					"Port (%d) promiscuous mode not enabled\n",
					test_params->bonding_port_id);
		} else {
			TEST_ASSERT_EQUAL(promiscuous_en, (int)0,
					"Port (%d) promiscuous mode enabled\n",
					test_params->bonding_port_id);
		}

	}

	ret = rte_eth_promiscuous_disable(test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to disable promiscuous mode for port %d: %s\n",
		test_params->bonding_port_id, rte_strerror(-ret));

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonding_port_id);
	TEST_ASSERT_EQUAL(promiscuous_en, (int)0,
			"Port (%d) promiscuous mode not disabled\n",
			test_params->bonding_port_id);

	for (i = 0; i < test_params->bonding_member_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->member_port_ids[i]);
		TEST_ASSERT_EQUAL(promiscuous_en, (int)0,
				"member port (%d) promiscuous mode not disabled\n",
				test_params->member_port_ids[i]);
	}

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_tlb_verify_mac_assignment(void)
{
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr expected_mac_addr_0, expected_mac_addr_1;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0],
			&expected_mac_addr_0),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1],
			&expected_mac_addr_1),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);

	/* Initialize bonding device with 2 members in active backup mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_TLB, 0, 2, 1),
			"Failed to initialize bonding device");

	/*
	 * Verify that bonding MACs is that of first member and that the other member
	 * MAC hasn't been changed.
	 */
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of primary port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not as expected",
			test_params->member_port_ids[1]);

	/* change primary and verify that MAC addresses haven't changed */
	TEST_ASSERT_EQUAL(rte_eth_bond_primary_set(test_params->bonding_port_id,
			test_params->member_port_ids[1]), 0,
			"Failed to set bonding port (%d) primary port to (%d)",
			test_params->bonding_port_id, test_params->member_port_ids[1]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of primary port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not as expected",
			test_params->member_port_ids[1]);

	/*
	 * stop / start bonding device and verify that primary MAC address is
	 * propagated to bonding device and members.
	 */

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonding_port_id),
			"Failed to stop bonding port %u",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonding_port_id),
			"Failed to start device");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of primary port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not as expected",
			test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of primary port",
			test_params->member_port_ids[1]);


	/* Set explicit MAC address */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonding_port_id,
			(struct rte_ether_addr *)bonding_mac),
			"failed to set MAC address");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonding_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonding_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&bonding_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonding port (%d) mac address not set to that of bonding port",
			test_params->bonding_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not as expected",
			test_params->member_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->member_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->member_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&bonding_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"member port (%d) mac address not set to that of bonding port",
			test_params->member_port_ids[1]);

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

static int
test_tlb_verify_member_link_status_change_failover(void)
{
	struct rte_mbuf *pkt_burst[TEST_ADAPTIVE_TLB_RX_BURST_MEMBER_COUNT][MAX_PKT_BURST];
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	uint16_t members[RTE_MAX_ETHPORTS];

	int i, burst_size, member_count, primary_port;

	burst_size = 21;

	memset(pkt_burst, 0, sizeof(pkt_burst));



	/* Initialize bonding device with 4 members in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonding_device_with_members(
			BONDING_MODE_TLB, 0,
			TEST_ADAPTIVE_TLB_RX_BURST_MEMBER_COUNT, 1),
			"Failed to initialize bonding device with members");

	/* Verify Current Members Count /Active Member Count is */
	member_count = rte_eth_bond_members_get(test_params->bonding_port_id, members,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count, 4,
			"Number of members (%d) is not as expected (%d).\n",
			member_count, 4);

	member_count = rte_eth_bond_active_members_get(test_params->bonding_port_id,
			members, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(member_count, 4,
			"Number of members (%d) is not as expected (%d).\n",
			member_count, 4);

	primary_port = rte_eth_bond_primary_get(test_params->bonding_port_id);
	TEST_ASSERT_EQUAL(primary_port, test_params->member_port_ids[0],
			"Primary port not as expected");

	/* Bring 2 members down and verify active member count */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[3], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_members_get(
			test_params->bonding_port_id, members, RTE_MAX_ETHPORTS), 2,
			"Number of active members (%d) is not as expected (%d).",
			member_count, 2);

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[1], 1);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[3], 1);


	/*
	 * Bring primary port down, verify that active member count is 3 and primary
	 *  has changed.
	 */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->member_port_ids[0], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_members_get(
			test_params->bonding_port_id, members, RTE_MAX_ETHPORTS), 3,
			"Number of active members (%d) is not as expected (%d).",
			member_count, 3);

	primary_port = rte_eth_bond_primary_get(test_params->bonding_port_id);
	TEST_ASSERT_EQUAL(primary_port, test_params->member_port_ids[2],
			"Primary port not as expected");
	rte_delay_us(500000);
	/* Verify that pkts are sent on new primary member */
	for (i = 0; i < 4; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&pkt_burst[0][0], burst_size, 0, 1, 0, 0, 0), burst_size,
				"generate_test_burst failed\n");
		TEST_ASSERT_EQUAL(rte_eth_tx_burst(
				test_params->bonding_port_id, 0, &pkt_burst[0][0], burst_size),
				burst_size,
				"rte_eth_tx_burst failed\n");
		rte_delay_us(11000);
	}

	rte_eth_stats_get(test_params->member_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (int8_t)0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->member_port_ids[0]);

	rte_eth_stats_get(test_params->member_port_ids[1], &port_stats);
	TEST_ASSERT_NOT_EQUAL(port_stats.opackets, (int8_t)0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->member_port_ids[1]);

	rte_eth_stats_get(test_params->member_port_ids[2], &port_stats);
	TEST_ASSERT_NOT_EQUAL(port_stats.opackets, (int8_t)0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->member_port_ids[2]);

	rte_eth_stats_get(test_params->member_port_ids[3], &port_stats);
	TEST_ASSERT_NOT_EQUAL(port_stats.opackets, (int8_t)0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->member_port_ids[3]);


	/* Generate packet burst for testing */

	for (i = 0; i < TEST_ADAPTIVE_TLB_RX_BURST_MEMBER_COUNT; i++) {
		if (generate_test_burst(&pkt_burst[i][0], burst_size, 0, 1, 0, 0, 0) !=
				burst_size)
			return -1;

		virtual_ethdev_add_mbufs_to_rx_queue(
				test_params->member_port_ids[i], &pkt_burst[i][0], burst_size);
	}

	if (rte_eth_rx_burst(test_params->bonding_port_id, 0, rx_pkt_burst,
			MAX_PKT_BURST) != burst_size) {
		printf("rte_eth_rx_burst\n");
		return -1;

	}

	/* Verify bonding device rx count */
	rte_eth_stats_get(test_params->bonding_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
			"(%d) port_stats.ipackets not as expected\n",
			test_params->bonding_port_id);

	/* Clean up and remove members from bonding device */
	return remove_members_and_stop_bonding_device();
}

#define TEST_ALB_MEMBER_COUNT	2

static uint8_t mac_client1[] = {0x00, 0xAA, 0x55, 0xFF, 0xCC, 1};
static uint8_t mac_client2[] = {0x00, 0xAA, 0x55, 0xFF, 0xCC, 2};
static uint8_t mac_client3[] = {0x00, 0xAA, 0x55, 0xFF, 0xCC, 3};
static uint8_t mac_client4[] = {0x00, 0xAA, 0x55, 0xFF, 0xCC, 4};

static uint32_t ip_host = IPV4_ADDR(192, 168, 0, 0);
static uint32_t ip_client1 = IPV4_ADDR(192, 168, 0, 1);
static uint32_t ip_client2 = IPV4_ADDR(192, 168, 0, 2);
static uint32_t ip_client3 = IPV4_ADDR(192, 168, 0, 3);
static uint32_t ip_client4 = IPV4_ADDR(192, 168, 0, 4);

static int
test_alb_change_mac_in_reply_sent(void)
{
	struct rte_mbuf *pkt;
	struct rte_mbuf *pkts_sent[MAX_PKT_BURST];

	struct rte_ether_hdr *eth_pkt;
	struct rte_arp_hdr *arp_pkt;

	int member_idx, nb_pkts, pkt_idx;
	int retval = 0;

	struct rte_ether_addr bond_mac, client_mac;
	struct rte_ether_addr *member_mac1, *member_mac2;

	TEST_ASSERT_SUCCESS(
			initialize_bonding_device_with_members(BONDING_MODE_ALB,
					0, TEST_ALB_MEMBER_COUNT, 1),
			"Failed to initialize_bonding_device_with_members.");

	/* Flush tx queue */
	rte_eth_tx_burst(test_params->bonding_port_id, 0, NULL, 0);
	for (member_idx = 0; member_idx < test_params->bonding_member_count;
			member_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->member_port_ids[member_idx], pkts_sent,
				MAX_PKT_BURST);
	}

	rte_ether_addr_copy(
			rte_eth_devices[test_params->bonding_port_id].data->mac_addrs,
			&bond_mac);

	/*
	 * Generating four packets with different mac and ip addresses and sending
	 * them through the bonding port.
	 */
	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client1, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_ARP, 0, 0);
	arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
					sizeof(struct rte_ether_hdr));
	initialize_arp_header(arp_pkt, &bond_mac, &client_mac, ip_host, ip_client1,
			RTE_ARP_OP_REPLY);
	rte_eth_tx_burst(test_params->bonding_port_id, 0, &pkt, 1);

	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client2, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_ARP, 0, 0);
	arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
					sizeof(struct rte_ether_hdr));
	initialize_arp_header(arp_pkt, &bond_mac, &client_mac, ip_host, ip_client2,
			RTE_ARP_OP_REPLY);
	rte_eth_tx_burst(test_params->bonding_port_id, 0, &pkt, 1);

	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client3, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_ARP, 0, 0);
	arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
					sizeof(struct rte_ether_hdr));
	initialize_arp_header(arp_pkt, &bond_mac, &client_mac, ip_host, ip_client3,
			RTE_ARP_OP_REPLY);
	rte_eth_tx_burst(test_params->bonding_port_id, 0, &pkt, 1);

	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client4, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_ARP, 0, 0);
	arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
					sizeof(struct rte_ether_hdr));
	initialize_arp_header(arp_pkt, &bond_mac, &client_mac, ip_host, ip_client4,
			RTE_ARP_OP_REPLY);
	rte_eth_tx_burst(test_params->bonding_port_id, 0, &pkt, 1);

	member_mac1 =
			rte_eth_devices[test_params->member_port_ids[0]].data->mac_addrs;
	member_mac2 =
			rte_eth_devices[test_params->member_port_ids[1]].data->mac_addrs;

	/*
	 * Checking if packets are properly distributed on bonding ports. Packets
	 * 0 and 2 should be sent on port 0 and packets 1 and 3 on port 1.
	 */
	for (member_idx = 0; member_idx < test_params->bonding_member_count; member_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->member_port_ids[member_idx], pkts_sent,
				MAX_PKT_BURST);

		for (pkt_idx = 0; pkt_idx < nb_pkts; pkt_idx++) {
			eth_pkt = rte_pktmbuf_mtod(
				pkts_sent[pkt_idx], struct rte_ether_hdr *);
			arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
						sizeof(struct rte_ether_hdr));

			if (member_idx%2 == 0) {
				if (!rte_is_same_ether_addr(member_mac1,
						&arp_pkt->arp_data.arp_sha)) {
					retval = -1;
					goto test_end;
				}
			} else {
				if (!rte_is_same_ether_addr(member_mac2,
						&arp_pkt->arp_data.arp_sha)) {
					retval = -1;
					goto test_end;
				}
			}
		}
	}

test_end:
	retval += remove_members_and_stop_bonding_device();
	return retval;
}

static int
test_alb_reply_from_client(void)
{
	struct rte_ether_hdr *eth_pkt;
	struct rte_arp_hdr *arp_pkt;

	struct rte_mbuf *pkt;
	struct rte_mbuf *pkts_sent[MAX_PKT_BURST];

	int member_idx, nb_pkts, pkt_idx, nb_pkts_sum = 0;
	int retval = 0;

	struct rte_ether_addr bond_mac, client_mac;
	struct rte_ether_addr *member_mac1, *member_mac2;

	TEST_ASSERT_SUCCESS(
			initialize_bonding_device_with_members(BONDING_MODE_ALB,
					0, TEST_ALB_MEMBER_COUNT, 1),
			"Failed to initialize_bonding_device_with_members.");

	/* Flush tx queue */
	rte_eth_tx_burst(test_params->bonding_port_id, 0, NULL, 0);
	for (member_idx = 0; member_idx < test_params->bonding_member_count; member_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->member_port_ids[member_idx], pkts_sent,
				MAX_PKT_BURST);
	}

	rte_ether_addr_copy(
			rte_eth_devices[test_params->bonding_port_id].data->mac_addrs,
			&bond_mac);

	/*
	 * Generating four packets with different mac and ip addresses and placing
	 * them in the rx queue to be received by the bonding driver on rx_burst.
	 */
	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client1, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_ARP, 0, 0);
	arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
					sizeof(struct rte_ether_hdr));
	initialize_arp_header(arp_pkt, &client_mac, &bond_mac, ip_client1, ip_host,
			RTE_ARP_OP_REPLY);
	virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[0], &pkt,
			1);

	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client2, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_ARP, 0, 0);
	arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
					sizeof(struct rte_ether_hdr));
	initialize_arp_header(arp_pkt, &client_mac, &bond_mac, ip_client2, ip_host,
			RTE_ARP_OP_REPLY);
	virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[0], &pkt,
			1);

	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client3, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_ARP, 0, 0);
	arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
					sizeof(struct rte_ether_hdr));
	initialize_arp_header(arp_pkt, &client_mac, &bond_mac, ip_client3, ip_host,
			RTE_ARP_OP_REPLY);
	virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[0], &pkt,
			1);

	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client4, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_ARP, 0, 0);
	arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
					sizeof(struct rte_ether_hdr));
	initialize_arp_header(arp_pkt, &client_mac, &bond_mac, ip_client4, ip_host,
			RTE_ARP_OP_REPLY);
	virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[0], &pkt,
			1);

	/*
	 * Issue rx_burst and tx_burst to force bonding driver to send update ARP
	 * packets to every client in alb table.
	 */
	rte_eth_rx_burst(test_params->bonding_port_id, 0, pkts_sent, MAX_PKT_BURST);
	rte_eth_tx_burst(test_params->bonding_port_id, 0, NULL, 0);

	member_mac1 = rte_eth_devices[test_params->member_port_ids[0]].data->mac_addrs;
	member_mac2 = rte_eth_devices[test_params->member_port_ids[1]].data->mac_addrs;

	/*
	 * Checking if update ARP packets were properly send on member ports.
	 */
	for (member_idx = 0; member_idx < test_params->bonding_member_count; member_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->member_port_ids[member_idx], pkts_sent, MAX_PKT_BURST);
		nb_pkts_sum += nb_pkts;

		for (pkt_idx = 0; pkt_idx < nb_pkts; pkt_idx++) {
			eth_pkt = rte_pktmbuf_mtod(
				pkts_sent[pkt_idx], struct rte_ether_hdr *);
			arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
						sizeof(struct rte_ether_hdr));

			if (member_idx%2 == 0) {
				if (!rte_is_same_ether_addr(member_mac1,
						&arp_pkt->arp_data.arp_sha)) {
					retval = -1;
					goto test_end;
				}
			} else {
				if (!rte_is_same_ether_addr(member_mac2,
						&arp_pkt->arp_data.arp_sha)) {
					retval = -1;
					goto test_end;
				}
			}
		}
	}

	/* Check if proper number of packets was send */
	if (nb_pkts_sum < 4) {
		retval = -1;
		goto test_end;
	}

test_end:
	retval += remove_members_and_stop_bonding_device();
	return retval;
}

static int
test_alb_receive_vlan_reply(void)
{
	struct rte_ether_hdr *eth_pkt;
	struct rte_vlan_hdr *vlan_pkt;
	struct rte_arp_hdr *arp_pkt;

	struct rte_mbuf *pkt;
	struct rte_mbuf *pkts_sent[MAX_PKT_BURST];

	int member_idx, nb_pkts, pkt_idx;
	int retval = 0;

	struct rte_ether_addr bond_mac, client_mac;

	TEST_ASSERT_SUCCESS(
			initialize_bonding_device_with_members(BONDING_MODE_ALB,
					0, TEST_ALB_MEMBER_COUNT, 1),
			"Failed to initialize_bonding_device_with_members.");

	/* Flush tx queue */
	rte_eth_tx_burst(test_params->bonding_port_id, 0, NULL, 0);
	for (member_idx = 0; member_idx < test_params->bonding_member_count; member_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->member_port_ids[member_idx], pkts_sent,
				MAX_PKT_BURST);
	}

	rte_ether_addr_copy(
			rte_eth_devices[test_params->bonding_port_id].data->mac_addrs,
			&bond_mac);

	/*
	 * Generating packet with double VLAN header and placing it in the rx queue.
	 */
	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client1, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_VLAN, 0, 0);
	vlan_pkt = (struct rte_vlan_hdr *)((char *)(eth_pkt + 1));
	vlan_pkt->vlan_tci = rte_cpu_to_be_16(1);
	vlan_pkt->eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	vlan_pkt = vlan_pkt+1;
	vlan_pkt->vlan_tci = rte_cpu_to_be_16(2);
	vlan_pkt->eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP);
	arp_pkt = (struct rte_arp_hdr *)((char *)(vlan_pkt + 1));
	initialize_arp_header(arp_pkt, &client_mac, &bond_mac, ip_client1, ip_host,
			RTE_ARP_OP_REPLY);
	virtual_ethdev_add_mbufs_to_rx_queue(test_params->member_port_ids[0], &pkt,
			1);

	rte_eth_rx_burst(test_params->bonding_port_id, 0, pkts_sent, MAX_PKT_BURST);
	rte_eth_tx_burst(test_params->bonding_port_id, 0, NULL, 0);

	/*
	 * Checking if VLAN headers in generated ARP Update packet are correct.
	 */
	for (member_idx = 0; member_idx < test_params->bonding_member_count; member_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->member_port_ids[member_idx], pkts_sent,
				MAX_PKT_BURST);

		for (pkt_idx = 0; pkt_idx < nb_pkts; pkt_idx++) {
			eth_pkt = rte_pktmbuf_mtod(
				pkts_sent[pkt_idx], struct rte_ether_hdr *);
			vlan_pkt = (struct rte_vlan_hdr *)(
				(char *)(eth_pkt + 1));
			if (vlan_pkt->vlan_tci != rte_cpu_to_be_16(1)) {
				retval = -1;
				goto test_end;
			}
			if (vlan_pkt->eth_proto != rte_cpu_to_be_16(
					RTE_ETHER_TYPE_VLAN)) {
				retval = -1;
				goto test_end;
			}
			vlan_pkt = vlan_pkt+1;
			if (vlan_pkt->vlan_tci != rte_cpu_to_be_16(2)) {
				retval = -1;
				goto test_end;
			}
			if (vlan_pkt->eth_proto != rte_cpu_to_be_16(
					RTE_ETHER_TYPE_ARP)) {
				retval = -1;
				goto test_end;
			}
		}
	}

test_end:
	retval += remove_members_and_stop_bonding_device();
	return retval;
}

static int
test_alb_ipv4_tx(void)
{
	int burst_size, retval, pkts_send;
	struct rte_mbuf *pkt_burst[MAX_PKT_BURST];

	retval = 0;

	TEST_ASSERT_SUCCESS(
			initialize_bonding_device_with_members(BONDING_MODE_ALB,
					0, TEST_ALB_MEMBER_COUNT, 1),
			"Failed to initialize_bonding_device_with_members.");

	burst_size = 32;

	/* Generate test bursts of packets to transmit */
	if (generate_test_burst(pkt_burst, burst_size, 0, 1, 0, 0, 0) != burst_size) {
		retval = -1;
		goto test_end;
	}

	/*
	 * Checking if ipv4 traffic is transmitted via TLB policy.
	 */
	pkts_send = rte_eth_tx_burst(
			test_params->bonding_port_id, 0, pkt_burst, burst_size);
	if (pkts_send != burst_size) {
		retval = -1;
		goto test_end;
	}

test_end:
	retval += remove_members_and_stop_bonding_device();
	return retval;
}

static struct unit_test_suite link_bonding_test_suite  = {
	.suite_name = "Link Bonding Unit Test Suite",
	.setup = test_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE(test_create_bonding_device),
		TEST_CASE(test_create_bonding_device_with_invalid_params),
		TEST_CASE(test_add_member_to_bonding_device),
		TEST_CASE(test_add_member_to_invalid_bonding_device),
		TEST_CASE(test_remove_member_from_bonding_device),
		TEST_CASE(test_remove_member_from_invalid_bonding_device),
		TEST_CASE(test_get_members_from_bonding_device),
		TEST_CASE(test_add_already_bonding_member_to_bonding_device),
		TEST_CASE(test_add_remove_multiple_members_to_from_bonding_device),
		TEST_CASE(test_start_bonding_device),
		TEST_CASE(test_stop_bonding_device),
		TEST_CASE(test_set_bonding_mode),
		TEST_CASE(test_set_primary_member),
		TEST_CASE(test_set_explicit_bonding_mac),
		TEST_CASE(test_set_bonding_port_initialization_mac_assignment),
		TEST_CASE(test_status_interrupt),
		TEST_CASE(test_adding_member_after_bonding_device_started),
		TEST_CASE(test_roundrobin_tx_burst),
		TEST_CASE(test_roundrobin_tx_burst_member_tx_fail),
		TEST_CASE(test_roundrobin_rx_burst_on_single_member),
		TEST_CASE(test_roundrobin_rx_burst_on_multiple_members),
		TEST_CASE(test_roundrobin_verify_promiscuous_enable_disable),
		TEST_CASE(test_roundrobin_verify_mac_assignment),
		TEST_CASE(test_roundrobin_verify_member_link_status_change_behaviour),
		TEST_CASE(test_roundrobin_verify_polling_member_link_status_change),
		TEST_CASE(test_activebackup_tx_burst),
		TEST_CASE(test_activebackup_rx_burst),
		TEST_CASE(test_activebackup_verify_promiscuous_enable_disable),
		TEST_CASE(test_activebackup_verify_mac_assignment),
		TEST_CASE(test_activebackup_verify_member_link_status_change_failover),
		TEST_CASE(test_balance_xmit_policy_configuration),
		TEST_CASE(test_balance_l2_tx_burst),
		TEST_CASE(test_balance_l23_tx_burst_ipv4_toggle_ip_addr),
		TEST_CASE(test_balance_l23_tx_burst_vlan_ipv4_toggle_ip_addr),
		TEST_CASE(test_balance_l23_tx_burst_ipv6_toggle_ip_addr),
		TEST_CASE(test_balance_l23_tx_burst_vlan_ipv6_toggle_ip_addr),
		TEST_CASE(test_balance_l23_tx_burst_toggle_mac_addr),
		TEST_CASE(test_balance_l34_tx_burst_ipv4_toggle_ip_addr),
		TEST_CASE(test_balance_l34_tx_burst_ipv4_toggle_udp_port),
		TEST_CASE(test_balance_l34_tx_burst_vlan_ipv4_toggle_ip_addr),
		TEST_CASE(test_balance_l34_tx_burst_ipv6_toggle_ip_addr),
		TEST_CASE(test_balance_l34_tx_burst_vlan_ipv6_toggle_ip_addr),
		TEST_CASE(test_balance_l34_tx_burst_ipv6_toggle_udp_port),
		TEST_CASE(test_balance_tx_burst_member_tx_fail),
		TEST_CASE(test_balance_rx_burst),
		TEST_CASE(test_balance_verify_promiscuous_enable_disable),
		TEST_CASE(test_balance_verify_mac_assignment),
		TEST_CASE(test_balance_verify_member_link_status_change_behaviour),
		TEST_CASE(test_tlb_tx_burst),
		TEST_CASE(test_tlb_rx_burst),
		TEST_CASE(test_tlb_verify_mac_assignment),
		TEST_CASE(test_tlb_verify_promiscuous_enable_disable),
		TEST_CASE(test_tlb_verify_member_link_status_change_failover),
		TEST_CASE(test_alb_change_mac_in_reply_sent),
		TEST_CASE(test_alb_reply_from_client),
		TEST_CASE(test_alb_receive_vlan_reply),
		TEST_CASE(test_alb_ipv4_tx),
		TEST_CASE(test_broadcast_tx_burst),
		TEST_CASE(test_broadcast_tx_burst_member_tx_fail),
		TEST_CASE(test_broadcast_rx_burst),
		TEST_CASE(test_broadcast_verify_promiscuous_enable_disable),
		TEST_CASE(test_broadcast_verify_mac_assignment),
		TEST_CASE(test_broadcast_verify_member_link_status_change_behaviour),
		TEST_CASE(test_reconfigure_bonding_device),
		TEST_CASE(test_close_bonding_device),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};


static int
test_link_bonding(void)
{
	return unit_test_suite_runner(&link_bonding_test_suite);
}

REGISTER_DRIVER_TEST(link_bonding_autotest, test_link_bonding);
