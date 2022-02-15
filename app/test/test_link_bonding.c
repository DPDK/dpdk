/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include "unistd.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
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

#define RTE_TEST_RX_DESC_MAX	(2048)
#define RTE_TEST_TX_DESC_MAX	(2048)
#define MAX_PKT_BURST			(512)
#define DEF_PKT_BURST			(16)

#define BONDED_DEV_NAME			("net_bonding_ut")

#define INVALID_SOCKET_ID		(-1)
#define INVALID_PORT_ID			(-1)
#define INVALID_BONDING_MODE	(-1)


uint8_t slave_mac[] = {0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00 };
uint8_t bonded_mac[] = {0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF };

struct link_bonding_unittest_params {
	int16_t bonded_port_id;
	int16_t slave_port_ids[TEST_MAX_NUMBER_OF_PORTS];
	uint16_t bonded_slave_count;
	uint8_t bonding_mode;

	uint16_t nb_rx_q;
	uint16_t nb_tx_q;

	struct rte_mempool *mbuf_pool;

	struct rte_ether_addr *default_slave_mac;
	struct rte_ether_addr *default_bonded_mac;

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
	.bonded_port_id = -1,
	.slave_port_ids = { -1 },
	.bonded_slave_count = 0,
	.bonding_mode = BONDING_MODE_ROUND_ROBIN,

	.nb_rx_q = 1,
	.nb_tx_q = 1,

	.mbuf_pool = NULL,

	.default_slave_mac = (struct rte_ether_addr *)slave_mac,
	.default_bonded_mac = (struct rte_ether_addr *)bonded_mac,

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
		.split_hdr_size = 0,
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

static int slaves_initialized;
static int mac_slaves_initialized;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cvar = PTHREAD_COND_INITIALIZER;


static int
test_setup(void)
{
	int i, nb_mbuf_per_pool;
	struct rte_ether_addr *mac_addr = (struct rte_ether_addr *)slave_mac;

	/* Allocate ethernet packet header with space for VLAN header */
	if (test_params->pkt_eth_hdr == NULL) {
		test_params->pkt_eth_hdr = malloc(sizeof(struct rte_ether_hdr) +
				sizeof(struct rte_vlan_hdr));

		TEST_ASSERT_NOT_NULL(test_params->pkt_eth_hdr,
				"Ethernet header struct allocation failed!");
	}

	nb_mbuf_per_pool = RTE_TEST_RX_DESC_MAX + DEF_PKT_BURST +
			RTE_TEST_TX_DESC_MAX + MAX_PKT_BURST;
	if (test_params->mbuf_pool == NULL) {
		test_params->mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
			nb_mbuf_per_pool, MBUF_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
		TEST_ASSERT_NOT_NULL(test_params->mbuf_pool,
				"rte_mempool_create failed");
	}

	/* Create / Initialize virtual eth devs */
	if (!slaves_initialized) {
		for (i = 0; i < TEST_MAX_NUMBER_OF_PORTS; i++) {
			char pmd_name[RTE_ETH_NAME_MAX_LEN];

			mac_addr->addr_bytes[RTE_ETHER_ADDR_LEN-1] = i;

			snprintf(pmd_name, RTE_ETH_NAME_MAX_LEN, "eth_virt_%d", i);

			test_params->slave_port_ids[i] = virtual_ethdev_create(pmd_name,
					mac_addr, rte_socket_id(), 1);
			TEST_ASSERT(test_params->slave_port_ids[i] >= 0,
					"Failed to create virtual virtual ethdev %s", pmd_name);

			TEST_ASSERT_SUCCESS(configure_ethdev(
					test_params->slave_port_ids[i], 1, 0),
					"Failed to configure virtual ethdev %s", pmd_name);
		}
		slaves_initialized = 1;
	}

	return 0;
}

static int
test_create_bonded_device(void)
{
	int current_slave_count;

	uint16_t slaves[RTE_MAX_ETHPORTS];

	/* Don't try to recreate bonded device if re-running test suite*/
	if (test_params->bonded_port_id == -1) {
		test_params->bonded_port_id = rte_eth_bond_create(BONDED_DEV_NAME,
				test_params->bonding_mode, rte_socket_id());

		TEST_ASSERT(test_params->bonded_port_id >= 0,
				"Failed to create bonded ethdev %s", BONDED_DEV_NAME);

		TEST_ASSERT_SUCCESS(configure_ethdev(test_params->bonded_port_id, 0, 0),
				"Failed to configure bonded ethdev %s", BONDED_DEV_NAME);
	}

	TEST_ASSERT_SUCCESS(rte_eth_bond_mode_set(test_params->bonded_port_id,
			test_params->bonding_mode), "Failed to set ethdev %d to mode %d",
			test_params->bonded_port_id, test_params->bonding_mode);

	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(current_slave_count, 0,
			"Number of slaves %d is great than expected %d.",
			current_slave_count, 0);

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(current_slave_count, 0,
			"Number of active slaves %d is great than expected %d.",
			current_slave_count, 0);

	return 0;
}


static int
test_create_bonded_device_with_invalid_params(void)
{
	int port_id;

	test_params->bonding_mode = BONDING_MODE_ROUND_ROBIN;

	/* Invalid name */
	port_id = rte_eth_bond_create(NULL, test_params->bonding_mode,
			rte_socket_id());
	TEST_ASSERT(port_id < 0, "Created bonded device unexpectedly");

	test_params->bonding_mode = INVALID_BONDING_MODE;

	/* Invalid bonding mode */
	port_id = rte_eth_bond_create(BONDED_DEV_NAME, test_params->bonding_mode,
			rte_socket_id());
	TEST_ASSERT(port_id < 0, "Created bonded device unexpectedly.");

	test_params->bonding_mode = BONDING_MODE_ROUND_ROBIN;

	/* Invalid socket id */
	port_id = rte_eth_bond_create(BONDED_DEV_NAME, test_params->bonding_mode,
			INVALID_SOCKET_ID);
	TEST_ASSERT(port_id < 0, "Created bonded device unexpectedly.");

	return 0;
}

static int
test_add_slave_to_bonded_device(void)
{
	int current_slave_count;

	uint16_t slaves[RTE_MAX_ETHPORTS];

	TEST_ASSERT_SUCCESS(rte_eth_bond_slave_add(test_params->bonded_port_id,
			test_params->slave_port_ids[test_params->bonded_slave_count]),
			"Failed to add slave (%d) to bonded port (%d).",
			test_params->slave_port_ids[test_params->bonded_slave_count],
			test_params->bonded_port_id);

	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_slave_count, test_params->bonded_slave_count + 1,
			"Number of slaves (%d) is greater than expected (%d).",
			current_slave_count, test_params->bonded_slave_count + 1);

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_slave_count, 0,
					"Number of active slaves (%d) is not as expected (%d).\n",
					current_slave_count, 0);

	test_params->bonded_slave_count++;

	return 0;
}

static int
test_add_slave_to_invalid_bonded_device(void)
{
	/* Invalid port ID */
	TEST_ASSERT_FAIL(rte_eth_bond_slave_add(test_params->bonded_port_id + 5,
			test_params->slave_port_ids[test_params->bonded_slave_count]),
			"Expected call to failed as invalid port specified.");

	/* Non bonded device */
	TEST_ASSERT_FAIL(rte_eth_bond_slave_add(test_params->slave_port_ids[0],
			test_params->slave_port_ids[test_params->bonded_slave_count]),
			"Expected call to failed as invalid port specified.");

	return 0;
}


static int
test_remove_slave_from_bonded_device(void)
{
	int current_slave_count;
	struct rte_ether_addr read_mac_addr, *mac_addr;
	uint16_t slaves[RTE_MAX_ETHPORTS];

	TEST_ASSERT_SUCCESS(rte_eth_bond_slave_remove(test_params->bonded_port_id,
			test_params->slave_port_ids[test_params->bonded_slave_count-1]),
			"Failed to remove slave %d from bonded port (%d).",
			test_params->slave_port_ids[test_params->bonded_slave_count-1],
			test_params->bonded_port_id);


	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(current_slave_count, test_params->bonded_slave_count - 1,
			"Number of slaves (%d) is great than expected (%d).\n",
			current_slave_count, test_params->bonded_slave_count - 1);


	mac_addr = (struct rte_ether_addr *)slave_mac;
	mac_addr->addr_bytes[RTE_ETHER_ADDR_LEN-1] =
			test_params->bonded_slave_count-1;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(
			test_params->slave_port_ids[test_params->bonded_slave_count-1],
			&read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[test_params->bonded_slave_count-1]);
	TEST_ASSERT_SUCCESS(memcmp(mac_addr, &read_mac_addr, sizeof(read_mac_addr)),
			"bonded port mac address not set to that of primary port\n");

	rte_eth_stats_reset(
			test_params->slave_port_ids[test_params->bonded_slave_count-1]);

	virtual_ethdev_simulate_link_status_interrupt(test_params->bonded_port_id,
			0);

	test_params->bonded_slave_count--;

	return 0;
}

static int
test_remove_slave_from_invalid_bonded_device(void)
{
	/* Invalid port ID */
	TEST_ASSERT_FAIL(rte_eth_bond_slave_remove(
			test_params->bonded_port_id + 5,
			test_params->slave_port_ids[test_params->bonded_slave_count - 1]),
			"Expected call to failed as invalid port specified.");

	/* Non bonded device */
	TEST_ASSERT_FAIL(rte_eth_bond_slave_remove(
			test_params->slave_port_ids[0],
			test_params->slave_port_ids[test_params->bonded_slave_count - 1]),
			"Expected call to failed as invalid port specified.");

	return 0;
}

static int bonded_id = 2;

static int
test_add_already_bonded_slave_to_bonded_device(void)
{
	int port_id, current_slave_count;
	uint16_t slaves[RTE_MAX_ETHPORTS];
	char pmd_name[RTE_ETH_NAME_MAX_LEN];

	test_add_slave_to_bonded_device();

	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_slave_count, 1,
			"Number of slaves (%d) is not that expected (%d).",
			current_slave_count, 1);

	snprintf(pmd_name, RTE_ETH_NAME_MAX_LEN, "%s_%d", BONDED_DEV_NAME, ++bonded_id);

	port_id = rte_eth_bond_create(pmd_name, test_params->bonding_mode,
			rte_socket_id());
	TEST_ASSERT(port_id >= 0, "Failed to create bonded device.");

	TEST_ASSERT(rte_eth_bond_slave_add(port_id,
			test_params->slave_port_ids[test_params->bonded_slave_count - 1])
			< 0,
			"Added slave (%d) to bonded port (%d) unexpectedly.",
			test_params->slave_port_ids[test_params->bonded_slave_count-1],
			port_id);

	return test_remove_slave_from_bonded_device();
}


static int
test_get_slaves_from_bonded_device(void)
{
	int current_slave_count;
	uint16_t slaves[RTE_MAX_ETHPORTS];

	TEST_ASSERT_SUCCESS(test_add_slave_to_bonded_device(),
			"Failed to add slave to bonded device");

	/* Invalid port id */
	current_slave_count = rte_eth_bond_slaves_get(INVALID_PORT_ID, slaves,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_slave_count < 0,
			"Invalid port id unexpectedly succeeded");

	current_slave_count = rte_eth_bond_active_slaves_get(INVALID_PORT_ID,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_slave_count < 0,
			"Invalid port id unexpectedly succeeded");

	/* Invalid slaves pointer */
	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			NULL, RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_slave_count < 0,
			"Invalid slave array unexpectedly succeeded");

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, NULL, RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_slave_count < 0,
			"Invalid slave array unexpectedly succeeded");

	/* non bonded device*/
	current_slave_count = rte_eth_bond_slaves_get(
			test_params->slave_port_ids[0], NULL, RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_slave_count < 0,
			"Invalid port id unexpectedly succeeded");

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->slave_port_ids[0],	NULL, RTE_MAX_ETHPORTS);
	TEST_ASSERT(current_slave_count < 0,
			"Invalid port id unexpectedly succeeded");

	TEST_ASSERT_SUCCESS(test_remove_slave_from_bonded_device(),
			"Failed to remove slaves from bonded device");

	return 0;
}


static int
test_add_remove_multiple_slaves_to_from_bonded_device(void)
{
	int i;

	for (i = 0; i < TEST_MAX_NUMBER_OF_PORTS; i++)
		TEST_ASSERT_SUCCESS(test_add_slave_to_bonded_device(),
				"Failed to add slave to bonded device");

	for (i = 0; i < TEST_MAX_NUMBER_OF_PORTS; i++)
		TEST_ASSERT_SUCCESS(test_remove_slave_from_bonded_device(),
				"Failed to remove slaves from bonded device");

	return 0;
}

static void
enable_bonded_slaves(void)
{
	int i;

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		virtual_ethdev_tx_burst_fn_set_success(test_params->slave_port_ids[i],
				1);

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 1);
	}
}

static int
test_start_bonded_device(void)
{
	struct rte_eth_link link_status;

	int current_slave_count, current_bonding_mode, primary_port;
	uint16_t slaves[RTE_MAX_ETHPORTS];
	int retval;

	/* Add slave to bonded device*/
	TEST_ASSERT_SUCCESS(test_add_slave_to_bonded_device(),
			"Failed to add slave to bonded device");

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonded_port_id),
		"Failed to start bonded pmd eth device %d.",
		test_params->bonded_port_id);

	/* Change link status of virtual pmd so it will be added to the active
	 * slave list of the bonded device*/
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[test_params->bonded_slave_count-1], 1);

	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_slave_count, test_params->bonded_slave_count,
			"Number of slaves (%d) is not expected value (%d).",
			current_slave_count, test_params->bonded_slave_count);

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_slave_count, test_params->bonded_slave_count,
			"Number of active slaves (%d) is not expected value (%d).",
			current_slave_count, test_params->bonded_slave_count);

	current_bonding_mode = rte_eth_bond_mode_get(test_params->bonded_port_id);
	TEST_ASSERT_EQUAL(current_bonding_mode, test_params->bonding_mode,
			"Bonded device mode (%d) is not expected value (%d).\n",
			current_bonding_mode, test_params->bonding_mode);

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	TEST_ASSERT_EQUAL(primary_port, test_params->slave_port_ids[0],
			"Primary port (%d) is not expected value (%d).",
			primary_port, test_params->slave_port_ids[0]);

	retval = rte_eth_link_get(test_params->bonded_port_id, &link_status);
	TEST_ASSERT(retval >= 0,
			"Bonded port (%d) link get failed: %s\n",
			test_params->bonded_port_id, rte_strerror(-retval));
	TEST_ASSERT_EQUAL(link_status.link_status, 1,
			"Bonded port (%d) status (%d) is not expected value (%d).\n",
			test_params->bonded_port_id, link_status.link_status, 1);

	return 0;
}

static int
test_stop_bonded_device(void)
{
	int current_slave_count;
	uint16_t slaves[RTE_MAX_ETHPORTS];

	struct rte_eth_link link_status;
	int retval;

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonded_port_id),
			"Failed to stop bonded port %u",
			test_params->bonded_port_id);

	retval = rte_eth_link_get(test_params->bonded_port_id, &link_status);
	TEST_ASSERT(retval >= 0,
			"Bonded port (%d) link get failed: %s\n",
			test_params->bonded_port_id, rte_strerror(-retval));
	TEST_ASSERT_EQUAL(link_status.link_status, 0,
			"Bonded port (%d) status (%d) is not expected value (%d).",
			test_params->bonded_port_id, link_status.link_status, 0);

	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_slave_count, test_params->bonded_slave_count,
			"Number of slaves (%d) is not expected value (%d).",
			current_slave_count, test_params->bonded_slave_count);

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(current_slave_count, 0,
			"Number of active slaves (%d) is not expected value (%d).",
			current_slave_count, 0);

	return 0;
}

static int
remove_slaves_and_stop_bonded_device(void)
{
	/* Clean up and remove slaves from bonded device */
	free_virtualpmd_tx_queue();
	while (test_params->bonded_slave_count > 0)
		TEST_ASSERT_SUCCESS(test_remove_slave_from_bonded_device(),
				"test_remove_slave_from_bonded_device failed");

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonded_port_id),
			"Failed to stop bonded port %u",
			test_params->bonded_port_id);

	rte_eth_stats_reset(test_params->bonded_port_id);
	rte_eth_bond_mac_address_reset(test_params->bonded_port_id);

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

		/* Non bonded device */
		TEST_ASSERT_FAIL(rte_eth_bond_mode_set(test_params->slave_port_ids[0],
				bonding_modes[i]),
				"Expected call to failed as invalid port (%d) specified.",
				test_params->slave_port_ids[0]);

		TEST_ASSERT_SUCCESS(rte_eth_bond_mode_set(test_params->bonded_port_id,
				bonding_modes[i]),
				"Failed to set link bonding mode on port (%d) to (%d).",
				test_params->bonded_port_id, bonding_modes[i]);

		bonding_mode = rte_eth_bond_mode_get(test_params->bonded_port_id);
		TEST_ASSERT_EQUAL(bonding_mode, bonding_modes[i],
				"Link bonding mode (%d) of port (%d) is not expected value (%d).",
				bonding_mode, test_params->bonded_port_id,
				bonding_modes[i]);

		/* Invalid port ID */
		bonding_mode = rte_eth_bond_mode_get(INVALID_PORT_ID);
		TEST_ASSERT(bonding_mode < 0,
				"Expected call to failed as invalid port (%d) specified.",
				INVALID_PORT_ID);

		/* Non bonded device */
		bonding_mode = rte_eth_bond_mode_get(test_params->slave_port_ids[0]);
		TEST_ASSERT(bonding_mode < 0,
				"Expected call to failed as invalid port (%d) specified.",
				test_params->slave_port_ids[0]);
	}

	return remove_slaves_and_stop_bonded_device();
}

static int
test_set_primary_slave(void)
{
	int i, j, retval;
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr *expected_mac_addr;

	/* Add 4 slaves to bonded device */
	for (i = test_params->bonded_slave_count; i < 4; i++)
		TEST_ASSERT_SUCCESS(test_add_slave_to_bonded_device(),
				"Failed to add slave to bonded device.");

	TEST_ASSERT_SUCCESS(rte_eth_bond_mode_set(test_params->bonded_port_id,
			BONDING_MODE_ROUND_ROBIN),
			"Failed to set link bonding mode on port (%d) to (%d).",
			test_params->bonded_port_id, BONDING_MODE_ROUND_ROBIN);

	/* Invalid port ID */
	TEST_ASSERT_FAIL(rte_eth_bond_primary_set(INVALID_PORT_ID,
			test_params->slave_port_ids[i]),
			"Expected call to failed as invalid port specified.");

	/* Non bonded device */
	TEST_ASSERT_FAIL(rte_eth_bond_primary_set(test_params->slave_port_ids[i],
			test_params->slave_port_ids[i]),
			"Expected call to failed as invalid port specified.");

	/* Set slave as primary
	 * Verify slave it is now primary slave
	 * Verify that MAC address of bonded device is that of primary slave
	 * Verify that MAC address of all bonded slaves are that of primary slave
	 */
	for (i = 0; i < 4; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_bond_primary_set(test_params->bonded_port_id,
				test_params->slave_port_ids[i]),
				"Failed to set bonded port (%d) primary port to (%d)",
				test_params->bonded_port_id, test_params->slave_port_ids[i]);

		retval = rte_eth_bond_primary_get(test_params->bonded_port_id);
		TEST_ASSERT(retval >= 0,
				"Failed to read primary port from bonded port (%d)\n",
					test_params->bonded_port_id);

		TEST_ASSERT_EQUAL(retval, test_params->slave_port_ids[i],
				"Bonded port (%d) primary port (%d) not expected value (%d)\n",
				test_params->bonded_port_id, retval,
				test_params->slave_port_ids[i]);

		/* stop/start bonded eth dev to apply new MAC */
		TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonded_port_id),
				"Failed to stop bonded port %u",
				test_params->bonded_port_id);

		TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonded_port_id),
				"Failed to start bonded port %d",
				test_params->bonded_port_id);

		expected_mac_addr = (struct rte_ether_addr *)&slave_mac;
		expected_mac_addr->addr_bytes[RTE_ETHER_ADDR_LEN-1] = i;

		/* Check primary slave MAC */
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->slave_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(expected_mac_addr, &read_mac_addr,
				sizeof(read_mac_addr)),
				"bonded port mac address not set to that of primary port\n");

		/* Check bonded MAC */
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->bonded_port_id);
		TEST_ASSERT_SUCCESS(memcmp(&read_mac_addr, &read_mac_addr,
				sizeof(read_mac_addr)),
				"bonded port mac address not set to that of primary port\n");

		/* Check other slaves MACs */
		for (j = 0; j < 4; j++) {
			if (j != i) {
				TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[j],
						&read_mac_addr),
						"Failed to get mac address (port %d)",
						test_params->slave_port_ids[j]);
				TEST_ASSERT_SUCCESS(memcmp(expected_mac_addr, &read_mac_addr,
						sizeof(read_mac_addr)),
						"slave port mac address not set to that of primary "
						"port");
			}
		}
	}


	/* Test with none existent port */
	TEST_ASSERT_FAIL(rte_eth_bond_primary_get(test_params->bonded_port_id + 10),
			"read primary port from expectedly");

	/* Test with slave port */
	TEST_ASSERT_FAIL(rte_eth_bond_primary_get(test_params->slave_port_ids[0]),
			"read primary port from expectedly\n");

	TEST_ASSERT_SUCCESS(remove_slaves_and_stop_bonded_device(),
			"Failed to stop and remove slaves from bonded device");

	/* No slaves  */
	TEST_ASSERT(rte_eth_bond_primary_get(test_params->bonded_port_id)  < 0,
			"read primary port from expectedly\n");

	return 0;
}

static int
test_set_explicit_bonded_mac(void)
{
	int i;
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr *mac_addr;

	uint8_t explicit_bonded_mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };

	mac_addr = (struct rte_ether_addr *)explicit_bonded_mac;

	/* Invalid port ID */
	TEST_ASSERT_FAIL(rte_eth_bond_mac_address_set(INVALID_PORT_ID, mac_addr),
			"Expected call to failed as invalid port specified.");

	/* Non bonded device */
	TEST_ASSERT_FAIL(rte_eth_bond_mac_address_set(
			test_params->slave_port_ids[0],	mac_addr),
			"Expected call to failed as invalid port specified.");

	/* NULL MAC address */
	TEST_ASSERT_FAIL(rte_eth_bond_mac_address_set(
			test_params->bonded_port_id, NULL),
			"Expected call to failed as NULL MAC specified");

	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonded_port_id, mac_addr),
			"Failed to set MAC address on bonded port (%d)",
			test_params->bonded_port_id);

	/* Add 4 slaves to bonded device */
	for (i = test_params->bonded_slave_count; i < 4; i++) {
		TEST_ASSERT_SUCCESS(test_add_slave_to_bonded_device(),
				"Failed to add slave to bonded device.\n");
	}

	/* Check bonded MAC */
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(mac_addr, &read_mac_addr, sizeof(read_mac_addr)),
			"bonded port mac address not set to that of primary port");

	/* Check other slaves MACs */
	for (i = 0; i < 4; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->slave_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(mac_addr, &read_mac_addr,
				sizeof(read_mac_addr)),
				"slave port mac address not set to that of primary port");
	}

	/* test resetting mac address on bonded device */
	TEST_ASSERT_SUCCESS(
			rte_eth_bond_mac_address_reset(test_params->bonded_port_id),
			"Failed to reset MAC address on bonded port (%d)",
			test_params->bonded_port_id);

	TEST_ASSERT_FAIL(
			rte_eth_bond_mac_address_reset(test_params->slave_port_ids[0]),
			"Reset MAC address on bonded port (%d) unexpectedly",
			test_params->slave_port_ids[1]);

	/* test resetting mac address on bonded device with no slaves */
	TEST_ASSERT_SUCCESS(remove_slaves_and_stop_bonded_device(),
			"Failed to remove slaves and stop bonded device");

	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_reset(test_params->bonded_port_id),
			"Failed to reset MAC address on bonded port (%d)",
				test_params->bonded_port_id);

	return 0;
}

#define BONDED_INIT_MAC_ASSIGNMENT_SLAVE_COUNT (3)

static int
test_set_bonded_port_initialization_mac_assignment(void)
{
	int i, slave_count;

	uint16_t slaves[RTE_MAX_ETHPORTS];
	static int bonded_port_id = -1;
	static int slave_port_ids[BONDED_INIT_MAC_ASSIGNMENT_SLAVE_COUNT];

	struct rte_ether_addr slave_mac_addr, bonded_mac_addr, read_mac_addr;

	/* Initialize default values for MAC addresses */
	memcpy(&slave_mac_addr, slave_mac, sizeof(struct rte_ether_addr));
	memcpy(&bonded_mac_addr, slave_mac, sizeof(struct rte_ether_addr));

	/*
	 * 1. a - Create / configure  bonded / slave ethdevs
	 */
	if (bonded_port_id == -1) {
		bonded_port_id = rte_eth_bond_create("net_bonding_mac_ass_test",
				BONDING_MODE_ACTIVE_BACKUP, rte_socket_id());
		TEST_ASSERT(bonded_port_id > 0, "failed to create bonded device");

		TEST_ASSERT_SUCCESS(configure_ethdev(bonded_port_id, 0, 0),
					"Failed to configure bonded ethdev");
	}

	if (!mac_slaves_initialized) {
		for (i = 0; i < BONDED_INIT_MAC_ASSIGNMENT_SLAVE_COUNT; i++) {
			char pmd_name[RTE_ETH_NAME_MAX_LEN];

			slave_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN - 1] =
				i + 100;

			snprintf(pmd_name, RTE_ETH_NAME_MAX_LEN,
				"eth_slave_%d", i);

			slave_port_ids[i] = virtual_ethdev_create(pmd_name,
					&slave_mac_addr, rte_socket_id(), 1);

			TEST_ASSERT(slave_port_ids[i] >= 0,
					"Failed to create slave ethdev %s",
					pmd_name);

			TEST_ASSERT_SUCCESS(configure_ethdev(slave_port_ids[i], 1, 0),
					"Failed to configure virtual ethdev %s",
					pmd_name);
		}
		mac_slaves_initialized = 1;
	}


	/*
	 * 2. Add slave ethdevs to bonded device
	 */
	for (i = 0; i < BONDED_INIT_MAC_ASSIGNMENT_SLAVE_COUNT; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_bond_slave_add(bonded_port_id,
				slave_port_ids[i]),
				"Failed to add slave (%d) to bonded port (%d).",
				slave_port_ids[i], bonded_port_id);
	}

	slave_count = rte_eth_bond_slaves_get(bonded_port_id, slaves,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(BONDED_INIT_MAC_ASSIGNMENT_SLAVE_COUNT, slave_count,
			"Number of slaves (%d) is not as expected (%d)",
			slave_count, BONDED_INIT_MAC_ASSIGNMENT_SLAVE_COUNT);


	/*
	 * 3. Set explicit MAC address on bonded ethdev
	 */
	bonded_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-2] = 0xFF;
	bonded_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 0xAA;

	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			bonded_port_id, &bonded_mac_addr),
			"Failed to set MAC address on bonded port (%d)",
			bonded_port_id);


	/* 4. a - Start bonded ethdev
	 *    b - Enable slave devices
	 *    c - Verify bonded/slaves ethdev MAC addresses
	 */
	TEST_ASSERT_SUCCESS(rte_eth_dev_start(bonded_port_id),
			"Failed to start bonded pmd eth device %d.",
			bonded_port_id);

	for (i = 0; i < BONDED_INIT_MAC_ASSIGNMENT_SLAVE_COUNT; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				slave_port_ids[i], 1);
	}

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&bonded_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port mac address not as expected");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&bonded_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port 0 mac address not as expected");

	slave_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 1 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&slave_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port 1 mac address not as expected");

	slave_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 2 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(slave_port_ids[2], &read_mac_addr),
			"Failed to get mac address (port %d)",
			slave_port_ids[2]);
	TEST_ASSERT_SUCCESS(memcmp(&slave_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port 2 mac address not as expected");


	/* 7. a - Change primary port
	 *    b - Stop / Start bonded port
	 *    d - Verify slave ethdev MAC addresses
	 */
	TEST_ASSERT_SUCCESS(rte_eth_bond_primary_set(bonded_port_id,
			slave_port_ids[2]),
			"failed to set primary port on bonded device.");

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(bonded_port_id),
			"Failed to stop bonded port %u",
			bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(bonded_port_id),
				"Failed to start bonded pmd eth device %d.",
				bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&bonded_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port mac address not as expected");

	slave_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 0 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&slave_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port 0 mac address not as expected");

	slave_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 1 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&slave_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port 1 mac address not as expected");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(slave_port_ids[2], &read_mac_addr),
			"Failed to get mac address (port %d)",
			slave_port_ids[2]);
	TEST_ASSERT_SUCCESS(memcmp(&bonded_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port 2 mac address not as expected");

	/* 6. a - Stop bonded ethdev
	 *    b - remove slave ethdevs
	 *    c - Verify slave ethdevs MACs are restored
	 */
	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(bonded_port_id),
			"Failed to stop bonded port %u",
			bonded_port_id);

	for (i = 0; i < BONDED_INIT_MAC_ASSIGNMENT_SLAVE_COUNT; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_bond_slave_remove(bonded_port_id,
				slave_port_ids[i]),
				"Failed to remove slave %d from bonded port (%d).",
				slave_port_ids[i], bonded_port_id);
	}

	slave_count = rte_eth_bond_slaves_get(bonded_port_id, slaves,
			RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(slave_count, 0,
			"Number of slaves (%d) is great than expected (%d).",
			slave_count, 0);

	slave_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 0 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&slave_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port 0 mac address not as expected");

	slave_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 1 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&slave_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port 1 mac address not as expected");

	slave_mac_addr.addr_bytes[RTE_ETHER_ADDR_LEN-1] = 2 + 100;
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(slave_port_ids[2], &read_mac_addr),
			"Failed to get mac address (port %d)",
			slave_port_ids[2]);
	TEST_ASSERT_SUCCESS(memcmp(&slave_mac_addr, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port 2 mac address not as expected");

	return 0;
}


static int
initialize_bonded_device_with_slaves(uint8_t bonding_mode, uint8_t bond_en_isr,
		uint16_t number_of_slaves, uint8_t enable_slave)
{
	/* Configure bonded device */
	TEST_ASSERT_SUCCESS(configure_ethdev(test_params->bonded_port_id, 0,
			bond_en_isr), "Failed to configure bonding port (%d) in mode %d "
			"with (%d) slaves.", test_params->bonded_port_id, bonding_mode,
			number_of_slaves);

	/* Add slaves to bonded device */
	while (number_of_slaves > test_params->bonded_slave_count)
		TEST_ASSERT_SUCCESS(test_add_slave_to_bonded_device(),
				"Failed to add slave (%d to  bonding port (%d).",
				test_params->bonded_slave_count - 1,
				test_params->bonded_port_id);

	/* Set link bonding mode  */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mode_set(test_params->bonded_port_id,
			bonding_mode),
			"Failed to set link bonding mode on port (%d) to (%d).",
			test_params->bonded_port_id, bonding_mode);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonded_port_id),
		"Failed to start bonded pmd eth device %d.",
		test_params->bonded_port_id);

	if (enable_slave)
		enable_bonded_slaves();

	return 0;
}

static int
test_adding_slave_after_bonded_device_started(void)
{
	int i;

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ROUND_ROBIN, 0, 4, 0),
			"Failed to add slaves to bonded device");

	/* Enabled slave devices */
	for (i = 0; i < test_params->bonded_slave_count + 1; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 1);
	}

	TEST_ASSERT_SUCCESS(rte_eth_bond_slave_add(test_params->bonded_port_id,
			test_params->slave_port_ids[test_params->bonded_slave_count]),
			"Failed to add slave to bonded port.\n");

	rte_eth_stats_reset(
			test_params->slave_port_ids[test_params->bonded_slave_count]);

	test_params->bonded_slave_count++;

	return remove_slaves_and_stop_bonded_device();
}

#define TEST_STATUS_INTERRUPT_SLAVE_COUNT	4
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
	int slave_count;
	uint16_t slaves[RTE_MAX_ETHPORTS];

	/* initialized bonding device with T slaves */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ROUND_ROBIN, 1,
			TEST_STATUS_INTERRUPT_SLAVE_COUNT, 1),
			"Failed to initialise bonded device");

	test_lsc_interrupt_count = 0;

	/* register link status change interrupt callback */
	rte_eth_dev_callback_register(test_params->bonded_port_id,
			RTE_ETH_EVENT_INTR_LSC, test_bonding_lsc_event_callback,
			&test_params->bonded_port_id);

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(slave_count, TEST_STATUS_INTERRUPT_SLAVE_COUNT,
			"Number of active slaves (%d) is not as expected (%d)",
			slave_count, TEST_STATUS_INTERRUPT_SLAVE_COUNT);

	/* Bring all 4 slaves link status to down and test that we have received a
	 * lsc interrupts */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[0], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[2], 0);

	TEST_ASSERT_EQUAL(test_lsc_interrupt_count, 0,
			"Received a link status change interrupt unexpectedly");

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 0);

	TEST_ASSERT(lsc_timeout(TEST_LSC_WAIT_TIMEOUT_US) == 0,
			"timed out waiting for interrupt");

	TEST_ASSERT(test_lsc_interrupt_count > 0,
			"Did not receive link status change interrupt");

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);

	TEST_ASSERT_EQUAL(slave_count, 0,
			"Number of active slaves (%d) is not as expected (%d)",
			slave_count, 0);

	/* bring one slave port up so link status will change */
	test_lsc_interrupt_count = 0;

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[0], 1);

	TEST_ASSERT(lsc_timeout(TEST_LSC_WAIT_TIMEOUT_US) == 0,
			"timed out waiting for interrupt");

	/* test that we have received another lsc interrupt */
	TEST_ASSERT(test_lsc_interrupt_count > 0,
			"Did not receive link status change interrupt");

	/* Verify that calling the same slave lsc interrupt doesn't cause another
	 * lsc interrupt from bonded device */
	test_lsc_interrupt_count = 0;

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[0], 1);

	TEST_ASSERT(lsc_timeout(TEST_LSC_WAIT_TIMEOUT_US) != 0,
			"received unexpected interrupt");

	TEST_ASSERT_EQUAL(test_lsc_interrupt_count, 0,
			"Did not receive link status change interrupt");


	/* unregister lsc callback before exiting */
	rte_eth_dev_callback_unregister(test_params->bonded_port_id,
				RTE_ETH_EVENT_INTR_LSC, test_bonding_lsc_event_callback,
				&test_params->bonded_port_id);

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
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

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ROUND_ROBIN, 0, 2, 1),
			"Failed to initialise bonded device");

	burst_size = 20 * test_params->bonded_slave_count;

	TEST_ASSERT(burst_size <= MAX_PKT_BURST,
			"Burst size specified is greater than supported.");

	/* Generate test bursts of packets to transmit */
	TEST_ASSERT_EQUAL(generate_test_burst(pkt_burst, burst_size, 0, 1, 0, 0, 0),
			burst_size, "failed to generate test burst");

	/* Send burst on bonded port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonded_port_id, 0, pkt_burst, burst_size), burst_size,
			"tx burst failed");

	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"Bonded Port (%d) opackets value (%u) not as expected (%d)\n",
			test_params->bonded_port_id, (unsigned int)port_stats.opackets,
			burst_size);

	/* Verify slave ports tx stats */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_stats_get(test_params->slave_port_ids[i], &port_stats);
		TEST_ASSERT_EQUAL(port_stats.opackets,
				(uint64_t)burst_size / test_params->bonded_slave_count,
				"Slave Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->bonded_port_id, (unsigned int)port_stats.opackets,
				burst_size / test_params->bonded_slave_count);
	}

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonded_port_id, 0,
			pkt_burst, burst_size), 0,
			"tx burst return unexpected value");

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
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

#define TEST_RR_SLAVE_TX_FAIL_SLAVE_COUNT		(2)
#define TEST_RR_SLAVE_TX_FAIL_BURST_SIZE		(64)
#define TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT		(22)
#define TEST_RR_SLAVE_TX_FAIL_FAILING_SLAVE_IDX	(1)

static int
test_roundrobin_tx_burst_slave_tx_fail(void)
{
	struct rte_mbuf *pkt_burst[MAX_PKT_BURST];
	struct rte_mbuf *expected_tx_fail_pkts[MAX_PKT_BURST];

	struct rte_eth_stats port_stats;

	int i, first_fail_idx, tx_count;

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ROUND_ROBIN, 0,
			TEST_RR_SLAVE_TX_FAIL_SLAVE_COUNT, 1),
			"Failed to initialise bonded device");

	/* Generate test bursts of packets to transmit */
	TEST_ASSERT_EQUAL(generate_test_burst(pkt_burst,
			TEST_RR_SLAVE_TX_FAIL_BURST_SIZE, 0, 1, 0, 0, 0),
			TEST_RR_SLAVE_TX_FAIL_BURST_SIZE,
			"Failed to generate test packet burst");

	/* Copy references to packets which we expect not to be transmitted */
	first_fail_idx = (TEST_RR_SLAVE_TX_FAIL_BURST_SIZE -
			(TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT *
			TEST_RR_SLAVE_TX_FAIL_SLAVE_COUNT)) +
			TEST_RR_SLAVE_TX_FAIL_FAILING_SLAVE_IDX;

	for (i = 0; i < TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT; i++) {
		expected_tx_fail_pkts[i] = pkt_burst[first_fail_idx +
				(i * TEST_RR_SLAVE_TX_FAIL_SLAVE_COUNT)];
	}

	/* Set virtual slave to only fail transmission of
	 * TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT packets in burst */
	virtual_ethdev_tx_burst_fn_set_success(
			test_params->slave_port_ids[TEST_RR_SLAVE_TX_FAIL_FAILING_SLAVE_IDX],
			0);

	virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(
			test_params->slave_port_ids[TEST_RR_SLAVE_TX_FAIL_FAILING_SLAVE_IDX],
			TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT);

	tx_count = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkt_burst,
			TEST_RR_SLAVE_TX_FAIL_BURST_SIZE);

	TEST_ASSERT_EQUAL(tx_count, TEST_RR_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT,
			"Transmitted (%d) an unexpected (%d) number of packets", tx_count,
			TEST_RR_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT);

	/* Verify that failed packet are expected failed packets */
	for (i = 0; i < TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT; i++) {
		TEST_ASSERT_EQUAL(expected_tx_fail_pkts[i], pkt_burst[i + tx_count],
				"expected mbuf (%d) pointer %p not expected pointer %p",
				i, expected_tx_fail_pkts[i], pkt_burst[i + tx_count]);
	}

	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)TEST_RR_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT,
			"Bonded Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.opackets,
			TEST_RR_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT);

	/* Verify slave ports tx stats */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		int slave_expected_tx_count;

		rte_eth_stats_get(test_params->slave_port_ids[i], &port_stats);

		slave_expected_tx_count = TEST_RR_SLAVE_TX_FAIL_BURST_SIZE /
				test_params->bonded_slave_count;

		if (i == TEST_RR_SLAVE_TX_FAIL_FAILING_SLAVE_IDX)
			slave_expected_tx_count = slave_expected_tx_count -
					TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT;

		TEST_ASSERT_EQUAL(port_stats.opackets,
				(uint64_t)slave_expected_tx_count,
				"Slave Port (%d) opackets value (%u) not as expected (%d)",
				test_params->slave_port_ids[i],
				(unsigned int)port_stats.opackets, slave_expected_tx_count);
	}

	/* Verify that all mbufs have a ref value of zero */
	TEST_ASSERT_SUCCESS(verify_mbufs_ref_count(&pkt_burst[tx_count],
			TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT, 1),
			"mbufs refcnts not as expected");
	free_mbufs(&pkt_burst[tx_count], TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT);

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_roundrobin_rx_burst_on_single_slave(void)
{
	struct rte_mbuf *gen_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };

	struct rte_eth_stats port_stats;

	int i, j, burst_size = 25;

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ROUND_ROBIN, 0, 4, 1),
			"Failed to initialize bonded device with slaves");

	/* Generate test bursts of packets to transmit */
	TEST_ASSERT_EQUAL(generate_test_burst(
			gen_pkt_burst, burst_size, 0, 1, 0, 0, 0), burst_size,
			"burst generation failed");

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		/* Add rx data to slave */
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[0], burst_size);

		/* Call rx burst on bonded device */
		/* Send burst on bonded port */
		TEST_ASSERT_EQUAL(rte_eth_rx_burst(
				test_params->bonded_port_id, 0, rx_pkt_burst,
				MAX_PKT_BURST), burst_size,
				"round-robin rx burst failed");

		/* Verify bonded device rx count */
		rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
		TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
				"Bonded Port (%d) ipackets value (%u) not as expected (%d)",
				test_params->bonded_port_id,
				(unsigned int)port_stats.ipackets, burst_size);



		/* Verify bonded slave devices rx count */
		/* Verify slave ports tx stats */
		for (j = 0; j < test_params->bonded_slave_count; j++) {
			rte_eth_stats_get(test_params->slave_port_ids[j], &port_stats);

			if (i == j) {
				TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
						"Slave Port (%d) ipackets value (%u) not as expected"
						" (%d)", test_params->slave_port_ids[i],
						(unsigned int)port_stats.ipackets, burst_size);
			} else {
				TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
						"Slave Port (%d) ipackets value (%u) not as expected"
						" (%d)", test_params->slave_port_ids[i],
						(unsigned int)port_stats.ipackets, 0);
			}

			/* Reset bonded slaves stats */
			rte_eth_stats_reset(test_params->slave_port_ids[j]);
		}
		/* reset bonded device stats */
		rte_eth_stats_reset(test_params->bonded_port_id);
	}

	/* free mbufs */
	for (i = 0; i < MAX_PKT_BURST; i++) {
		if (rx_pkt_burst[i] != NULL)
			rte_pktmbuf_free(rx_pkt_burst[i]);
	}


	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

#define TEST_ROUNDROBIN_TX_BURST_SLAVE_COUNT (3)

static int
test_roundrobin_rx_burst_on_multiple_slaves(void)
{
	struct rte_mbuf *gen_pkt_burst[TEST_ROUNDROBIN_TX_BURST_SLAVE_COUNT][MAX_PKT_BURST];

	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	int burst_size[TEST_ROUNDROBIN_TX_BURST_SLAVE_COUNT] = { 15, 13, 36 };
	int i, nb_rx;

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ROUND_ROBIN, 0, 4, 1),
			"Failed to initialize bonded device with slaves");

	/* Generate test bursts of packets to transmit */
	for (i = 0; i < TEST_ROUNDROBIN_TX_BURST_SLAVE_COUNT; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[i][0], burst_size[i], 0, 1, 0, 0, 0),
				burst_size[i], "burst generation failed");
	}

	/* Add rx data to slaves */
	for (i = 0; i < TEST_ROUNDROBIN_TX_BURST_SLAVE_COUNT; i++) {
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[i][0], burst_size[i]);
	}

	/* Call rx burst on bonded device */
	/* Send burst on bonded port */
	nb_rx = rte_eth_rx_burst(test_params->bonded_port_id, 0, rx_pkt_burst,
			MAX_PKT_BURST);
	TEST_ASSERT_EQUAL(nb_rx , burst_size[0] + burst_size[1] + burst_size[2],
			"round-robin rx burst failed (%d != %d)\n", nb_rx,
			burst_size[0] + burst_size[1] + burst_size[2]);

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets,
			(uint64_t)(burst_size[0] + burst_size[1] + burst_size[2]),
			"Bonded Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.ipackets,
			burst_size[0] + burst_size[1] + burst_size[2]);

	/* Verify bonded slave devices rx counts */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[0],
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[0],
			(unsigned int)port_stats.ipackets, burst_size[0]);

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[1],
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[1], (unsigned int)port_stats.ipackets,
			burst_size[1]);

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[2],
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
				test_params->slave_port_ids[2],
				(unsigned int)port_stats.ipackets, burst_size[2]);

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[3],
			(unsigned int)port_stats.ipackets, 0);

	/* free mbufs */
	for (i = 0; i < MAX_PKT_BURST; i++) {
		if (rx_pkt_burst[i] != NULL)
			rte_pktmbuf_free(rx_pkt_burst[i]);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_roundrobin_verify_mac_assignment(void)
{
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr expected_mac_addr_0, expected_mac_addr_2;

	int i;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &expected_mac_addr_0),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[2], &expected_mac_addr_2),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[2]);

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
				BONDING_MODE_ROUND_ROBIN, 0, 4, 1),
				"Failed to initialize bonded device with slaves");

	/* Verify that all MACs are the same as first slave added to bonded dev */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->slave_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr)),
				"slave port (%d) mac address not set to that of primary port",
				test_params->slave_port_ids[i]);
	}

	/* change primary and verify that MAC addresses haven't changed */
	TEST_ASSERT_SUCCESS(rte_eth_bond_primary_set(test_params->bonded_port_id,
			test_params->slave_port_ids[2]),
			"Failed to set bonded port (%d) primary port to (%d)",
			test_params->bonded_port_id, test_params->slave_port_ids[i]);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->slave_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr)),
				"slave port (%d) mac address has changed to that of primary"
				" port without stop/start toggle of bonded device",
				test_params->slave_port_ids[i]);
	}

	/* stop / start bonded device and verify that primary MAC address is
	 * propagate to bonded device and slaves */
	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonded_port_id),
			"Failed to stop bonded port %u",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonded_port_id),
			"Failed to start bonded device");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(
			memcmp(&expected_mac_addr_2, &read_mac_addr, sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of new primary port",
			test_params->slave_port_ids[i]);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->slave_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_2, &read_mac_addr,
				sizeof(read_mac_addr)),
				"slave port (%d) mac address not set to that of new primary"
				" port", test_params->slave_port_ids[i]);
	}

	/* Set explicit MAC address */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonded_port_id,
			(struct rte_ether_addr *)bonded_mac),
			"Failed to set MAC");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(bonded_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of new primary port",
				test_params->slave_port_ids[i]);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->slave_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(bonded_mac, &read_mac_addr,
				sizeof(read_mac_addr)), "slave port (%d) mac address not set to"
				" that of new primary port\n", test_params->slave_port_ids[i]);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_roundrobin_verify_promiscuous_enable_disable(void)
{
	int i, promiscuous_en;
	int ret;

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ROUND_ROBIN, 0, 4, 1),
			"Failed to initialize bonded device with slaves");

	ret = rte_eth_promiscuous_enable(test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to enable promiscuous mode for port %d: %s",
		test_params->bonded_port_id, rte_strerror(-ret));

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	TEST_ASSERT_EQUAL(promiscuous_en, 1,
			"Port (%d) promiscuous mode not enabled",
			test_params->bonded_port_id);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		TEST_ASSERT_EQUAL(promiscuous_en, 1,
				"slave port (%d) promiscuous mode not enabled",
				test_params->slave_port_ids[i]);
	}

	ret = rte_eth_promiscuous_disable(test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to disable promiscuous mode for port %d: %s",
		test_params->bonded_port_id, rte_strerror(-ret));

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	TEST_ASSERT_EQUAL(promiscuous_en, 0,
			"Port (%d) promiscuous mode not disabled\n",
			test_params->bonded_port_id);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		TEST_ASSERT_EQUAL(promiscuous_en, 0,
				"Port (%d) promiscuous mode not disabled\n",
				test_params->slave_port_ids[i]);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

#define TEST_RR_LINK_STATUS_SLAVE_COUNT (4)
#define TEST_RR_LINK_STATUS_EXPECTED_ACTIVE_SLAVE_COUNT (2)

static int
test_roundrobin_verify_slave_link_status_change_behaviour(void)
{
	struct rte_mbuf *tx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_mbuf *gen_pkt_burst[TEST_RR_LINK_STATUS_SLAVE_COUNT][MAX_PKT_BURST];
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };

	struct rte_eth_stats port_stats;
	uint16_t slaves[RTE_MAX_ETHPORTS];

	int i, burst_size, slave_count;

	/* NULL all pointers in array to simplify cleanup */
	memset(gen_pkt_burst, 0, sizeof(gen_pkt_burst));

	/* Initialize bonded device with TEST_RR_LINK_STATUS_SLAVE_COUNT slaves
	 * in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ROUND_ROBIN, 0, TEST_RR_LINK_STATUS_SLAVE_COUNT, 1),
			"Failed to initialize bonded device with slaves");

	/* Verify Current Slaves Count /Active Slave Count is */
	slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count, TEST_RR_LINK_STATUS_SLAVE_COUNT,
			"Number of slaves (%d) is not as expected (%d).",
			slave_count, TEST_RR_LINK_STATUS_SLAVE_COUNT);

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count, TEST_RR_LINK_STATUS_SLAVE_COUNT,
			"Number of active slaves (%d) is not as expected (%d).",
			slave_count, TEST_RR_LINK_STATUS_SLAVE_COUNT);

	/* Set 2 slaves eth_devs link status to down */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 0);

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count,
			TEST_RR_LINK_STATUS_EXPECTED_ACTIVE_SLAVE_COUNT,
			"Number of active slaves (%d) is not as expected (%d).\n",
			slave_count, TEST_RR_LINK_STATUS_EXPECTED_ACTIVE_SLAVE_COUNT);

	burst_size = 20;

	/* Verify that pkts are not sent on slaves with link status down:
	 *
	 * 1. Generate test burst of traffic
	 * 2. Transmit burst on bonded eth_dev
	 * 3. Verify stats for bonded eth_dev (opackets = burst_size)
	 * 4. Verify stats for slave eth_devs (s0 = 10, s1 = 0, s2 = 10, s3 = 0)
	 */
	TEST_ASSERT_EQUAL(
			generate_test_burst(tx_pkt_burst, burst_size, 0, 1, 0, 0, 0),
			burst_size, "generate_test_burst failed");

	rte_eth_stats_reset(test_params->bonded_port_id);


	TEST_ASSERT_EQUAL(
			rte_eth_tx_burst(test_params->bonded_port_id, 0, tx_pkt_burst,
			burst_size), burst_size, "rte_eth_tx_burst failed");

	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"Port (%d) opackets stats (%d) not expected (%d) value",
			test_params->bonded_port_id, (int)port_stats.opackets,
			burst_size);

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)10,
			"Port (%d) opackets stats (%d) not expected (%d) value",
			test_params->slave_port_ids[0], (int)port_stats.opackets, 10);

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)0,
			"Port (%d) opackets stats (%d) not expected (%d) value",
			test_params->slave_port_ids[1], (int)port_stats.opackets, 0);

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)10,
			"Port (%d) opackets stats (%d) not expected (%d) value",
			test_params->slave_port_ids[2], (int)port_stats.opackets, 10);

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)0,
			"Port (%d) opackets stats (%d) not expected (%d) value",
			test_params->slave_port_ids[3], (int)port_stats.opackets, 0);

	/* Verify that pkts are not sent on slaves with link status down:
	 *
	 * 1. Generate test bursts of traffic
	 * 2. Add bursts on to virtual eth_devs
	 * 3. Rx burst on bonded eth_dev, expected (burst_ size *
	 *    TEST_RR_LINK_STATUS_EXPECTED_ACTIVE_SLAVE_COUNT) received
	 * 4. Verify stats for bonded eth_dev
	 * 6. Verify stats for slave eth_devs (s0 = 10, s1 = 0, s2 = 10, s3 = 0)
	 */
	for (i = 0; i < TEST_RR_LINK_STATUS_SLAVE_COUNT; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[i][0], burst_size, 0, 1, 0, 0, 0),
				burst_size, "failed to generate packet burst");

		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[i][0], burst_size);
	}

	TEST_ASSERT_EQUAL(rte_eth_rx_burst(
			test_params->bonded_port_id, 0, rx_pkt_burst, MAX_PKT_BURST),
			burst_size + burst_size,
			"rte_eth_rx_burst failed");

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets , (uint64_t)(burst_size + burst_size),
			"(%d) port_stats.ipackets not as expected\n",
			test_params->bonded_port_id);

	/* free mbufs */
	for (i = 0; i < MAX_PKT_BURST; i++) {
		if (rx_pkt_burst[i] != NULL)
			rte_pktmbuf_free(rx_pkt_burst[i]);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

#define TEST_RR_POLLING_LINK_STATUS_SLAVE_COUNT (2)

uint8_t polling_slave_mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00 };


int polling_test_slaves[TEST_RR_POLLING_LINK_STATUS_SLAVE_COUNT] = { -1, -1 };

static int
test_roundrobin_verfiy_polling_slave_link_status_change(void)
{
	struct rte_ether_addr *mac_addr =
		(struct rte_ether_addr *)polling_slave_mac;
	char slave_name[RTE_ETH_NAME_MAX_LEN];

	int i;

	for (i = 0; i < TEST_RR_POLLING_LINK_STATUS_SLAVE_COUNT; i++) {
		/* Generate slave name / MAC address */
		snprintf(slave_name, RTE_ETH_NAME_MAX_LEN, "eth_virt_poll_%d", i);
		mac_addr->addr_bytes[RTE_ETHER_ADDR_LEN-1] = i;

		/* Create slave devices with no ISR Support */
		if (polling_test_slaves[i] == -1) {
			polling_test_slaves[i] = virtual_ethdev_create(slave_name, mac_addr,
					rte_socket_id(), 0);
			TEST_ASSERT(polling_test_slaves[i] >= 0,
					"Failed to create virtual virtual ethdev %s\n", slave_name);

			/* Configure slave */
			TEST_ASSERT_SUCCESS(configure_ethdev(polling_test_slaves[i], 0, 0),
					"Failed to configure virtual ethdev %s(%d)", slave_name,
					polling_test_slaves[i]);
		}

		/* Add slave to bonded device */
		TEST_ASSERT_SUCCESS(rte_eth_bond_slave_add(test_params->bonded_port_id,
				polling_test_slaves[i]),
				"Failed to add slave %s(%d) to bonded device %d",
				slave_name, polling_test_slaves[i],
				test_params->bonded_port_id);
	}

	/* Initialize bonded device */
	TEST_ASSERT_SUCCESS(configure_ethdev(test_params->bonded_port_id, 1, 1),
			"Failed to configure bonded device %d",
			test_params->bonded_port_id);


	/* Register link status change interrupt callback */
	rte_eth_dev_callback_register(test_params->bonded_port_id,
			RTE_ETH_EVENT_INTR_LSC, test_bonding_lsc_event_callback,
			&test_params->bonded_port_id);

	/* link status change callback for first slave link up */
	test_lsc_interrupt_count = 0;

	virtual_ethdev_set_link_status(polling_test_slaves[0], 1);

	TEST_ASSERT_SUCCESS(lsc_timeout(15000), "timed out waiting for interrupt");


	/* no link status change callback for second slave link up */
	test_lsc_interrupt_count = 0;

	virtual_ethdev_set_link_status(polling_test_slaves[1], 1);

	TEST_ASSERT_FAIL(lsc_timeout(15000), "unexpectedly succeeded");

	/* link status change callback for both slave links down */
	test_lsc_interrupt_count = 0;

	virtual_ethdev_set_link_status(polling_test_slaves[0], 0);
	virtual_ethdev_set_link_status(polling_test_slaves[1], 0);

	TEST_ASSERT_SUCCESS(lsc_timeout(20000), "timed out waiting for interrupt");

	/* Un-Register link status change interrupt callback */
	rte_eth_dev_callback_unregister(test_params->bonded_port_id,
			RTE_ETH_EVENT_INTR_LSC, test_bonding_lsc_event_callback,
			&test_params->bonded_port_id);


	/* Clean up and remove slaves from bonded device */
	for (i = 0; i < TEST_RR_POLLING_LINK_STATUS_SLAVE_COUNT; i++) {

		TEST_ASSERT_SUCCESS(
				rte_eth_bond_slave_remove(test_params->bonded_port_id,
						polling_test_slaves[i]),
				"Failed to remove slave %d from bonded port (%d)",
				polling_test_slaves[i], test_params->bonded_port_id);
	}

	return remove_slaves_and_stop_bonded_device();
}


/** Active Backup Mode Tests */

static int
test_activebackup_tx_burst(void)
{
	int i, pktlen, primary_port, burst_size;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_eth_stats port_stats;

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ACTIVE_BACKUP, 0, 1, 1),
			"Failed to initialize bonded device with slaves");

	initialize_eth_header(test_params->pkt_eth_hdr,
			(struct rte_ether_addr *)src_mac,
			(struct rte_ether_addr *)dst_mac_0,
			RTE_ETHER_TYPE_IPV4,  0, 0);
	pktlen = initialize_udp_header(test_params->pkt_udp_hdr, src_port,
			dst_port_0, 16);
	pktlen = initialize_ipv4_header(test_params->pkt_ipv4_hdr, src_addr,
			dst_addr_0, pktlen);

	burst_size = 20 * test_params->bonded_slave_count;

	TEST_ASSERT(burst_size < MAX_PKT_BURST,
			"Burst size specified is greater than supported.");

	/* Generate a burst of packets to transmit */
	TEST_ASSERT_EQUAL(generate_packet_burst(test_params->mbuf_pool, pkts_burst,
			test_params->pkt_eth_hdr, 0, test_params->pkt_ipv4_hdr, 1,
			test_params->pkt_udp_hdr, burst_size, PACKET_BURST_GEN_PKT_LEN, 1),
			burst_size,	"failed to generate burst correctly");

	/* Send burst on bonded port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst,
			burst_size),  burst_size, "tx burst failed");

	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"Bonded Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.opackets,
			burst_size);

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);

	/* Verify slave ports tx stats */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_stats_get(test_params->slave_port_ids[i], &port_stats);
		if (test_params->slave_port_ids[i] == primary_port) {
			TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
					"Slave Port (%d) opackets value (%u) not as expected (%d)",
					test_params->bonded_port_id,
					(unsigned int)port_stats.opackets,
					burst_size / test_params->bonded_slave_count);
		} else {
			TEST_ASSERT_EQUAL(port_stats.opackets, 0,
					"Slave Port (%d) opackets value (%u) not as expected (%d)",
					test_params->bonded_port_id,
					(unsigned int)port_stats.opackets, 0);
		}
	}

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonded_port_id, 0,
			pkts_burst, burst_size), 0, "Sending empty burst failed");

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

#define TEST_ACTIVE_BACKUP_RX_BURST_SLAVE_COUNT (4)

static int
test_activebackup_rx_burst(void)
{
	struct rte_mbuf *gen_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };

	struct rte_eth_stats port_stats;

	int primary_port;

	int i, j, burst_size = 17;

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ACTIVE_BACKUP, 0,
			TEST_ACTIVE_BACKUP_RX_BURST_SLAVE_COUNT, 1),
			"Failed to initialize bonded device with slaves");

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	TEST_ASSERT(primary_port >= 0,
			"failed to get primary slave for bonded port (%d)",
			test_params->bonded_port_id);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		/* Generate test bursts of packets to transmit */
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[0], burst_size, 0, 1, 0, 0, 0),
				burst_size, "burst generation failed");

		/* Add rx data to slave */
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[0], burst_size);

		/* Call rx burst on bonded device */
		TEST_ASSERT_EQUAL(rte_eth_rx_burst(test_params->bonded_port_id, 0,
				&rx_pkt_burst[0], MAX_PKT_BURST), burst_size,
				"rte_eth_rx_burst failed");

		if (test_params->slave_port_ids[i] == primary_port) {
			/* Verify bonded device rx count */
			rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
			TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
					"Bonded Port (%d) ipackets value (%u) not as expected (%d)",
					test_params->bonded_port_id,
					(unsigned int)port_stats.ipackets, burst_size);

			/* Verify bonded slave devices rx count */
			for (j = 0; j < test_params->bonded_slave_count; j++) {
				rte_eth_stats_get(test_params->slave_port_ids[j], &port_stats);
				if (i == j) {
					TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
							"Slave Port (%d) ipackets value (%u) not as "
							"expected (%d)", test_params->slave_port_ids[i],
							(unsigned int)port_stats.ipackets, burst_size);
				} else {
					TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
							"Slave Port (%d) ipackets value (%u) not as "
							"expected (%d)\n", test_params->slave_port_ids[i],
							(unsigned int)port_stats.ipackets, 0);
				}
			}
		} else {
			for (j = 0; j < test_params->bonded_slave_count; j++) {
				rte_eth_stats_get(test_params->slave_port_ids[j], &port_stats);
				TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
						"Slave Port (%d) ipackets value (%u) not as expected "
						"(%d)", test_params->slave_port_ids[i],
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

		/* reset bonded device stats */
		rte_eth_stats_reset(test_params->bonded_port_id);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_activebackup_verify_promiscuous_enable_disable(void)
{
	int i, primary_port, promiscuous_en;
	int ret;

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ACTIVE_BACKUP, 0, 4, 1),
			"Failed to initialize bonded device with slaves");

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	TEST_ASSERT(primary_port >= 0,
			"failed to get primary slave for bonded port (%d)",
			test_params->bonded_port_id);

	ret = rte_eth_promiscuous_enable(test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to enable promiscuous mode for port %d: %s",
		test_params->bonded_port_id, rte_strerror(-ret));

	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonded_port_id), 1,
			"Port (%d) promiscuous mode not enabled",
			test_params->bonded_port_id);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		if (primary_port == test_params->slave_port_ids[i]) {
			TEST_ASSERT_EQUAL(promiscuous_en, 1,
					"slave port (%d) promiscuous mode not enabled",
					test_params->slave_port_ids[i]);
		} else {
			TEST_ASSERT_EQUAL(promiscuous_en, 0,
					"slave port (%d) promiscuous mode enabled",
					test_params->slave_port_ids[i]);
		}

	}

	ret = rte_eth_promiscuous_disable(test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to disable promiscuous mode for port %d: %s",
		test_params->bonded_port_id, rte_strerror(-ret));

	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonded_port_id), 0,
			"Port (%d) promiscuous mode not disabled\n",
			test_params->bonded_port_id);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		TEST_ASSERT_EQUAL(promiscuous_en, 0,
				"slave port (%d) promiscuous mode not disabled\n",
				test_params->slave_port_ids[i]);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_activebackup_verify_mac_assignment(void)
{
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr expected_mac_addr_0, expected_mac_addr_1;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &expected_mac_addr_0),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &expected_mac_addr_1),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);

	/* Initialize bonded device with 2 slaves in active backup mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ACTIVE_BACKUP, 0, 2, 1),
			"Failed to initialize bonded device with slaves");

	/* Verify that bonded MACs is that of first slave and that the other slave
	 * MAC hasn't been changed */
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of primary port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not as expected",
			test_params->slave_port_ids[1]);

	/* change primary and verify that MAC addresses haven't changed */
	TEST_ASSERT_EQUAL(rte_eth_bond_primary_set(test_params->bonded_port_id,
			test_params->slave_port_ids[1]), 0,
			"Failed to set bonded port (%d) primary port to (%d)",
			test_params->bonded_port_id, test_params->slave_port_ids[1]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of primary port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not as expected",
			test_params->slave_port_ids[1]);

	/* stop / start bonded device and verify that primary MAC address is
	 * propagated to bonded device and slaves */

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonded_port_id),
			"Failed to stop bonded port %u",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonded_port_id),
			"Failed to start device");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of primary port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not as expected",
			test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[1]);

	/* Set explicit MAC address */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonded_port_id,
			(struct rte_ether_addr *)bonded_mac),
			"failed to set MAC address");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&bonded_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of bonded port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not as expected",
			test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&bonded_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of bonded port",
			test_params->slave_port_ids[1]);

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_activebackup_verify_slave_link_status_change_failover(void)
{
	struct rte_mbuf *pkt_burst[TEST_ACTIVE_BACKUP_RX_BURST_SLAVE_COUNT][MAX_PKT_BURST];
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	uint16_t slaves[RTE_MAX_ETHPORTS];

	int i, burst_size, slave_count, primary_port;

	burst_size = 21;

	memset(pkt_burst, 0, sizeof(pkt_burst));

	/* Generate packet burst for testing */
	TEST_ASSERT_EQUAL(generate_test_burst(
			&pkt_burst[0][0], burst_size, 0, 1, 0, 0, 0), burst_size,
			"generate_test_burst failed");

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ACTIVE_BACKUP, 0,
			TEST_ACTIVE_BACKUP_RX_BURST_SLAVE_COUNT, 1),
			"Failed to initialize bonded device with slaves");

	/* Verify Current Slaves Count /Active Slave Count is */
	slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count, 4,
			"Number of slaves (%d) is not as expected (%d).",
			slave_count, 4);

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count, 4,
			"Number of active slaves (%d) is not as expected (%d).",
			slave_count, 4);

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	TEST_ASSERT_EQUAL(primary_port, test_params->slave_port_ids[0],
			"Primary port not as expected");

	/* Bring 2 slaves down and verify active slave count */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS), 2,
			"Number of active slaves (%d) is not as expected (%d).",
			slave_count, 2);

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 1);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 1);


	/* Bring primary port down, verify that active slave count is 3 and primary
	 *  has changed */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[0], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS),
			3,
			"Number of active slaves (%d) is not as expected (%d).",
			slave_count, 3);

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	TEST_ASSERT_EQUAL(primary_port, test_params->slave_port_ids[2],
			"Primary port not as expected");

	/* Verify that pkts are sent on new primary slave */

	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonded_port_id, 0, &pkt_burst[0][0],
			burst_size), burst_size, "rte_eth_tx_burst failed");

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets not as expected",
			test_params->slave_port_ids[2]);

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->slave_port_ids[0]);

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->slave_port_ids[1]);

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->slave_port_ids[3]);

	/* Generate packet burst for testing */

	for (i = 0; i < TEST_ACTIVE_BACKUP_RX_BURST_SLAVE_COUNT; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&pkt_burst[i][0], burst_size, 0, 1, 0, 0, 0), burst_size,
				"generate_test_burst failed");

		virtual_ethdev_add_mbufs_to_rx_queue(
			test_params->slave_port_ids[i], &pkt_burst[i][0], burst_size);
	}

	TEST_ASSERT_EQUAL(rte_eth_rx_burst(
			test_params->bonded_port_id, 0, rx_pkt_burst, MAX_PKT_BURST),
			burst_size, "rte_eth_rx_burst\n");

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
			"(%d) port_stats.ipackets not as expected",
			test_params->bonded_port_id);

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets not as expected",
			test_params->slave_port_ids[2]);

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected",
			test_params->slave_port_ids[0]);

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected",
			test_params->slave_port_ids[1]);

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected",
			test_params->slave_port_ids[3]);

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

/** Balance Mode Tests */

static int
test_balance_xmit_policy_configuration(void)
{
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_ACTIVE_BACKUP, 0, 2, 1),
			"Failed to initialize_bonded_device_with_slaves.");

	/* Invalid port id */
	TEST_ASSERT_FAIL(rte_eth_bond_xmit_policy_set(
			INVALID_PORT_ID, BALANCE_XMIT_POLICY_LAYER2),
			"Expected call to failed as invalid port specified.");

	/* Set xmit policy on non bonded device */
	TEST_ASSERT_FAIL(rte_eth_bond_xmit_policy_set(
			test_params->slave_port_ids[0],	BALANCE_XMIT_POLICY_LAYER2),
			"Expected call to failed as invalid port specified.");


	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonded_port_id, BALANCE_XMIT_POLICY_LAYER2),
			"Failed to set balance xmit policy.");

	TEST_ASSERT_EQUAL(rte_eth_bond_xmit_policy_get(test_params->bonded_port_id),
			BALANCE_XMIT_POLICY_LAYER2, "balance xmit policy not as expected.");


	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonded_port_id, BALANCE_XMIT_POLICY_LAYER23),
			"Failed to set balance xmit policy.");

	TEST_ASSERT_EQUAL(rte_eth_bond_xmit_policy_get(test_params->bonded_port_id),
			BALANCE_XMIT_POLICY_LAYER23,
			"balance xmit policy not as expected.");


	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonded_port_id, BALANCE_XMIT_POLICY_LAYER34),
			"Failed to set balance xmit policy.");

	TEST_ASSERT_EQUAL(rte_eth_bond_xmit_policy_get(test_params->bonded_port_id),
			BALANCE_XMIT_POLICY_LAYER34,
			"balance xmit policy not as expected.");

	/* Invalid port id */
	TEST_ASSERT_FAIL(rte_eth_bond_xmit_policy_get(INVALID_PORT_ID),
			"Expected call to failed as invalid port specified.");

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

#define TEST_BALANCE_L2_TX_BURST_SLAVE_COUNT (2)

static int
test_balance_l2_tx_burst(void)
{
	struct rte_mbuf *pkts_burst[TEST_BALANCE_L2_TX_BURST_SLAVE_COUNT][MAX_PKT_BURST];
	int burst_size[TEST_BALANCE_L2_TX_BURST_SLAVE_COUNT] = { 10, 15 };

	uint16_t pktlen;
	int i;
	struct rte_eth_stats port_stats;

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BALANCE, 0, TEST_BALANCE_L2_TX_BURST_SLAVE_COUNT, 1),
			"Failed to initialize_bonded_device_with_slaves.");

	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonded_port_id, BALANCE_XMIT_POLICY_LAYER2),
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

	/* Send burst 1 on bonded port */
	for (i = 0; i < TEST_BALANCE_L2_TX_BURST_SLAVE_COUNT; i++) {
		TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonded_port_id, 0,
				&pkts_burst[i][0], burst_size[i]),
				burst_size[i], "Failed to transmit packet burst");
	}

	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)(burst_size[0] + burst_size[1]),
			"Bonded Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.opackets,
			burst_size[0] + burst_size[1]);


	/* Verify slave ports tx stats */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size[0],
			"Slave Port (%d) opackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[0], (unsigned int)port_stats.opackets,
			burst_size[0]);

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size[1],
			"Slave Port (%d) opackets value (%u) not as expected (%d)\n",
			test_params->slave_port_ids[1], (unsigned int)port_stats.opackets,
			burst_size[1]);

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonded_port_id, 0, &pkts_burst[0][0], burst_size[0]),
			0, "Expected zero packet");

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
balance_l23_tx_burst(uint8_t vlan_enabled, uint8_t ipv4,
		uint8_t toggle_mac_addr, uint8_t toggle_ip_addr)
{
	int i, burst_size_1, burst_size_2, nb_tx_1, nb_tx_2;

	struct rte_mbuf *pkts_burst_1[MAX_PKT_BURST];
	struct rte_mbuf *pkts_burst_2[MAX_PKT_BURST];

	struct rte_eth_stats port_stats;

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BALANCE, 0, 2, 1),
			"Failed to initialize_bonded_device_with_slaves.");

	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonded_port_id, BALANCE_XMIT_POLICY_LAYER23),
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

	/* Send burst 1 on bonded port */
	nb_tx_1 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_1,
			burst_size_1);
	TEST_ASSERT_EQUAL(nb_tx_1, burst_size_1, "tx burst failed");

	/* Send burst 2 on bonded port */
	nb_tx_2 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_2,
			burst_size_2);
	TEST_ASSERT_EQUAL(nb_tx_2, burst_size_2, "tx burst failed");

	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)(nb_tx_1 + nb_tx_2),
			"Bonded Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.opackets,
			nb_tx_1 + nb_tx_2);

	/* Verify slave ports tx stats */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)nb_tx_1,
			"Slave Port (%d) opackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[0], (unsigned int)port_stats.opackets,
			nb_tx_1);

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)nb_tx_2,
			"Slave Port (%d) opackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[1], (unsigned int)port_stats.opackets,
			nb_tx_2);

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonded_port_id, 0, pkts_burst_1,
			burst_size_1), 0, "Expected zero packet");


	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
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

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BALANCE, 0, 2, 1),
			"Failed to initialize_bonded_device_with_slaves.");

	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonded_port_id, BALANCE_XMIT_POLICY_LAYER34),
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

	/* Send burst 1 on bonded port */
	nb_tx_1 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_1,
			burst_size_1);
	TEST_ASSERT_EQUAL(nb_tx_1, burst_size_1, "tx burst failed");

	/* Send burst 2 on bonded port */
	nb_tx_2 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_2,
			burst_size_2);
	TEST_ASSERT_EQUAL(nb_tx_2, burst_size_2, "tx burst failed");


	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)(nb_tx_1 + nb_tx_2),
			"Bonded Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.opackets,
			nb_tx_1 + nb_tx_2);

	/* Verify slave ports tx stats */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)nb_tx_1,
			"Slave Port (%d) opackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[0], (unsigned int)port_stats.opackets,
			nb_tx_1);

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)nb_tx_2,
			"Slave Port (%d) opackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[1], (unsigned int)port_stats.opackets,
			nb_tx_2);

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonded_port_id, 0, pkts_burst_1,
			burst_size_1), 0, "Expected zero packet");

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
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

#define TEST_BAL_SLAVE_TX_FAIL_SLAVE_COUNT			(2)
#define TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1			(40)
#define TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_2			(20)
#define TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT		(25)
#define TEST_BAL_SLAVE_TX_FAIL_FAILING_SLAVE_IDX	(0)

static int
test_balance_tx_burst_slave_tx_fail(void)
{
	struct rte_mbuf *pkts_burst_1[TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1];
	struct rte_mbuf *pkts_burst_2[TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_2];

	struct rte_mbuf *expected_fail_pkts[TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT];

	struct rte_eth_stats port_stats;

	int i, first_tx_fail_idx, tx_count_1, tx_count_2;

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BALANCE, 0,
			TEST_BAL_SLAVE_TX_FAIL_SLAVE_COUNT, 1),
			"Failed to initialise bonded device");

	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonded_port_id, BALANCE_XMIT_POLICY_LAYER2),
			"Failed to set balance xmit policy.");


	/* Generate test bursts for transmission */
	TEST_ASSERT_EQUAL(generate_test_burst(pkts_burst_1,
			TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1, 0, 0, 0, 0, 0),
			TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1,
			"Failed to generate test packet burst 1");

	first_tx_fail_idx = TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1 -
			TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT;

	/* copy mbuf references for expected transmission failures */
	for (i = 0; i < TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT; i++)
		expected_fail_pkts[i] = pkts_burst_1[i + first_tx_fail_idx];

	TEST_ASSERT_EQUAL(generate_test_burst(pkts_burst_2,
			TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_2, 0, 0, 1, 0, 0),
			TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_2,
			"Failed to generate test packet burst 2");


	/* Set virtual slave TEST_BAL_SLAVE_TX_FAIL_FAILING_SLAVE_IDX to only fail
	 * transmission of TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT packets of burst */
	virtual_ethdev_tx_burst_fn_set_success(
			test_params->slave_port_ids[TEST_BAL_SLAVE_TX_FAIL_FAILING_SLAVE_IDX],
			0);

	virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(
			test_params->slave_port_ids[TEST_BAL_SLAVE_TX_FAIL_FAILING_SLAVE_IDX],
			TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT);


	/* Transmit burst 1 */
	tx_count_1 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_1,
			TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1);

	TEST_ASSERT_EQUAL(tx_count_1, TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1 -
			TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT,
			"Transmitted (%d) packets, expected to transmit (%d) packets",
			tx_count_1, TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1 -
			TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT);

	/* Verify that failed packet are expected failed packets */
	for (i = 0; i < TEST_RR_SLAVE_TX_FAIL_PACKETS_COUNT; i++) {
		TEST_ASSERT_EQUAL(expected_fail_pkts[i], pkts_burst_1[i + tx_count_1],
				"expected mbuf (%d) pointer %p not expected pointer %p",
				i, expected_fail_pkts[i], pkts_burst_1[i + tx_count_1]);
	}

	/* Transmit burst 2 */
	tx_count_2 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_2,
			TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_2);

	TEST_ASSERT_EQUAL(tx_count_2, TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_2,
			"Transmitted (%d) packets, expected to transmit (%d) packets",
			tx_count_2, TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_2);


	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)((TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1 -
			TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT) +
			TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_2),
			"Bonded Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.opackets,
			(TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1 -
			TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT) +
			TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_2);

	/* Verify slave ports tx stats */

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)
				TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1 -
				TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT,
				"Slave Port (%d) opackets value (%u) not as expected (%d)",
				test_params->slave_port_ids[0],
				(unsigned int)port_stats.opackets,
				TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_1 -
				TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT);




	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
				(uint64_t)TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_2,
				"Slave Port (%d) opackets value (%u) not as expected (%d)",
				test_params->slave_port_ids[1],
				(unsigned int)port_stats.opackets,
				TEST_BAL_SLAVE_TX_FAIL_BURST_SIZE_2);

	/* Verify that all mbufs have a ref value of zero */
	TEST_ASSERT_SUCCESS(verify_mbufs_ref_count(&pkts_burst_1[tx_count_1],
			TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT, 1),
			"mbufs refcnts not as expected");

	free_mbufs(&pkts_burst_1[tx_count_1],
			TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT);

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

#define TEST_BALANCE_RX_BURST_SLAVE_COUNT (3)

static int
test_balance_rx_burst(void)
{
	struct rte_mbuf *gen_pkt_burst[TEST_BALANCE_RX_BURST_SLAVE_COUNT][MAX_PKT_BURST];

	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	int burst_size[TEST_BALANCE_RX_BURST_SLAVE_COUNT] = { 10, 5, 30 };
	int i, j;

	memset(gen_pkt_burst, 0, sizeof(gen_pkt_burst));

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BALANCE, 0, 3, 1),
			"Failed to initialise bonded device");

	/* Generate test bursts of packets to transmit */
	for (i = 0; i < TEST_BALANCE_RX_BURST_SLAVE_COUNT; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[i][0], burst_size[i], 0, 0, 1,
				0, 0), burst_size[i],
				"failed to generate packet burst");
	}

	/* Add rx data to slaves */
	for (i = 0; i < TEST_BALANCE_RX_BURST_SLAVE_COUNT; i++) {
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[i][0], burst_size[i]);
	}

	/* Call rx burst on bonded device */
	/* Send burst on bonded port */
	TEST_ASSERT_EQUAL(rte_eth_rx_burst(test_params->bonded_port_id, 0,
			rx_pkt_burst, MAX_PKT_BURST),
			burst_size[0] + burst_size[1] + burst_size[2],
			"balance rx burst failed\n");

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets,
			(uint64_t)(burst_size[0] + burst_size[1] + burst_size[2]),
			"Bonded Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.ipackets,
			burst_size[0] + burst_size[1] + burst_size[2]);


	/* Verify bonded slave devices rx counts */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[0],
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
				test_params->slave_port_ids[0],
				(unsigned int)port_stats.ipackets, burst_size[0]);

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[1],
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[1], (unsigned int)port_stats.ipackets,
			burst_size[1]);

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[2],
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[2], (unsigned int)port_stats.ipackets,
			burst_size[2]);

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[3],	(unsigned int)port_stats.ipackets,
			0);

	/* free mbufs */
	for (i = 0; i < TEST_BALANCE_RX_BURST_SLAVE_COUNT; i++) {
		for (j = 0; j < MAX_PKT_BURST; j++) {
			if (gen_pkt_burst[i][j] != NULL) {
				rte_pktmbuf_free(gen_pkt_burst[i][j]);
				gen_pkt_burst[i][j] = NULL;
			}
		}
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_balance_verify_promiscuous_enable_disable(void)
{
	int i;
	int ret;

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BALANCE, 0, 4, 1),
			"Failed to initialise bonded device");

	ret = rte_eth_promiscuous_enable(test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to enable promiscuous mode for port %d: %s",
		test_params->bonded_port_id, rte_strerror(-ret));

	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonded_port_id), 1,
			"Port (%d) promiscuous mode not enabled",
			test_params->bonded_port_id);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]), 1,
				"Port (%d) promiscuous mode not enabled",
				test_params->slave_port_ids[i]);
	}

	ret = rte_eth_promiscuous_disable(test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to disable promiscuous mode for port %d: %s",
		test_params->bonded_port_id, rte_strerror(-ret));

	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonded_port_id), 0,
			"Port (%d) promiscuous mode not disabled",
			test_params->bonded_port_id);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]), 0,
				"Port (%d) promiscuous mode not disabled",
				test_params->slave_port_ids[i]);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_balance_verify_mac_assignment(void)
{
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr expected_mac_addr_0, expected_mac_addr_1;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &expected_mac_addr_0),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &expected_mac_addr_1),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);

	/* Initialize bonded device with 2 slaves in active backup mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BALANCE, 0, 2, 1),
			"Failed to initialise bonded device");

	/* Verify that bonded MACs is that of first slave and that the other slave
	 * MAC hasn't been changed */
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of primary port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[1]);

	/* change primary and verify that MAC addresses haven't changed */
	TEST_ASSERT_SUCCESS(rte_eth_bond_primary_set(test_params->bonded_port_id,
			test_params->slave_port_ids[1]),
			"Failed to set bonded port (%d) primary port to (%d)\n",
			test_params->bonded_port_id, test_params->slave_port_ids[1]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of primary port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[1]);

	/* stop / start bonded device and verify that primary MAC address is
	 * propagated to bonded device and slaves */

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonded_port_id),
			"Failed to stop bonded port %u",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonded_port_id),
			"Failed to start bonded device");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of primary port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[1]);

	/* Set explicit MAC address */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonded_port_id,
			(struct rte_ether_addr *)bonded_mac),
			"failed to set MAC");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&bonded_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of bonded port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&bonded_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not as expected\n",
				test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&bonded_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of bonded port",
			test_params->slave_port_ids[1]);

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

#define TEST_BALANCE_LINK_STATUS_SLAVE_COUNT (4)

static int
test_balance_verify_slave_link_status_change_behaviour(void)
{
	struct rte_mbuf *pkt_burst[TEST_BALANCE_LINK_STATUS_SLAVE_COUNT][MAX_PKT_BURST];
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	uint16_t slaves[RTE_MAX_ETHPORTS];

	int i, burst_size, slave_count;

	memset(pkt_burst, 0, sizeof(pkt_burst));

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BALANCE, 0, TEST_BALANCE_LINK_STATUS_SLAVE_COUNT, 1),
			"Failed to initialise bonded device");

	TEST_ASSERT_SUCCESS(rte_eth_bond_xmit_policy_set(
			test_params->bonded_port_id, BALANCE_XMIT_POLICY_LAYER2),
			"Failed to set balance xmit policy.");


	/* Verify Current Slaves Count /Active Slave Count is */
	slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count, TEST_BALANCE_LINK_STATUS_SLAVE_COUNT,
			"Number of slaves (%d) is not as expected (%d).",
			slave_count, TEST_BALANCE_LINK_STATUS_SLAVE_COUNT);

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count, TEST_BALANCE_LINK_STATUS_SLAVE_COUNT,
			"Number of active slaves (%d) is not as expected (%d).",
			slave_count, TEST_BALANCE_LINK_STATUS_SLAVE_COUNT);

	/* Set 2 slaves link status to down */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS), 2,
			"Number of active slaves (%d) is not as expected (%d).",
			slave_count, 2);

	/* Send to sets of packet burst and verify that they are balanced across
	 *  slaves */
	burst_size = 21;

	TEST_ASSERT_EQUAL(generate_test_burst(
			&pkt_burst[0][0], burst_size, 0, 1, 0, 0, 0), burst_size,
			"generate_test_burst failed");

	TEST_ASSERT_EQUAL(generate_test_burst(
			&pkt_burst[1][0], burst_size, 0, 1, 1, 0, 0), burst_size,
			"generate_test_burst failed");

	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonded_port_id, 0, &pkt_burst[0][0], burst_size),
			burst_size, "rte_eth_tx_burst failed");

	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonded_port_id, 0, &pkt_burst[1][0], burst_size),
			burst_size, "rte_eth_tx_burst failed");


	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)(burst_size + burst_size),
			"(%d) port_stats.opackets (%d) not as expected (%d).",
			test_params->bonded_port_id, (int)port_stats.opackets,
			burst_size + burst_size);

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets (%d) not as expected (%d).",
			test_params->slave_port_ids[0], (int)port_stats.opackets,
			burst_size);

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets (%d) not as expected (%d).",
			test_params->slave_port_ids[2], (int)port_stats.opackets,
			burst_size);

	/* verify that all packets get send on primary slave when no other slaves
	 * are available */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[2], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS), 1,
			"Number of active slaves (%d) is not as expected (%d).",
			slave_count, 1);

	TEST_ASSERT_EQUAL(generate_test_burst(
			&pkt_burst[1][0], burst_size, 0, 1, 1, 0, 0), burst_size,
			"generate_test_burst failed");

	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonded_port_id, 0, &pkt_burst[1][0], burst_size),
			burst_size, "rte_eth_tx_burst failed");

	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)(burst_size + burst_size + burst_size),
			"(%d) port_stats.opackets (%d) not as expected (%d).\n",
			test_params->bonded_port_id, (int)port_stats.opackets,
			burst_size + burst_size + burst_size);

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)(burst_size + burst_size),
			"(%d) port_stats.opackets (%d) not as expected (%d).",
			test_params->slave_port_ids[0], (int)port_stats.opackets,
			burst_size + burst_size);

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[0], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 1);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[2], 1);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 1);

	for (i = 0; i < TEST_BALANCE_LINK_STATUS_SLAVE_COUNT; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&pkt_burst[i][0], burst_size, 0, 1, 0, 0, 0), burst_size,
				"Failed to generate packet burst");

		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&pkt_burst[i][0], burst_size);
	}

	/* Verify that pkts are not received on slaves with link status down */

	rte_eth_rx_burst(test_params->bonded_port_id, 0, rx_pkt_burst,
			MAX_PKT_BURST);

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)(burst_size * 3),
			"(%d) port_stats.ipackets (%d) not as expected (%d)\n",
			test_params->bonded_port_id, (int)port_stats.ipackets,
			burst_size * 3);

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_broadcast_tx_burst(void)
{
	int i, pktlen, burst_size;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	struct rte_eth_stats port_stats;

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BROADCAST, 0, 2, 1),
			"Failed to initialise bonded device");

	initialize_eth_header(test_params->pkt_eth_hdr,
			(struct rte_ether_addr *)src_mac,
			(struct rte_ether_addr *)dst_mac_0,
			RTE_ETHER_TYPE_IPV4, 0, 0);

	pktlen = initialize_udp_header(test_params->pkt_udp_hdr, src_port,
			dst_port_0, 16);
	pktlen = initialize_ipv4_header(test_params->pkt_ipv4_hdr, src_addr,
			dst_addr_0, pktlen);

	burst_size = 20 * test_params->bonded_slave_count;

	TEST_ASSERT(burst_size < MAX_PKT_BURST,
			"Burst size specified is greater than supported.");

	/* Generate a burst of packets to transmit */
	TEST_ASSERT_EQUAL(generate_packet_burst(test_params->mbuf_pool,
			pkts_burst,	test_params->pkt_eth_hdr, 0, test_params->pkt_ipv4_hdr,
			1, test_params->pkt_udp_hdr, burst_size, PACKET_BURST_GEN_PKT_LEN,
			1), burst_size, "Failed to generate packet burst");

	/* Send burst on bonded port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonded_port_id, 0,
			pkts_burst, burst_size), burst_size,
			"Bonded Port (%d) rx burst failed, packets transmitted value "
			"not as expected (%d)",
			test_params->bonded_port_id, burst_size);

	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)burst_size * test_params->bonded_slave_count,
			"Bonded Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.opackets,
			burst_size);

	/* Verify slave ports tx stats */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_stats_get(test_params->slave_port_ids[i], &port_stats);
		TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
				"Slave Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->bonded_port_id,
				(unsigned int)port_stats.opackets, burst_size);
	}

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	TEST_ASSERT_EQUAL(rte_eth_tx_burst(
			test_params->bonded_port_id, 0, pkts_burst, burst_size),  0,
			"transmitted an unexpected number of packets");

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}


#define TEST_BCAST_SLAVE_TX_FAIL_SLAVE_COUNT		(3)
#define TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE			(40)
#define TEST_BCAST_SLAVE_TX_FAIL_MAX_PACKETS_COUNT	(15)
#define TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT	(10)

static int
test_broadcast_tx_burst_slave_tx_fail(void)
{
	struct rte_mbuf *pkts_burst[TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE];
	struct rte_mbuf *expected_fail_pkts[TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT];

	struct rte_eth_stats port_stats;

	int i, tx_count;

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BROADCAST, 0,
			TEST_BCAST_SLAVE_TX_FAIL_SLAVE_COUNT, 1),
			"Failed to initialise bonded device");

	/* Generate test bursts for transmission */
	TEST_ASSERT_EQUAL(generate_test_burst(pkts_burst,
			TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE, 0, 0, 0, 0, 0),
			TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE,
			"Failed to generate test packet burst");

	for (i = 0; i < TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT; i++) {
		expected_fail_pkts[i] = pkts_burst[TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT + i];
	}

	/* Set virtual slave TEST_BAL_SLAVE_TX_FAIL_FAILING_SLAVE_IDX to only fail
	 * transmission of TEST_BAL_SLAVE_TX_FAIL_PACKETS_COUNT packets of burst */
	virtual_ethdev_tx_burst_fn_set_success(
			test_params->slave_port_ids[0],
			0);
	virtual_ethdev_tx_burst_fn_set_success(
			test_params->slave_port_ids[1],
			0);
	virtual_ethdev_tx_burst_fn_set_success(
			test_params->slave_port_ids[2],
			0);

	virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(
			test_params->slave_port_ids[0],
			TEST_BCAST_SLAVE_TX_FAIL_MAX_PACKETS_COUNT);

	virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(
			test_params->slave_port_ids[1],
			TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT);

	virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(
			test_params->slave_port_ids[2],
			TEST_BCAST_SLAVE_TX_FAIL_MAX_PACKETS_COUNT);

	/* Transmit burst */
	tx_count = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst,
			TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE);

	TEST_ASSERT_EQUAL(tx_count, TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT,
			"Transmitted (%d) packets, expected to transmit (%d) packets",
			tx_count, TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT);

	/* Verify that failed packet are expected failed packets */
	for (i = 0; i < TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT; i++) {
		TEST_ASSERT_EQUAL(expected_fail_pkts[i], pkts_burst[i + tx_count],
				"expected mbuf (%d) pointer %p not expected pointer %p",
				i, expected_fail_pkts[i], pkts_burst[i + tx_count]);
	}

	/* Verify slave ports tx stats */

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_BCAST_SLAVE_TX_FAIL_MAX_PACKETS_COUNT,
			"Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.opackets,
			TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_BCAST_SLAVE_TX_FAIL_MAX_PACKETS_COUNT);


	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT,
			"Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.opackets,
			TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT);

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);

	TEST_ASSERT_EQUAL(port_stats.opackets,
			(uint64_t)TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_BCAST_SLAVE_TX_FAIL_MAX_PACKETS_COUNT,
			"Port (%d) opackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.opackets,
			TEST_BCAST_SLAVE_TX_FAIL_BURST_SIZE -
			TEST_BCAST_SLAVE_TX_FAIL_MAX_PACKETS_COUNT);


	/* Verify that all mbufs who transmission failed have a ref value of one */
	TEST_ASSERT_SUCCESS(verify_mbufs_ref_count(&pkts_burst[tx_count],
			TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT, 1),
			"mbufs refcnts not as expected");

	free_mbufs(&pkts_burst[tx_count],
		TEST_BCAST_SLAVE_TX_FAIL_MIN_PACKETS_COUNT);

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

#define BROADCAST_RX_BURST_NUM_OF_SLAVES (3)

static int
test_broadcast_rx_burst(void)
{
	struct rte_mbuf *gen_pkt_burst[BROADCAST_RX_BURST_NUM_OF_SLAVES][MAX_PKT_BURST];

	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	int burst_size[BROADCAST_RX_BURST_NUM_OF_SLAVES] = { 10, 5, 30 };
	int i, j;

	memset(gen_pkt_burst, 0, sizeof(gen_pkt_burst));

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BROADCAST, 0, 3, 1),
			"Failed to initialise bonded device");

	/* Generate test bursts of packets to transmit */
	for (i = 0; i < BROADCAST_RX_BURST_NUM_OF_SLAVES; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[i][0], burst_size[i], 0, 0, 1, 0, 0),
				burst_size[i], "failed to generate packet burst");
	}

	/* Add rx data to slave 0 */
	for (i = 0; i < BROADCAST_RX_BURST_NUM_OF_SLAVES; i++) {
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[i][0], burst_size[i]);
	}


	/* Call rx burst on bonded device */
	/* Send burst on bonded port */
	TEST_ASSERT_EQUAL(rte_eth_rx_burst(
			test_params->bonded_port_id, 0, rx_pkt_burst, MAX_PKT_BURST),
			burst_size[0] + burst_size[1] + burst_size[2],
			"rx burst failed");

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets,
			(uint64_t)(burst_size[0] + burst_size[1] + burst_size[2]),
			"Bonded Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->bonded_port_id, (unsigned int)port_stats.ipackets,
			burst_size[0] + burst_size[1] + burst_size[2]);


	/* Verify bonded slave devices rx counts */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[0],
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[0], (unsigned int)port_stats.ipackets,
			burst_size[0]);

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[1],
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[0], (unsigned int)port_stats.ipackets,
			burst_size[1]);

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size[2],
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[2], (unsigned int)port_stats.ipackets,
			burst_size[2]);

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, 0,
			"Slave Port (%d) ipackets value (%u) not as expected (%d)",
			test_params->slave_port_ids[3], (unsigned int)port_stats.ipackets,
			0);

	/* free mbufs allocate for rx testing */
	for (i = 0; i < BROADCAST_RX_BURST_NUM_OF_SLAVES; i++) {
		for (j = 0; j < MAX_PKT_BURST; j++) {
			if (gen_pkt_burst[i][j] != NULL) {
				rte_pktmbuf_free(gen_pkt_burst[i][j]);
				gen_pkt_burst[i][j] = NULL;
			}
		}
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_broadcast_verify_promiscuous_enable_disable(void)
{
	int i;
	int ret;

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BROADCAST, 0, 4, 1),
			"Failed to initialise bonded device");

	ret = rte_eth_promiscuous_enable(test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to enable promiscuous mode for port %d: %s",
		test_params->bonded_port_id, rte_strerror(-ret));


	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonded_port_id), 1,
			"Port (%d) promiscuous mode not enabled",
			test_params->bonded_port_id);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]), 1,
				"Port (%d) promiscuous mode not enabled",
				test_params->slave_port_ids[i]);
	}

	ret = rte_eth_promiscuous_disable(test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to disable promiscuous mode for port %d: %s",
		test_params->bonded_port_id, rte_strerror(-ret));

	TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(test_params->bonded_port_id), 0,
			"Port (%d) promiscuous mode not disabled",
			test_params->bonded_port_id);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_EQUAL(rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]), 0,
				"Port (%d) promiscuous mode not disabled",
				test_params->slave_port_ids[i]);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_broadcast_verify_mac_assignment(void)
{
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr expected_mac_addr_0, expected_mac_addr_1;

	int i;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &expected_mac_addr_0),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[2], &expected_mac_addr_1),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[2]);

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_BROADCAST, 0, 4, 1),
			"Failed to initialise bonded device");

	/* Verify that all MACs are the same as first slave added to bonded
	 * device */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->slave_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr)),
				"slave port (%d) mac address not set to that of primary port",
				test_params->slave_port_ids[i]);
	}

	/* change primary and verify that MAC addresses haven't changed */
	TEST_ASSERT_SUCCESS(rte_eth_bond_primary_set(test_params->bonded_port_id,
			test_params->slave_port_ids[2]),
			"Failed to set bonded port (%d) primary port to (%d)",
			test_params->bonded_port_id, test_params->slave_port_ids[i]);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->slave_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr)),
				"slave port (%d) mac address has changed to that of primary "
				"port without stop/start toggle of bonded device",
				test_params->slave_port_ids[i]);
	}

	/* stop / start bonded device and verify that primary MAC address is
	 * propagated to bonded device and slaves */

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonded_port_id),
			"Failed to stop bonded port %u",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonded_port_id),
			"Failed to start bonded device");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of new primary  port",
			test_params->slave_port_ids[i]);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->slave_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
				sizeof(read_mac_addr)),
				"slave port (%d) mac address not set to that of new primary "
				"port", test_params->slave_port_ids[i]);
	}

	/* Set explicit MAC address */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonded_port_id,
			(struct rte_ether_addr *)bonded_mac),
			"Failed to set MAC address");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(bonded_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of new primary port",
			test_params->slave_port_ids[i]);


	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr),
				"Failed to get mac address (port %d)",
				test_params->slave_port_ids[i]);
		TEST_ASSERT_SUCCESS(memcmp(bonded_mac, &read_mac_addr,
				sizeof(read_mac_addr)),
				"slave port (%d) mac address not set to that of new primary "
				"port", test_params->slave_port_ids[i]);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

#define BROADCAST_LINK_STATUS_NUM_OF_SLAVES (4)
static int
test_broadcast_verify_slave_link_status_change_behaviour(void)
{
	struct rte_mbuf *pkt_burst[BROADCAST_LINK_STATUS_NUM_OF_SLAVES][MAX_PKT_BURST];
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	uint16_t slaves[RTE_MAX_ETHPORTS];

	int i, burst_size, slave_count;

	memset(pkt_burst, 0, sizeof(pkt_burst));

	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
				BONDING_MODE_BROADCAST, 0, BROADCAST_LINK_STATUS_NUM_OF_SLAVES,
				1), "Failed to initialise bonded device");

	/* Verify Current Slaves Count /Active Slave Count is */
	slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count, 4,
			"Number of slaves (%d) is not as expected (%d).",
			slave_count, 4);

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count, 4,
			"Number of active slaves (%d) is not as expected (%d).",
			slave_count, 4);

	/* Set 2 slaves link status to down */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 0);

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count, 2,
			"Number of active slaves (%d) is not as expected (%d).",
			slave_count, 2);

	for (i = 0; i < test_params->bonded_slave_count; i++)
		rte_eth_stats_reset(test_params->slave_port_ids[i]);

	/* Verify that pkts are not sent on slaves with link status down */
	burst_size = 21;

	TEST_ASSERT_EQUAL(generate_test_burst(
			&pkt_burst[0][0], burst_size, 0, 0, 1, 0, 0), burst_size,
			"generate_test_burst failed");

	TEST_ASSERT_EQUAL(rte_eth_tx_burst(test_params->bonded_port_id, 0,
			&pkt_burst[0][0], burst_size), burst_size,
			"rte_eth_tx_burst failed\n");

	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)(burst_size * slave_count),
			"(%d) port_stats.opackets (%d) not as expected (%d)\n",
			test_params->bonded_port_id, (int)port_stats.opackets,
			burst_size * slave_count);

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets not as expected",
			test_params->slave_port_ids[0]);

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected",
				test_params->slave_port_ids[1]);

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (uint64_t)burst_size,
			"(%d) port_stats.opackets not as expected",
				test_params->slave_port_ids[2]);


	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, 0,
			"(%d) port_stats.opackets not as expected",
			test_params->slave_port_ids[3]);


	for (i = 0; i < BROADCAST_LINK_STATUS_NUM_OF_SLAVES; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&pkt_burst[i][0], burst_size, 0, 0, 1, 0, 0),
				burst_size, "failed to generate packet burst");

		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&pkt_burst[i][0], burst_size);
	}

	/* Verify that pkts are not received on slaves with link status down */
	TEST_ASSERT_EQUAL(rte_eth_rx_burst(
			test_params->bonded_port_id, 0, rx_pkt_burst, MAX_PKT_BURST),
			burst_size + burst_size, "rte_eth_rx_burst failed");


	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)(burst_size + burst_size),
			"(%d) port_stats.ipackets not as expected\n",
			test_params->bonded_port_id);

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_reconfigure_bonded_device(void)
{
	test_params->nb_rx_q = 4;
	test_params->nb_tx_q = 4;

	TEST_ASSERT_SUCCESS(configure_ethdev(test_params->bonded_port_id, 0, 0),
			"failed to reconfigure bonded device");

	test_params->nb_rx_q = 2;
	test_params->nb_tx_q = 2;

	TEST_ASSERT_SUCCESS(configure_ethdev(test_params->bonded_port_id, 0, 0),
			"failed to reconfigure bonded device with less rx/tx queues");

	return 0;
}


static int
test_close_bonded_device(void)
{
	rte_eth_dev_close(test_params->bonded_port_id);
	return 0;
}

static void
testsuite_teardown(void)
{
	free(test_params->pkt_eth_hdr);
	test_params->pkt_eth_hdr = NULL;

	/* Clean up and remove slaves from bonded device */
	remove_slaves_and_stop_bonded_device();
}

static void
free_virtualpmd_tx_queue(void)
{
	int i, slave_port, to_free_cnt;
	struct rte_mbuf *pkts_to_free[MAX_PKT_BURST];

	/* Free tx queue of virtual pmd */
	for (slave_port = 0; slave_port < test_params->bonded_slave_count;
			slave_port++) {
		to_free_cnt = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->slave_port_ids[slave_port],
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

	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves
			(BONDING_MODE_TLB, 1, 3, 1),
			"Failed to initialise bonded device");

	burst_size = 20 * test_params->bonded_slave_count;

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
					(struct rte_ether_addr *)test_params->default_slave_mac,
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
		/* Send burst on bonded port */
		nb_tx = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkt_burst,
				burst_size);
		nb_tx2 += nb_tx;

		free_virtualpmd_tx_queue();

		TEST_ASSERT_EQUAL(nb_tx, burst_size,
				"number of packet not equal burst size");

		rte_delay_us(5);
	}


	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats[0]);

	all_bond_opackets = port_stats[0].opackets;
	all_bond_obytes = port_stats[0].obytes;

	TEST_ASSERT_EQUAL(port_stats[0].opackets, (uint64_t)nb_tx2,
			"Bonded Port (%d) opackets value (%u) not as expected (%d)\n",
			test_params->bonded_port_id, (unsigned int)port_stats[0].opackets,
			burst_size);


	/* Verify slave ports tx stats */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_stats_get(test_params->slave_port_ids[i], &port_stats[i]);
		sum_ports_opackets += port_stats[i].opackets;
	}

	TEST_ASSERT_EQUAL(sum_ports_opackets, (uint64_t)all_bond_opackets,
			"Total packets sent by slaves is not equal to packets sent by bond interface");

	/* checking if distribution of packets is balanced over slaves */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		TEST_ASSERT(port_stats[i].obytes > 0 &&
				port_stats[i].obytes < all_bond_obytes,
						"Packets are not balanced over slaves");
	}

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	nb_tx = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkt_burst,
			burst_size);
	TEST_ASSERT_EQUAL(nb_tx, 0, " bad number of packet in burst");

	/* Clean ugit checkout masterp and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

#define TEST_ADAPTIVE_TRANSMIT_LOAD_BALANCING_RX_BURST_SLAVE_COUNT (4)

static int
test_tlb_rx_burst(void)
{
	struct rte_mbuf *gen_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };

	struct rte_eth_stats port_stats;

	int primary_port;

	uint16_t i, j, nb_rx, burst_size = 17;

	/* Initialize bonded device with 4 slaves in transmit load balancing mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_TLB,
			TEST_ADAPTIVE_TRANSMIT_LOAD_BALANCING_RX_BURST_SLAVE_COUNT, 1, 1),
			"Failed to initialize bonded device");


	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	TEST_ASSERT(primary_port >= 0,
			"failed to get primary slave for bonded port (%d)",
			test_params->bonded_port_id);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		/* Generate test bursts of packets to transmit */
		TEST_ASSERT_EQUAL(generate_test_burst(
				&gen_pkt_burst[0], burst_size, 0, 1, 0, 0, 0), burst_size,
				"burst generation failed");

		/* Add rx data to slave */
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[0], burst_size);

		/* Call rx burst on bonded device */
		nb_rx = rte_eth_rx_burst(test_params->bonded_port_id, 0,
				&rx_pkt_burst[0], MAX_PKT_BURST);

		TEST_ASSERT_EQUAL(nb_rx, burst_size, "rte_eth_rx_burst failed\n");

		if (test_params->slave_port_ids[i] == primary_port) {
			/* Verify bonded device rx count */
			rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
			TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
					"Bonded Port (%d) ipackets value (%u) not as expected (%d)\n",
					test_params->bonded_port_id,
					(unsigned int)port_stats.ipackets, burst_size);

			/* Verify bonded slave devices rx count */
			for (j = 0; j < test_params->bonded_slave_count; j++) {
				rte_eth_stats_get(test_params->slave_port_ids[j], &port_stats);
				if (i == j) {
					TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
							"Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
							test_params->slave_port_ids[i],
							(unsigned int)port_stats.ipackets, burst_size);
				} else {
					TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)0,
							"Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
							test_params->slave_port_ids[i],
							(unsigned int)port_stats.ipackets, 0);
				}
			}
		} else {
			for (j = 0; j < test_params->bonded_slave_count; j++) {
				rte_eth_stats_get(test_params->slave_port_ids[j], &port_stats);
				TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)0,
						"Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
						test_params->slave_port_ids[i],
						(unsigned int)port_stats.ipackets, 0);
			}
		}

		/* free mbufs */
		for (i = 0; i < burst_size; i++)
			rte_pktmbuf_free(rx_pkt_burst[i]);

		/* reset bonded device stats */
		rte_eth_stats_reset(test_params->bonded_port_id);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_tlb_verify_promiscuous_enable_disable(void)
{
	int i, primary_port, promiscuous_en;
	int ret;

	/* Initialize bonded device with 4 slaves in transmit load balancing mode */
	TEST_ASSERT_SUCCESS( initialize_bonded_device_with_slaves(
			BONDING_MODE_TLB, 0, 4, 1),
			"Failed to initialize bonded device");

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	TEST_ASSERT(primary_port >= 0,
			"failed to get primary slave for bonded port (%d)",
			test_params->bonded_port_id);

	ret = rte_eth_promiscuous_enable(test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to enable promiscuous mode for port %d: %s",
		test_params->bonded_port_id, rte_strerror(-ret));

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	TEST_ASSERT_EQUAL(promiscuous_en, (int)1,
			"Port (%d) promiscuous mode not enabled\n",
			test_params->bonded_port_id);
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		if (primary_port == test_params->slave_port_ids[i]) {
			TEST_ASSERT_EQUAL(promiscuous_en, (int)1,
					"Port (%d) promiscuous mode not enabled\n",
					test_params->bonded_port_id);
		} else {
			TEST_ASSERT_EQUAL(promiscuous_en, (int)0,
					"Port (%d) promiscuous mode enabled\n",
					test_params->bonded_port_id);
		}

	}

	ret = rte_eth_promiscuous_disable(test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed to disable promiscuous mode for port %d: %s\n",
		test_params->bonded_port_id, rte_strerror(-ret));

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	TEST_ASSERT_EQUAL(promiscuous_en, (int)0,
			"Port (%d) promiscuous mode not disabled\n",
			test_params->bonded_port_id);

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		TEST_ASSERT_EQUAL(promiscuous_en, (int)0,
				"slave port (%d) promiscuous mode not disabled\n",
				test_params->slave_port_ids[i]);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_tlb_verify_mac_assignment(void)
{
	struct rte_ether_addr read_mac_addr;
	struct rte_ether_addr expected_mac_addr_0, expected_mac_addr_1;

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &expected_mac_addr_0),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &expected_mac_addr_1),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);

	/* Initialize bonded device with 2 slaves in active backup mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_TLB, 0, 2, 1),
			"Failed to initialize bonded device");

	/* Verify that bonded MACs is that of first slave and that the other slave
	 * MAC hasn't been changed */
	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of primary port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not as expected",
			test_params->slave_port_ids[1]);

	/* change primary and verify that MAC addresses haven't changed */
	TEST_ASSERT_EQUAL(rte_eth_bond_primary_set(test_params->bonded_port_id,
			test_params->slave_port_ids[1]), 0,
			"Failed to set bonded port (%d) primary port to (%d)",
			test_params->bonded_port_id, test_params->slave_port_ids[1]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of primary port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not as expected",
			test_params->slave_port_ids[1]);

	/* stop / start bonded device and verify that primary MAC address is
	 * propagated to bonded device and slaves */

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params->bonded_port_id),
			"Failed to stop bonded port %u",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params->bonded_port_id),
			"Failed to start device");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of primary port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not as expected",
			test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_1, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of primary port",
			test_params->slave_port_ids[1]);


	/* Set explicit MAC address */
	TEST_ASSERT_SUCCESS(rte_eth_bond_mac_address_set(
			test_params->bonded_port_id,
			(struct rte_ether_addr *)bonded_mac),
			"failed to set MAC address");

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->bonded_port_id);
	TEST_ASSERT_SUCCESS(memcmp(&bonded_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"bonded port (%d) mac address not set to that of bonded port",
			test_params->bonded_port_id);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[0]);
	TEST_ASSERT_SUCCESS(memcmp(&expected_mac_addr_0, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not as expected",
			test_params->slave_port_ids[0]);

	TEST_ASSERT_SUCCESS(rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr),
			"Failed to get mac address (port %d)",
			test_params->slave_port_ids[1]);
	TEST_ASSERT_SUCCESS(memcmp(&bonded_mac, &read_mac_addr,
			sizeof(read_mac_addr)),
			"slave port (%d) mac address not set to that of bonded port",
			test_params->slave_port_ids[1]);

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_tlb_verify_slave_link_status_change_failover(void)
{
	struct rte_mbuf *pkt_burst[TEST_ADAPTIVE_TRANSMIT_LOAD_BALANCING_RX_BURST_SLAVE_COUNT][MAX_PKT_BURST];
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	uint16_t slaves[RTE_MAX_ETHPORTS];

	int i, burst_size, slave_count, primary_port;

	burst_size = 21;

	memset(pkt_burst, 0, sizeof(pkt_burst));



	/* Initialize bonded device with 4 slaves in round robin mode */
	TEST_ASSERT_SUCCESS(initialize_bonded_device_with_slaves(
			BONDING_MODE_TLB, 0,
			TEST_ADAPTIVE_TRANSMIT_LOAD_BALANCING_RX_BURST_SLAVE_COUNT, 1),
			"Failed to initialize bonded device with slaves");

	/* Verify Current Slaves Count /Active Slave Count is */
	slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count, 4,
			"Number of slaves (%d) is not as expected (%d).\n",
			slave_count, 4);

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	TEST_ASSERT_EQUAL(slave_count, (int)4,
			"Number of slaves (%d) is not as expected (%d).\n",
			slave_count, 4);

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	TEST_ASSERT_EQUAL(primary_port, test_params->slave_port_ids[0],
			"Primary port not as expected");

	/* Bring 2 slaves down and verify active slave count */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS), 2,
			"Number of active slaves (%d) is not as expected (%d).",
			slave_count, 2);

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 1);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 1);


	/* Bring primary port down, verify that active slave count is 3 and primary
	 *  has changed */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[0], 0);

	TEST_ASSERT_EQUAL(rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS), 3,
			"Number of active slaves (%d) is not as expected (%d).",
			slave_count, 3);

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	TEST_ASSERT_EQUAL(primary_port, test_params->slave_port_ids[2],
			"Primary port not as expected");
	rte_delay_us(500000);
	/* Verify that pkts are sent on new primary slave */
	for (i = 0; i < 4; i++) {
		TEST_ASSERT_EQUAL(generate_test_burst(
				&pkt_burst[0][0], burst_size, 0, 1, 0, 0, 0), burst_size,
				"generate_test_burst failed\n");
		TEST_ASSERT_EQUAL(rte_eth_tx_burst(
				test_params->bonded_port_id, 0, &pkt_burst[0][0], burst_size), burst_size,
				"rte_eth_tx_burst failed\n");
		rte_delay_us(11000);
	}

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	TEST_ASSERT_EQUAL(port_stats.opackets, (int8_t)0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->slave_port_ids[0]);

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	TEST_ASSERT_NOT_EQUAL(port_stats.opackets, (int8_t)0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->slave_port_ids[1]);

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	TEST_ASSERT_NOT_EQUAL(port_stats.opackets, (int8_t)0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->slave_port_ids[2]);

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	TEST_ASSERT_NOT_EQUAL(port_stats.opackets, (int8_t)0,
			"(%d) port_stats.opackets not as expected\n",
			test_params->slave_port_ids[3]);


	/* Generate packet burst for testing */

	for (i = 0; i < TEST_ADAPTIVE_TRANSMIT_LOAD_BALANCING_RX_BURST_SLAVE_COUNT; i++) {
		if (generate_test_burst(&pkt_burst[i][0], burst_size, 0, 1, 0, 0, 0) !=
				burst_size)
			return -1;

		virtual_ethdev_add_mbufs_to_rx_queue(
				test_params->slave_port_ids[i], &pkt_burst[i][0], burst_size);
	}

	if (rte_eth_rx_burst(test_params->bonded_port_id, 0, rx_pkt_burst,
			MAX_PKT_BURST) != burst_size) {
		printf("rte_eth_rx_burst\n");
		return -1;

	}

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	TEST_ASSERT_EQUAL(port_stats.ipackets, (uint64_t)burst_size,
			"(%d) port_stats.ipackets not as expected\n",
			test_params->bonded_port_id);

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

#define TEST_ALB_SLAVE_COUNT	2

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

	int slave_idx, nb_pkts, pkt_idx;
	int retval = 0;

	struct rte_ether_addr bond_mac, client_mac;
	struct rte_ether_addr *slave_mac1, *slave_mac2;

	TEST_ASSERT_SUCCESS(
			initialize_bonded_device_with_slaves(BONDING_MODE_ALB,
					0, TEST_ALB_SLAVE_COUNT, 1),
			"Failed to initialize_bonded_device_with_slaves.");

	/* Flush tx queue */
	rte_eth_tx_burst(test_params->bonded_port_id, 0, NULL, 0);
	for (slave_idx = 0; slave_idx < test_params->bonded_slave_count;
			slave_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->slave_port_ids[slave_idx], pkts_sent,
				MAX_PKT_BURST);
	}

	rte_ether_addr_copy(
			rte_eth_devices[test_params->bonded_port_id].data->mac_addrs,
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
	rte_eth_tx_burst(test_params->bonded_port_id, 0, &pkt, 1);

	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client2, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_ARP, 0, 0);
	arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
					sizeof(struct rte_ether_hdr));
	initialize_arp_header(arp_pkt, &bond_mac, &client_mac, ip_host, ip_client2,
			RTE_ARP_OP_REPLY);
	rte_eth_tx_burst(test_params->bonded_port_id, 0, &pkt, 1);

	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client3, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_ARP, 0, 0);
	arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
					sizeof(struct rte_ether_hdr));
	initialize_arp_header(arp_pkt, &bond_mac, &client_mac, ip_host, ip_client3,
			RTE_ARP_OP_REPLY);
	rte_eth_tx_burst(test_params->bonded_port_id, 0, &pkt, 1);

	pkt = rte_pktmbuf_alloc(test_params->mbuf_pool);
	memcpy(client_mac.addr_bytes, mac_client4, RTE_ETHER_ADDR_LEN);
	eth_pkt = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	initialize_eth_header(eth_pkt, &bond_mac, &client_mac,
			RTE_ETHER_TYPE_ARP, 0, 0);
	arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
					sizeof(struct rte_ether_hdr));
	initialize_arp_header(arp_pkt, &bond_mac, &client_mac, ip_host, ip_client4,
			RTE_ARP_OP_REPLY);
	rte_eth_tx_burst(test_params->bonded_port_id, 0, &pkt, 1);

	slave_mac1 =
			rte_eth_devices[test_params->slave_port_ids[0]].data->mac_addrs;
	slave_mac2 =
			rte_eth_devices[test_params->slave_port_ids[1]].data->mac_addrs;

	/*
	 * Checking if packets are properly distributed on bonding ports. Packets
	 * 0 and 2 should be sent on port 0 and packets 1 and 3 on port 1.
	 */
	for (slave_idx = 0; slave_idx < test_params->bonded_slave_count; slave_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->slave_port_ids[slave_idx], pkts_sent,
				MAX_PKT_BURST);

		for (pkt_idx = 0; pkt_idx < nb_pkts; pkt_idx++) {
			eth_pkt = rte_pktmbuf_mtod(
				pkts_sent[pkt_idx], struct rte_ether_hdr *);
			arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
						sizeof(struct rte_ether_hdr));

			if (slave_idx%2 == 0) {
				if (!rte_is_same_ether_addr(slave_mac1,
						&arp_pkt->arp_data.arp_sha)) {
					retval = -1;
					goto test_end;
				}
			} else {
				if (!rte_is_same_ether_addr(slave_mac2,
						&arp_pkt->arp_data.arp_sha)) {
					retval = -1;
					goto test_end;
				}
			}
		}
	}

test_end:
	retval += remove_slaves_and_stop_bonded_device();
	return retval;
}

static int
test_alb_reply_from_client(void)
{
	struct rte_ether_hdr *eth_pkt;
	struct rte_arp_hdr *arp_pkt;

	struct rte_mbuf *pkt;
	struct rte_mbuf *pkts_sent[MAX_PKT_BURST];

	int slave_idx, nb_pkts, pkt_idx, nb_pkts_sum = 0;
	int retval = 0;

	struct rte_ether_addr bond_mac, client_mac;
	struct rte_ether_addr *slave_mac1, *slave_mac2;

	TEST_ASSERT_SUCCESS(
			initialize_bonded_device_with_slaves(BONDING_MODE_ALB,
					0, TEST_ALB_SLAVE_COUNT, 1),
			"Failed to initialize_bonded_device_with_slaves.");

	/* Flush tx queue */
	rte_eth_tx_burst(test_params->bonded_port_id, 0, NULL, 0);
	for (slave_idx = 0; slave_idx < test_params->bonded_slave_count; slave_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->slave_port_ids[slave_idx], pkts_sent,
				MAX_PKT_BURST);
	}

	rte_ether_addr_copy(
			rte_eth_devices[test_params->bonded_port_id].data->mac_addrs,
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
	virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[0], &pkt,
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
	virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[0], &pkt,
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
	virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[0], &pkt,
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
	virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[0], &pkt,
			1);

	/*
	 * Issue rx_burst and tx_burst to force bonding driver to send update ARP
	 * packets to every client in alb table.
	 */
	rte_eth_rx_burst(test_params->bonded_port_id, 0, pkts_sent, MAX_PKT_BURST);
	rte_eth_tx_burst(test_params->bonded_port_id, 0, NULL, 0);

	slave_mac1 = rte_eth_devices[test_params->slave_port_ids[0]].data->mac_addrs;
	slave_mac2 = rte_eth_devices[test_params->slave_port_ids[1]].data->mac_addrs;

	/*
	 * Checking if update ARP packets were properly send on slave ports.
	 */
	for (slave_idx = 0; slave_idx < test_params->bonded_slave_count; slave_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->slave_port_ids[slave_idx], pkts_sent, MAX_PKT_BURST);
		nb_pkts_sum += nb_pkts;

		for (pkt_idx = 0; pkt_idx < nb_pkts; pkt_idx++) {
			eth_pkt = rte_pktmbuf_mtod(
				pkts_sent[pkt_idx], struct rte_ether_hdr *);
			arp_pkt = (struct rte_arp_hdr *)((char *)eth_pkt +
						sizeof(struct rte_ether_hdr));

			if (slave_idx%2 == 0) {
				if (!rte_is_same_ether_addr(slave_mac1,
						&arp_pkt->arp_data.arp_sha)) {
					retval = -1;
					goto test_end;
				}
			} else {
				if (!rte_is_same_ether_addr(slave_mac2,
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
	retval += remove_slaves_and_stop_bonded_device();
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

	int slave_idx, nb_pkts, pkt_idx;
	int retval = 0;

	struct rte_ether_addr bond_mac, client_mac;

	TEST_ASSERT_SUCCESS(
			initialize_bonded_device_with_slaves(BONDING_MODE_ALB,
					0, TEST_ALB_SLAVE_COUNT, 1),
			"Failed to initialize_bonded_device_with_slaves.");

	/* Flush tx queue */
	rte_eth_tx_burst(test_params->bonded_port_id, 0, NULL, 0);
	for (slave_idx = 0; slave_idx < test_params->bonded_slave_count; slave_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->slave_port_ids[slave_idx], pkts_sent,
				MAX_PKT_BURST);
	}

	rte_ether_addr_copy(
			rte_eth_devices[test_params->bonded_port_id].data->mac_addrs,
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
	virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[0], &pkt,
			1);

	rte_eth_rx_burst(test_params->bonded_port_id, 0, pkts_sent, MAX_PKT_BURST);
	rte_eth_tx_burst(test_params->bonded_port_id, 0, NULL, 0);

	/*
	 * Checking if VLAN headers in generated ARP Update packet are correct.
	 */
	for (slave_idx = 0; slave_idx < test_params->bonded_slave_count; slave_idx++) {
		nb_pkts = virtual_ethdev_get_mbufs_from_tx_queue(
				test_params->slave_port_ids[slave_idx], pkts_sent,
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
	retval += remove_slaves_and_stop_bonded_device();
	return retval;
}

static int
test_alb_ipv4_tx(void)
{
	int burst_size, retval, pkts_send;
	struct rte_mbuf *pkt_burst[MAX_PKT_BURST];

	retval = 0;

	TEST_ASSERT_SUCCESS(
			initialize_bonded_device_with_slaves(BONDING_MODE_ALB,
					0, TEST_ALB_SLAVE_COUNT, 1),
			"Failed to initialize_bonded_device_with_slaves.");

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
			test_params->bonded_port_id, 0, pkt_burst, burst_size);
	if (pkts_send != burst_size) {
		retval = -1;
		goto test_end;
	}

test_end:
	retval += remove_slaves_and_stop_bonded_device();
	return retval;
}

static struct unit_test_suite link_bonding_test_suite  = {
	.suite_name = "Link Bonding Unit Test Suite",
	.setup = test_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE(test_create_bonded_device),
		TEST_CASE(test_create_bonded_device_with_invalid_params),
		TEST_CASE(test_add_slave_to_bonded_device),
		TEST_CASE(test_add_slave_to_invalid_bonded_device),
		TEST_CASE(test_remove_slave_from_bonded_device),
		TEST_CASE(test_remove_slave_from_invalid_bonded_device),
		TEST_CASE(test_get_slaves_from_bonded_device),
		TEST_CASE(test_add_already_bonded_slave_to_bonded_device),
		TEST_CASE(test_add_remove_multiple_slaves_to_from_bonded_device),
		TEST_CASE(test_start_bonded_device),
		TEST_CASE(test_stop_bonded_device),
		TEST_CASE(test_set_bonding_mode),
		TEST_CASE(test_set_primary_slave),
		TEST_CASE(test_set_explicit_bonded_mac),
		TEST_CASE(test_set_bonded_port_initialization_mac_assignment),
		TEST_CASE(test_status_interrupt),
		TEST_CASE(test_adding_slave_after_bonded_device_started),
		TEST_CASE(test_roundrobin_tx_burst),
		TEST_CASE(test_roundrobin_tx_burst_slave_tx_fail),
		TEST_CASE(test_roundrobin_rx_burst_on_single_slave),
		TEST_CASE(test_roundrobin_rx_burst_on_multiple_slaves),
		TEST_CASE(test_roundrobin_verify_promiscuous_enable_disable),
		TEST_CASE(test_roundrobin_verify_mac_assignment),
		TEST_CASE(test_roundrobin_verify_slave_link_status_change_behaviour),
		TEST_CASE(test_roundrobin_verfiy_polling_slave_link_status_change),
		TEST_CASE(test_activebackup_tx_burst),
		TEST_CASE(test_activebackup_rx_burst),
		TEST_CASE(test_activebackup_verify_promiscuous_enable_disable),
		TEST_CASE(test_activebackup_verify_mac_assignment),
		TEST_CASE(test_activebackup_verify_slave_link_status_change_failover),
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
		TEST_CASE(test_balance_tx_burst_slave_tx_fail),
		TEST_CASE(test_balance_rx_burst),
		TEST_CASE(test_balance_verify_promiscuous_enable_disable),
		TEST_CASE(test_balance_verify_mac_assignment),
		TEST_CASE(test_balance_verify_slave_link_status_change_behaviour),
		TEST_CASE(test_tlb_tx_burst),
		TEST_CASE(test_tlb_rx_burst),
		TEST_CASE(test_tlb_verify_mac_assignment),
		TEST_CASE(test_tlb_verify_promiscuous_enable_disable),
		TEST_CASE(test_tlb_verify_slave_link_status_change_failover),
		TEST_CASE(test_alb_change_mac_in_reply_sent),
		TEST_CASE(test_alb_reply_from_client),
		TEST_CASE(test_alb_receive_vlan_reply),
		TEST_CASE(test_alb_ipv4_tx),
		TEST_CASE(test_broadcast_tx_burst),
		TEST_CASE(test_broadcast_tx_burst_slave_tx_fail),
		TEST_CASE(test_broadcast_rx_burst),
		TEST_CASE(test_broadcast_verify_promiscuous_enable_disable),
		TEST_CASE(test_broadcast_verify_mac_assignment),
		TEST_CASE(test_broadcast_verify_slave_link_status_change_behaviour),
		TEST_CASE(test_reconfigure_bonded_device),
		TEST_CASE(test_close_bonded_device),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};


static int
test_link_bonding(void)
{
	return unit_test_suite_runner(&link_bonding_test_suite);
}

REGISTER_TEST_COMMAND(link_bonding_autotest, test_link_bonding);
