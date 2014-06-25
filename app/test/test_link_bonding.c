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

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_string_fns.h>
#include <rte_eth_bond.h>

#include "virtual_pmd.h"
#include "packet_burst_generator.h"

#include "test.h"

#define TEST_MAX_NUMBER_OF_PORTS (16)

#define RX_RING_SIZE 128
#define RX_FREE_THRESH 32
#define RX_PTHRESH 8
#define RX_HTHRESH 8
#define RX_WTHRESH 0

#define TX_RING_SIZE 512
#define TX_FREE_THRESH 32
#define TX_PTHRESH 32
#define TX_HTHRESH 0
#define TX_WTHRESH 0
#define TX_RSBIT_THRESH 32
#define TX_Q_FLAGS (ETH_TXQ_FLAGS_NOMULTSEGS | ETH_TXQ_FLAGS_NOVLANOFFL |\
	ETH_TXQ_FLAGS_NOXSUMSCTP | ETH_TXQ_FLAGS_NOXSUMUDP | \
	ETH_TXQ_FLAGS_NOXSUMTCP)

#define MBUF_PAYLOAD_SIZE	(2048)
#define MBUF_SIZE (MBUF_PAYLOAD_SIZE + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE (250)
#define BURST_SIZE (32)

#define DEFAULT_MBUF_DATA_SIZE	(2048)
#define RTE_TEST_RX_DESC_MAX	(2048)
#define RTE_TEST_TX_DESC_MAX	(2048)
#define MAX_PKT_BURST			(512)
#define DEF_PKT_BURST			(16)

#define BONDED_DEV_NAME			("unit_test_bonded_device")

#define INVALID_SOCKET_ID		(-1)
#define INVALID_PORT_ID			(-1)
#define INVALID_BONDING_MODE	(-1)


uint8_t slave_mac[] = {0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00 };
uint8_t bonded_mac[] = {0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF };

struct link_bonding_unittest_params {
	int8_t bonded_port_id;
	int8_t slave_port_ids[TEST_MAX_NUMBER_OF_PORTS];
	uint8_t bonded_slave_count;
	uint8_t bonding_mode;

	uint16_t nb_rx_q;
	uint16_t nb_tx_q;

	struct rte_mempool *mbuf_pool;

	struct ether_addr *default_slave_mac;
	struct ether_addr *default_bonded_mac;

	/* Packet Headers */
	struct ether_hdr *pkt_eth_hdr;
	struct ipv4_hdr *pkt_ipv4_hdr;
	struct ipv6_hdr *pkt_ipv6_hdr;
	struct udp_hdr *pkt_udp_hdr;

};

static struct ipv4_hdr pkt_ipv4_hdr;
static struct ipv6_hdr pkt_ipv6_hdr;
static struct udp_hdr pkt_udp_hdr;

static struct link_bonding_unittest_params default_params  = {
	.bonded_port_id = -1,
	.slave_port_ids = { 0 },
	.bonded_slave_count = 0,
	.bonding_mode = BONDING_MODE_ROUND_ROBIN,

	.nb_rx_q = 1,
	.nb_tx_q = 1,

	.mbuf_pool = NULL,

	.default_slave_mac = (struct ether_addr *)slave_mac,
	.default_bonded_mac = (struct ether_addr *)bonded_mac,

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

struct rte_eth_rxmode rx_mode = {
	.max_rx_pkt_len = ETHER_MAX_LEN, /**< Default maximum frame length. */
	.split_hdr_size = 0,
	.header_split   = 0, /**< Header Split disabled. */
	.hw_ip_checksum = 0, /**< IP checksum offload disabled. */
	.hw_vlan_filter = 1, /**< VLAN filtering enabled. */
	.hw_vlan_strip  = 1, /**< VLAN strip enabled. */
	.hw_vlan_extend = 0, /**< Extended VLAN disabled. */
	.jumbo_frame    = 0, /**< Jumbo Frame Support disabled. */
	.hw_strip_crc   = 0, /**< CRC stripping by hardware disabled. */
};

struct rte_fdir_conf fdir_conf = {
	.mode = RTE_FDIR_MODE_NONE,
	.pballoc = RTE_FDIR_PBALLOC_64K,
	.status = RTE_FDIR_REPORT_STATUS,
	.flexbytes_offset = 0x6,
	.drop_queue = 127,
};

static struct rte_eth_conf default_pmd_conf = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_NONE,
		.max_rx_pkt_len = ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload enabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
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
	.txq_flags = TX_Q_FLAGS

};

static int
configure_ethdev(uint8_t port_id, uint8_t start)
{
	int q_id;

	if (rte_eth_dev_configure(port_id, test_params->nb_rx_q,
			test_params->nb_tx_q, &default_pmd_conf) != 0) {
		goto error;
	}

	for (q_id = 0; q_id < test_params->nb_rx_q; q_id++) {
		if (rte_eth_rx_queue_setup(port_id, q_id, RX_RING_SIZE,
				rte_eth_dev_socket_id(port_id), &rx_conf_default,
				test_params->mbuf_pool) < 0) {
			goto error;
		}
	}

	for (q_id = 0; q_id < test_params->nb_tx_q; q_id++) {
		if (rte_eth_tx_queue_setup(port_id, q_id, TX_RING_SIZE,
				rte_eth_dev_socket_id(port_id), &tx_conf_default) < 0) {
			printf("Failed to setup tx queue (%d).\n", q_id);
			goto error;
		}
	}

	if (start) {
		if (rte_eth_dev_start(port_id) < 0) {
			printf("Failed to start device (%d).\n", port_id);
			goto error;
		}
	}
	return 0;

error:
	printf("Failed to configure ethdev %d", port_id);
	return -1;
}


static int
test_setup(void)
{
	int i, retval, nb_mbuf_per_pool;
	struct ether_addr *mac_addr = (struct ether_addr *)slave_mac;

	/* Allocate ethernet packet header with space for VLAN header */
	test_params->pkt_eth_hdr = malloc(sizeof(struct ether_hdr) +
			sizeof(struct vlan_hdr));
	if (test_params->pkt_eth_hdr == NULL) {
		printf("ethernet header struct allocation failed!\n");
		return -1;
	}


	nb_mbuf_per_pool = RTE_TEST_RX_DESC_MAX + DEF_PKT_BURST +
			RTE_TEST_TX_DESC_MAX + MAX_PKT_BURST;

	test_params->mbuf_pool = rte_mempool_create("MBUF_POOL", nb_mbuf_per_pool,
			MBUF_SIZE, MBUF_CACHE_SIZE, sizeof(struct rte_pktmbuf_pool_private),
			rte_pktmbuf_pool_init, NULL, rte_pktmbuf_init, NULL,
			rte_socket_id(), 0);
	if (test_params->mbuf_pool == NULL) {
		printf("rte_mempool_create failed\n");
		return -1;
	}

	/* Create / Initialize virtual eth devs */
	for (i = 0; i < TEST_MAX_NUMBER_OF_PORTS; i++) {
		char pmd_name[RTE_ETH_NAME_MAX_LEN];

		mac_addr->addr_bytes[ETHER_ADDR_LEN-1] = i;

		snprintf(pmd_name, RTE_ETH_NAME_MAX_LEN, "test_slave_pmd_%d", i);

		test_params->slave_port_ids[i] = virtual_ethdev_create(pmd_name,
				mac_addr, rte_socket_id());
		if (test_params->slave_port_ids[i] < 0) {
			printf("Failed to create virtual pmd eth device.\n");
			return -1;
		}

		retval = configure_ethdev(test_params->slave_port_ids[i], 1);
		if (retval != 0) {
			printf("Failed to configure virtual pmd eth device.\n");
			return -1;
		}
	}

	return 0;
}

static int
test_create_bonded_device(void)
{
	int retval, current_slave_count;

	uint8_t slaves[RTE_MAX_ETHPORTS];

	test_params->bonded_port_id = rte_eth_bond_create(BONDED_DEV_NAME,
			test_params->bonding_mode, rte_socket_id());
	if (test_params->bonded_port_id < 0) {
		printf("\t Failed to create bonded device.\n");
		return -1;
	}

	retval = configure_ethdev(test_params->bonded_port_id, 0);
	if (retval != 0) {
		printf("Failed to configure bonded pmd eth device.\n");
		return -1;
	}

	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	if (current_slave_count > 0) {
		printf("Number of slaves is great than expected.\n");
		return -1;
	}

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS);
	if (current_slave_count > 0) {
		printf("Number of active slaves is great than expected.\n");
		return -1;
	}


	if (rte_eth_bond_mode_get(test_params->bonded_port_id) !=
			test_params->bonding_mode) {
		printf("Bonded device mode not as expected.\n");
		return -1;

	}

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
	if (port_id >= 0) {
		printf("Created bonded device unexpectedly.\n");
		return -1;
	}

	test_params->bonding_mode = INVALID_BONDING_MODE;

	/* Invalid bonding mode */
	port_id = rte_eth_bond_create(BONDED_DEV_NAME, test_params->bonding_mode,
			rte_socket_id());
	if (port_id >= 0) {
		printf("Created bonded device unexpectedly.\n");
		return -1;
	}

	test_params->bonding_mode = BONDING_MODE_ROUND_ROBIN;

	/* Invalid socket id */
	port_id = rte_eth_bond_create(BONDED_DEV_NAME, test_params->bonding_mode,
			INVALID_SOCKET_ID);
	if (port_id >= 0) {
		printf("Created bonded device unexpectedly.\n");
		return -1;
	}

	return 0;
}

static int
test_add_slave_to_bonded_device(void)
{
	int retval, current_slave_count;

	uint8_t slaves[RTE_MAX_ETHPORTS];

	retval = rte_eth_bond_slave_add(test_params->bonded_port_id,
			test_params->slave_port_ids[test_params->bonded_slave_count]);
	if (retval != 0) {
		printf("Failed to add slave (%d) to bonded port (%d).\n",
				test_params->bonded_port_id,
				test_params->slave_port_ids[test_params->bonded_slave_count]);
		return -1;
	}

	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	if (current_slave_count != test_params->bonded_slave_count + 1) {
		printf("Number of slaves (%d) is greater than expected (%d).\n",
				current_slave_count, test_params->bonded_slave_count + 1);
		return -1;
	}

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS);
	if (current_slave_count != 0) {
		printf("Number of active slaves (%d) is not as expected (%d).\n",
				current_slave_count, 0);
		return -1;
	}

	test_params->bonded_slave_count++;

	return 0;
}

static int
test_add_slave_to_invalid_bonded_device(void)
{
	int retval;

	/* Invalid port ID */
	retval = rte_eth_bond_slave_add(test_params->bonded_port_id + 5,
			test_params->slave_port_ids[test_params->bonded_slave_count]);
	if (retval == 0) {
		printf("Expected call to failed as invalid port specified.\n");
		return -1;
	}

	/* Non bonded device */
	retval = rte_eth_bond_slave_add(test_params->slave_port_ids[0],
			test_params->slave_port_ids[test_params->bonded_slave_count]);
	if (retval == 0) {
		printf("Expected call to failed as invalid port specified.\n");
		return -1;
	}

	return 0;
}


static int
test_remove_slave_from_bonded_device(void)
{
	int retval, current_slave_count;
	struct ether_addr read_mac_addr, *mac_addr;
	uint8_t slaves[RTE_MAX_ETHPORTS];

	retval = rte_eth_bond_slave_remove(test_params->bonded_port_id,
			test_params->slave_port_ids[test_params->bonded_slave_count-1]);
	if (retval != 0) {
		printf("\t Failed to remove slave %d from bonded port (%d).\n",
				test_params->slave_port_ids[test_params->bonded_slave_count-1],
				test_params->bonded_port_id);
		return -1;
	}


	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	if (current_slave_count != test_params->bonded_slave_count - 1) {
		printf("Number of slaves (%d) is great than expected (%d).\n",
				current_slave_count, 0);
		return -1;
	}


	mac_addr = (struct ether_addr *)slave_mac;
	mac_addr->addr_bytes[ETHER_ADDR_LEN-1] =
			test_params->slave_port_ids[test_params->bonded_slave_count-1];

	rte_eth_macaddr_get(
			test_params->slave_port_ids[test_params->bonded_slave_count-1],
			&read_mac_addr);
	if (memcmp(mac_addr, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port mac address not set to that of primary port\n");
		return -1;
	}

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
	int retval;

	/* Invalid port ID */
	retval = rte_eth_bond_slave_remove(test_params->bonded_port_id + 5,
			test_params->slave_port_ids[test_params->bonded_slave_count - 1]);
	if (retval == 0) {
		printf("Expected call to failed as invalid port specified.\n");
		return -1;
	}

	/* Non bonded device */
	retval = rte_eth_bond_slave_remove(test_params->slave_port_ids[0],
			test_params->slave_port_ids[test_params->bonded_slave_count - 1]);
	if (retval == 0) {
		printf("Expected call to failed as invalid port specified.\n");
		return -1;
	}

	return 0;
}

static int
test_add_already_bonded_slave_to_bonded_device(void)
{
	int retval, port_id, current_slave_count;
	uint8_t slaves[RTE_MAX_ETHPORTS];
	char pmd_name[RTE_ETH_NAME_MAX_LEN];

	test_add_slave_to_bonded_device();

	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	if (current_slave_count != 1) {
		printf("Number of slaves (%d) is not that expected (%d).\n",
				current_slave_count, 1);
		return -1;
	}

	snprintf(pmd_name, RTE_ETH_NAME_MAX_LEN, "%s_2", BONDED_DEV_NAME);

	port_id = rte_eth_bond_create(pmd_name, test_params->bonding_mode,
			rte_socket_id());
	if (port_id < 0) {
		printf("Failed to create bonded device.\n");
		return -1;
	}

	retval = rte_eth_bond_slave_add(port_id,
			test_params->slave_port_ids[test_params->bonded_slave_count - 1]);
	if (retval == 0) {
		printf("Added slave (%d) to bonded port (%d) unexpectedly.\n",
				test_params->slave_port_ids[test_params->bonded_slave_count-1],
				port_id);
		return -1;
	}

	return test_remove_slave_from_bonded_device();
}


static int
test_get_slaves_from_bonded_device(void)
{
	int retval, current_slave_count;

	uint8_t slaves[RTE_MAX_ETHPORTS];

	retval = test_add_slave_to_bonded_device();
	if (retval != 0)
		return -1;

	/* Invalid port id */
	current_slave_count = rte_eth_bond_slaves_get(INVALID_PORT_ID, slaves,
			RTE_MAX_ETHPORTS);
	if (current_slave_count >= 0)
		return -1;

	current_slave_count = rte_eth_bond_active_slaves_get(INVALID_PORT_ID,
			slaves, RTE_MAX_ETHPORTS);
	if (current_slave_count >= 0)
		return -1;

	/* Invalid slaves pointer */
	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			NULL, RTE_MAX_ETHPORTS);
	if (current_slave_count >= 0)
		return -1;

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, NULL, RTE_MAX_ETHPORTS);
	if (current_slave_count >= 0)
		return -1;

	/* non bonded device*/
	current_slave_count = rte_eth_bond_slaves_get(
			test_params->slave_port_ids[0], NULL, RTE_MAX_ETHPORTS);
	if (current_slave_count >= 0)
		return -1;

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->slave_port_ids[0],	NULL, RTE_MAX_ETHPORTS);
	if (current_slave_count >= 0)
		return -1;

	retval = test_remove_slave_from_bonded_device();
	if (retval != 0)
		return -1;

	return 0;
}


static int
test_add_remove_multiple_slaves_to_from_bonded_device(void)
{
	int i;

	for (i = 0; i < TEST_MAX_NUMBER_OF_PORTS; i++) {
		if (test_add_slave_to_bonded_device() != 0)
			return -1;
	}

	for (i = 0; i < TEST_MAX_NUMBER_OF_PORTS; i++) {
		if (test_remove_slave_from_bonded_device() != 0)
			return -1;
	}

	return 0;
}

static void
enable_bonded_slaves(void)
{
	int i;

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 1);
	}
}

static int
test_start_bonded_device(void)
{
	struct rte_eth_link link_status;

	int retval, current_slave_count, current_bonding_mode, primary_port;
	uint8_t slaves[RTE_MAX_ETHPORTS];

	/* Add slave to bonded device*/
	if (test_add_slave_to_bonded_device() != 0)
		return -1;

	retval = rte_eth_dev_start(test_params->bonded_port_id);
	if (retval != 0) {
		printf("Failed to start bonded pmd eth device.\n");
		return -1;
	}

	/* Change link status of virtual pmd so it will be added to the active
	 * slave list of the bonded device*/
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[test_params->bonded_slave_count-1], 1);

	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	if (current_slave_count != test_params->bonded_slave_count) {
		printf("Number of slaves (%d) is not expected value (%d).\n",
				current_slave_count, test_params->bonded_slave_count);
		return -1;
	}

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS);
	if (current_slave_count != test_params->bonded_slave_count) {
		printf("Number of active slaves (%d) is not expected value (%d).\n",
				current_slave_count, test_params->bonded_slave_count);
		return -1;
	}

	current_bonding_mode = rte_eth_bond_mode_get(test_params->bonded_port_id);
	if (current_bonding_mode != test_params->bonding_mode) {
		printf("Bonded device mode (%d) is not expected value (%d).\n",
				current_bonding_mode, test_params->bonding_mode);
		return -1;

	}

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	if (primary_port != test_params->slave_port_ids[0]) {
		printf("Primary port (%d) is not expected value (%d).\n",
				primary_port, test_params->slave_port_ids[0]);
		return -1;

	}

	rte_eth_link_get(test_params->bonded_port_id, &link_status);
	if (!link_status.link_status) {
		printf("Bonded port (%d) status (%d) is not expected value (%d).\n",
				test_params->bonded_port_id, link_status.link_status, 1);
		return -1;

	}

	return 0;
}

static int
test_stop_bonded_device(void)
{
	int current_slave_count;
	uint8_t slaves[RTE_MAX_ETHPORTS];

	struct rte_eth_link link_status;

	rte_eth_dev_stop(test_params->bonded_port_id);

	rte_eth_link_get(test_params->bonded_port_id, &link_status);
	if (link_status.link_status) {
		printf("Bonded port (%d) status (%d) is not expected value (%d).\n",
				test_params->bonded_port_id, link_status.link_status, 0);
		return -1;
	}

	current_slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	if (current_slave_count != test_params->bonded_slave_count) {
		printf("Number of slaves (%d) is not expected value (%d).\n",
				current_slave_count, test_params->bonded_slave_count);
		return -1;
	}

	current_slave_count = rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS);
	if (current_slave_count != 0) {
		printf("Number of active slaves (%d) is not expected value (%d).\n",
				current_slave_count, 0);
		return -1;
	}

	return 0;
}

static int remove_slaves_and_stop_bonded_device(void)
{
	/* Clean up and remove slaves from bonded device */
	while (test_params->bonded_slave_count > 0) {
		if (test_remove_slave_from_bonded_device() != 0) {
			printf("test_remove_slave_from_bonded_device failed\n");
			return -1;
		}
	}

	rte_eth_dev_stop(test_params->bonded_port_id);
	rte_eth_stats_reset(test_params->bonded_port_id);
	rte_eth_bond_mac_address_reset(test_params->bonded_port_id);

	return 0;
}

static int
test_set_bonding_mode(void)
{
	int i;
	int retval, bonding_mode;

	int bonding_modes[] = { BONDING_MODE_ROUND_ROBIN,
							BONDING_MODE_ACTIVE_BACKUP,
							BONDING_MODE_BALANCE,
							BONDING_MODE_BROADCAST };

	/* Test supported link bonding modes */
	for (i = 0; i < (int)RTE_DIM(bonding_modes);	i++) {
		/* Invalid port ID */
		retval = rte_eth_bond_mode_set(INVALID_PORT_ID, bonding_modes[i]);
		if (retval == 0) {
			printf("Expected call to failed as invalid port (%d) specified.\n",
					INVALID_PORT_ID);
			return -1;
		}

		/* Non bonded device */
		retval = rte_eth_bond_mode_set(test_params->slave_port_ids[0],
				bonding_modes[i]);
		if (retval == 0) {
			printf("Expected call to failed as invalid port (%d) specified.\n",
					test_params->slave_port_ids[0]);
			return -1;
		}

		retval = rte_eth_bond_mode_set(test_params->bonded_port_id,
				bonding_modes[i]);
		if (retval != 0) {
			printf("Failed to set link bonding mode on port (%d) to (%d).\n",
					test_params->bonded_port_id, bonding_modes[i]);
			return -1;
		}

		bonding_mode = rte_eth_bond_mode_get(test_params->bonded_port_id);
		if (bonding_mode != bonding_modes[i]) {
			printf("Link bonding mode (%d) of port (%d) is not expected value (%d).\n",
					bonding_mode, test_params->bonded_port_id,
					bonding_modes[i]);
			return -1;
		}


		/* Invalid port ID */
		bonding_mode = rte_eth_bond_mode_get(INVALID_PORT_ID);
		if (bonding_mode >= 0) {
			printf("Expected call to failed as invalid port (%d) specified.\n",
					INVALID_PORT_ID);
			return -1;
		}


		/* Non bonded device */
		bonding_mode = rte_eth_bond_mode_get(test_params->slave_port_ids[0]);
		if (bonding_mode >= 0) {
			printf("Expected call to failed as invalid port (%d) specified.\n",
					test_params->slave_port_ids[0]);
			return -1;
		}

	}

	return remove_slaves_and_stop_bonded_device();
}

static int
test_set_primary_slave(void)
{
	int i, j, retval;
	struct ether_addr read_mac_addr;
	struct ether_addr *expected_mac_addr;

	/* Add 4 slaves to bonded device */
	for (i = test_params->bonded_slave_count; i < 4; i++) {
		retval = test_add_slave_to_bonded_device();
		if (retval != 0) {
			printf("Failed to add slave to bonded device.\n");
			return -1;
		}
	}
	retval = rte_eth_bond_mode_set(test_params->bonded_port_id,
			BONDING_MODE_ROUND_ROBIN);
	if (retval != 0) {
		printf("Failed to set link bonding mode on port (%d) to (%d).\n",
				test_params->bonded_port_id, BONDING_MODE_ROUND_ROBIN);
		return -1;
	}

	/* Invalid port ID */
	retval = rte_eth_bond_primary_set(INVALID_PORT_ID,
			test_params->slave_port_ids[i]);
	if (retval == 0) {
		printf("Expected call to failed as invalid port specified.\n");
		return -1;
	}

	/* Set slave as primary
	 * Verify slave it is now primary slave
	 * Verify that MAC address of bonded device is that of primary slave
	 * Verify that MAC address of all bonded slaves are that of primary slave
	 */
	for (i = 0; i < 4; i++) {

		/* Non bonded device */
		retval = rte_eth_bond_primary_set(test_params->slave_port_ids[i],
				test_params->slave_port_ids[i]);
		if (retval == 0) {
			printf("Expected call to failed as invalid port specified.\n");
			return -1;
		}

		retval = rte_eth_bond_primary_set(test_params->bonded_port_id,
				test_params->slave_port_ids[i]);
		if (retval != 0) {
			printf("Failed to set bonded port (%d) primary port to (%d)\n",
					test_params->bonded_port_id,
					test_params->slave_port_ids[i]);
			return -1;
		}

		retval = rte_eth_bond_primary_get(test_params->bonded_port_id);
		if (retval < 0) {
			printf("Failed to read primary port from bonded port (%d)\n",
					test_params->bonded_port_id);
			return -1;
		} else if (retval != test_params->slave_port_ids[i]) {
			printf("Bonded port (%d) primary port (%d) not expected value (%d)\n",
					test_params->bonded_port_id, retval,
					test_params->slave_port_ids[i]);
			return -1;
		}

		/* stop/start bonded eth dev to apply new MAC */
		rte_eth_dev_stop(test_params->bonded_port_id);
		if (rte_eth_dev_start(test_params->bonded_port_id) != 0)
			return -1;

		expected_mac_addr = (struct ether_addr *)&slave_mac;
		expected_mac_addr->addr_bytes[ETHER_ADDR_LEN-1] =
				test_params->slave_port_ids[i];

		/* Check primary slave MAC */
		rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr);
		if (memcmp(expected_mac_addr, &read_mac_addr, sizeof(read_mac_addr))) {
			printf("bonded port mac address not set to that of primary port\n");
			return -1;
		}

		/* Check bonded MAC */
		rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
		if (memcmp(&read_mac_addr, &read_mac_addr, sizeof(read_mac_addr))) {
			printf("bonded port mac address not set to that of primary port\n");
			return -1;
		}

		/* Check other slaves MACs */
		for (j = 0; j < 4; j++) {
			if (j != i) {
				rte_eth_macaddr_get(test_params->slave_port_ids[j],
						&read_mac_addr);
				if (memcmp(expected_mac_addr, &read_mac_addr,
						sizeof(read_mac_addr))) {
					printf("slave port mac address not set to that of primary port\n");
					return -1;
				}
			}
		}
	}


	/* Test with none existent port */
	retval = rte_eth_bond_primary_get(test_params->bonded_port_id + 10);
	if (retval >= 0) {
		printf("read primary port from expectedly\n");
		return -1;
	}

	/* Test with slave port */
	retval = rte_eth_bond_primary_get(test_params->slave_port_ids[0]);
	if (retval >= 0) {
		printf("read primary port from expectedly\n");
		return -1;
	}

	if (remove_slaves_and_stop_bonded_device() != 0)
		return -1;

	/* No slaves  */
	retval = rte_eth_bond_primary_get(test_params->bonded_port_id);
	if (retval >= 0) {
		printf("read primary port from expectedly\n");
		return -1;
	}

	return 0;
}

static int
test_set_explicit_bonded_mac(void)
{
	int i, retval;
	struct ether_addr read_mac_addr;
	struct ether_addr *mac_addr;

	uint8_t explicit_bonded_mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };

	mac_addr = (struct ether_addr *)explicit_bonded_mac;

	/* Invalid port ID */
	retval = rte_eth_bond_mac_address_set(INVALID_PORT_ID, mac_addr);
	if (retval == 0) {
		printf("Expected call to failed as invalid port specified.\n");
		return -1;
	}

	/* Non bonded device */
	retval = rte_eth_bond_mac_address_set(test_params->slave_port_ids[0],
			mac_addr);
	if (retval == 0) {
		printf("Expected call to failed as invalid port specified.\n");
		return -1;
	}

	/* NULL MAC address */
	retval = rte_eth_bond_mac_address_set(test_params->bonded_port_id, NULL);
	if (retval == 0) {
		printf("Expected call to failed as NULL MAC specified\n");
		return -1;
	}

	retval = rte_eth_bond_mac_address_set(test_params->bonded_port_id,
			mac_addr);
	if (retval != 0) {
		printf("Failed to set MAC address on bonded port (%d)\n",
				test_params->bonded_port_id);
		return -1;
	}

	/* Add 4 slaves to bonded device */
	for (i = test_params->bonded_slave_count; i < 4; i++) {
		retval = test_add_slave_to_bonded_device();
		if (retval != 0) {
			printf("Failed to add slave to bonded device.\n");
			return -1;
		}
	}

	/* Check bonded MAC */
	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(mac_addr, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port mac address not set to that of primary port\n");
		return -1;
	}

	/* Check other slaves MACs */
	for (i = 0; i < 4; i++) {
		rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr);
		if (memcmp(mac_addr, &read_mac_addr, sizeof(read_mac_addr))) {
			printf("slave port mac address not set to that of primary port\n");
			return -1;
		}
	}

	/* test resetting mac address on bonded device */
	if (rte_eth_bond_mac_address_reset(test_params->bonded_port_id) != 0) {
		printf("Failed to reset MAC address on bonded port (%d)\n",
				test_params->bonded_port_id);

		return -1;
	}

	if (rte_eth_bond_mac_address_reset(test_params->slave_port_ids[0]) == 0) {
		printf("Reset MAC address on bonded port (%d) unexpectedly\n",
				test_params->slave_port_ids[1]);

		return -1;
	}

	/* test resetting mac address on bonded device with no slaves */

	if (remove_slaves_and_stop_bonded_device() != 0)
		return -1;

	if (rte_eth_bond_mac_address_reset(test_params->bonded_port_id) != 0) {
		printf("Failed to reset MAC address on bonded port (%d)\n",
				test_params->bonded_port_id);

		return -1;
	}

	return 0;
}


static int
initialize_bonded_device_with_slaves(uint8_t bonding_mode,
		uint8_t number_of_slaves, uint8_t enable_slave)
{
	int retval;

	/* configure bonded device */
	retval = configure_ethdev(test_params->bonded_port_id, 0);
	if (retval != 0) {
		printf("Failed to configure bonding port (%d) in mode %d with (%d) slaves.\n",
				test_params->bonded_port_id, bonding_mode, number_of_slaves);
		return -1;
	}

	while (number_of_slaves > test_params->bonded_slave_count) {
		/* Add slaves to bonded device */
		retval = test_add_slave_to_bonded_device();
		if (retval != 0) {
			printf("Failed to add slave (%d to  bonding port (%d).\n",
					test_params->bonded_slave_count - 1,
					test_params->bonded_port_id);
			return -1;
		}
	}

	/* Set link bonding mode  */
	retval = rte_eth_bond_mode_set(test_params->bonded_port_id, bonding_mode);
	if (retval != 0) {
		printf("Failed to set link bonding mode on port (%d) to (%d).\n",
				test_params->bonded_port_id, bonding_mode);
		return -1;
	}

	retval = rte_eth_dev_start(test_params->bonded_port_id);
	if (retval != 0) {
		printf("Failed to start bonded pmd eth device.\n");
		return -1;
	}

	if (enable_slave)
		enable_bonded_slaves();

	return 0;
}

static int
test_adding_slave_after_bonded_device_started(void)
{
	int i;

	if (initialize_bonded_device_with_slaves(BONDING_MODE_ROUND_ROBIN, 4, 0) !=
			0)
		return -1;

	/* Enabled slave devices */
	for (i = 0; i < test_params->bonded_slave_count + 1; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 1);
	}

	if (rte_eth_bond_slave_add(test_params->bonded_port_id,
			test_params->slave_port_ids[test_params->bonded_slave_count]) !=
					0) {
		printf("\t Failed to add slave to bonded port.\n");
		return -1;
	}

	test_params->bonded_slave_count++;

	return remove_slaves_and_stop_bonded_device();
}

static int
generate_test_burst(struct rte_mbuf **pkts_burst, uint16_t burst_size,
		uint8_t vlan, uint8_t ipv4, uint8_t toggle_dst_mac,
		uint8_t toggle_ip_addr, uint8_t toggle_udp_port)
{
	uint16_t pktlen, generated_burst_size;
	void *ip_hdr;

	if (toggle_dst_mac)
		initialize_eth_header(test_params->pkt_eth_hdr,
				(struct ether_addr *)src_mac, (struct ether_addr *)dst_mac_1,
				vlan, vlan_id);
	else
		initialize_eth_header(test_params->pkt_eth_hdr,
				(struct ether_addr *)src_mac, (struct ether_addr *)dst_mac_0,
				vlan, vlan_id);


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
			test_params->pkt_udp_hdr, burst_size);
	if (generated_burst_size != burst_size) {
		printf("Failed to generate packet burst");
		return -1;
	}

	return generated_burst_size;
}

/** Round Robin Mode Tests */

static int
test_roundrobin_tx_burst(void)
{
	int i, burst_size, nb_tx;
	struct rte_mbuf *pkt_burst[MAX_PKT_BURST];
	struct rte_eth_stats port_stats;

	if (initialize_bonded_device_with_slaves(BONDING_MODE_ROUND_ROBIN, 2, 1)
			!= 0)
		return -1;

	burst_size = 20 * test_params->bonded_slave_count;

	if (burst_size > MAX_PKT_BURST) {
		printf("Burst size specified is greater than supported.\n");
		return -1;
	}

	/* Generate test bursts of packets to transmit */
	if (generate_test_burst(pkt_burst, burst_size, 0, 1, 0, 0, 0) != burst_size)
		return -1;

	/* Send burst on bonded port */
	nb_tx = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkt_burst,
			burst_size);
	if (nb_tx != burst_size)
		return -1;

	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size) {
		printf("Bonded Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->bonded_port_id, (unsigned int)port_stats.opackets,
				burst_size);
		return -1;
	}

	/* Verify slave ports tx stats */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_stats_get(test_params->slave_port_ids[i], &port_stats);
		if (port_stats.opackets !=
				(uint64_t)burst_size / test_params->bonded_slave_count) {
			printf("Slave Port (%d) opackets value (%u) not as expected (%d)\n",
					test_params->bonded_port_id,
					(unsigned int)port_stats.opackets,
					burst_size / test_params->bonded_slave_count);
			return -1;
		}
	}

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	nb_tx = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkt_burst,
			burst_size);
	if (nb_tx != 0)
		return -1;

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_roundrobin_rx_burst_on_single_slave(void)
{
	struct rte_mbuf *gen_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };

	struct rte_eth_stats port_stats;

	int i, j, nb_rx, burst_size = 25;

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_ROUND_ROBIN, 4, 1) !=
			0)
		return -1;

	/* Generate test bursts of packets to transmit */
	if (generate_test_burst(gen_pkt_burst, burst_size, 0, 1, 0, 0, 0) !=
			burst_size)
		return -1;

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		/* Add rx data to slave */
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[0], burst_size);

		/* Call rx burst on bonded device */
		/* Send burst on bonded port */
		nb_rx = rte_eth_rx_burst(test_params->bonded_port_id, 0, rx_pkt_burst,
				MAX_PKT_BURST);
		if (nb_rx != burst_size) {
			printf("round-robin rx burst failed");
			return -1;
		}

		/* Verify bonded device rx count */
		rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
		if (port_stats.ipackets != (uint64_t)burst_size) {
			printf("Bonded Port (%d) ipackets value (%u) not as expected (%d)\n",
					test_params->bonded_port_id,
					(unsigned int)port_stats.ipackets, burst_size);
			return -1;
		}


		/* Verify bonded slave devices rx count */
		/* Verify slave ports tx stats */
		for (j = 0; j < test_params->bonded_slave_count; j++) {
			rte_eth_stats_get(test_params->slave_port_ids[j], &port_stats);

			if (i == j) {
				if (port_stats.ipackets != (uint64_t)burst_size) {
					printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
							test_params->slave_port_ids[i],
							(unsigned int)port_stats.ipackets, burst_size);
					return -1;
				}
			} else {
				if (port_stats.ipackets != 0) {
					printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
							test_params->slave_port_ids[i],
							(unsigned int)port_stats.ipackets, 0);
					return -1;
				}
			}

			/* Reset bonded slaves stats */
			rte_eth_stats_reset(test_params->slave_port_ids[j]);
		}
		/* reset bonded device stats */
		rte_eth_stats_reset(test_params->bonded_port_id);
	}

	/* free mbufs */
	for (i = 0; i < MAX_PKT_BURST; i++) {
		if (gen_pkt_burst[i] != NULL)
			rte_pktmbuf_free(gen_pkt_burst[i]);

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
	if (initialize_bonded_device_with_slaves(BONDING_MODE_ROUND_ROBIN, 4, 1) !=
			0)
		return -1;

	/* Generate test bursts of packets to transmit */
	for (i = 0; i < TEST_ROUNDROBIN_TX_BURST_SLAVE_COUNT; i++) {
		if (generate_test_burst(&gen_pkt_burst[i][0], burst_size[i], 0, 1, 0, 0,
				0) != burst_size[i])
			return -1;
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
	if (nb_rx != burst_size[0] + burst_size[1] + burst_size[2]) {
		printf("round-robin rx burst failed (%d != %d)\n", nb_rx,
				burst_size[0] + burst_size[1] + burst_size[2]);
		return -1;
	}

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.ipackets != (uint64_t)(burst_size[0] + burst_size[1] +
			burst_size[2])) {
		printf("Bonded Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->bonded_port_id, (unsigned int)port_stats.ipackets,
				burst_size[0] + burst_size[1] + burst_size[2]);
		return -1;
	}


	/* Verify bonded slave devices rx counts */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.ipackets != (uint64_t)burst_size[0]) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[0],
				(unsigned int)port_stats.ipackets, burst_size[0]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	if (port_stats.ipackets != (uint64_t)burst_size[1]) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[1],
				(unsigned int)port_stats.ipackets, burst_size[1]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	if (port_stats.ipackets != (uint64_t)burst_size[2]) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[2],
				(unsigned int)port_stats.ipackets, burst_size[2]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	if (port_stats.ipackets != 0) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[3],
				(unsigned int)port_stats.ipackets, 0);
		return -1;
	}

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
	struct ether_addr read_mac_addr, expected_mac_addr_0, expected_mac_addr_2;

	int i, retval;

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &expected_mac_addr_0);
	rte_eth_macaddr_get(test_params->slave_port_ids[2], &expected_mac_addr_2);

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_ROUND_ROBIN, 4, 1)
			!= 0)
		return -1;

	/* Verify that all MACs are the same as first slave added to bonded dev */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr);
		if (memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr))) {
			printf("slave port (%d) mac address not set to that of primary port\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	/* change primary and verify that MAC addresses haven't changed */
	retval = rte_eth_bond_primary_set(test_params->bonded_port_id,
			test_params->slave_port_ids[2]);
	if (retval != 0) {
		printf("Failed to set bonded port (%d) primary port to (%d)\n",
				test_params->bonded_port_id, test_params->slave_port_ids[i]);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr);
		if (memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr))) {
			printf("slave port (%d) mac address has changed to that of primary port without stop/start toggle of bonded device\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	/* stop / start bonded device and verify that primary MAC address is
	 * propagate to bonded device and slaves */

	rte_eth_dev_stop(test_params->bonded_port_id);

	if (rte_eth_dev_start(test_params->bonded_port_id) != 0)
		return -1;

	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(&expected_mac_addr_2, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of new primary port\n",
				test_params->slave_port_ids[i]);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr);
		if (memcmp(&expected_mac_addr_2, &read_mac_addr,
				sizeof(read_mac_addr))) {
			printf("slave port (%d) mac address not set to that of new primary port\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	/* Set explicit MAC address */
	if (rte_eth_bond_mac_address_set(test_params->bonded_port_id,
			(struct ether_addr *)bonded_mac) != 0) {
		return -1;
	}

	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(bonded_mac, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of new primary port\n",
				test_params->slave_port_ids[i]);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr);
		if (memcmp(bonded_mac, &read_mac_addr, sizeof(read_mac_addr))) {
			printf("slave port (%d) mac address not set to that of new primary port\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_roundrobin_verify_promiscuous_enable_disable(void)
{
	int i, promiscuous_en;

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_ROUND_ROBIN, 4, 1) !=
			0)
		return -1;

	rte_eth_promiscuous_enable(test_params->bonded_port_id);

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	if (promiscuous_en != 1) {
		printf("Port (%d) promiscuous mode not enabled\n",
				test_params->bonded_port_id);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(test_params->slave_port_ids[i]);
		if (promiscuous_en != 1) {
			printf("slave port (%d) promiscuous mode not enabled\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	rte_eth_promiscuous_disable(test_params->bonded_port_id);

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	if (promiscuous_en != 0) {
		printf("Port (%d) promiscuous mode not disabled\n",
				test_params->bonded_port_id);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(test_params->slave_port_ids[i]);
		if (promiscuous_en != 0) {
			printf("slave port (%d) promiscuous mode not disabled\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
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
	uint8_t slaves[RTE_MAX_ETHPORTS];

	int i, burst_size, slave_count;

	/* NULL all pointers in array to simplify cleanup */
	memset(gen_pkt_burst, 0, sizeof(gen_pkt_burst));

	/* Initialize bonded device with TEST_RR_LINK_STATUS_SLAVE_COUNT slaves
	 * in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_ROUND_ROBIN,
			TEST_RR_LINK_STATUS_SLAVE_COUNT, 1) != 0)
		return -1;

	/* Verify Current Slaves Count /Active Slave Count is */
	slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS);
	if (slave_count != TEST_RR_LINK_STATUS_SLAVE_COUNT) {
		printf("Number of slaves (%d) is not as expected (%d).\n", slave_count,
				TEST_RR_LINK_STATUS_SLAVE_COUNT);
		return -1;
	}

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	if (slave_count != TEST_RR_LINK_STATUS_SLAVE_COUNT) {
		printf("Number of active slaves (%d) is not as expected (%d).\n",
				slave_count, TEST_RR_LINK_STATUS_SLAVE_COUNT);
		return -1;
	}

	/* Set 2 slaves eth_devs link status to down */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 0);

	if (rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS) !=
					TEST_RR_LINK_STATUS_EXPECTED_ACTIVE_SLAVE_COUNT) {
		printf("Number of active slaves (%d) is not as expected (%d).\n",
				slave_count, TEST_RR_LINK_STATUS_EXPECTED_ACTIVE_SLAVE_COUNT);
		return -1;
	}

	burst_size = 21;

	/* Verify that pkts are not sent on slaves with link status down:
	 *
	 * 1. Generate test burst of traffic
	 * 2. Transmit burst on bonded eth_dev
	 * 3. Verify stats for bonded eth_dev (opackets = burst_size)
	 * 4. Verify stats for slave eth_devs (s0 = 11, s1 = 0, s2 = 10, s3 = 0)
	 */
	if (generate_test_burst(tx_pkt_burst, burst_size, 0, 1, 0, 0, 0) !=
			burst_size) {
		printf("generate_test_burst failed\n");
		return -1;
	}

	if (rte_eth_tx_burst(test_params->bonded_port_id, 0, tx_pkt_burst,
			burst_size) != burst_size) {
		printf("rte_eth_tx_burst failed\n");
		return -1;
	}

	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->bonded_port_id);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.opackets != 11) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[0]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	if (port_stats.opackets != 0) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[1]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	if (port_stats.opackets != 10) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[2]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	if (port_stats.opackets != 0) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[3]);
		return -1;
	}

	/* Verify that pkts are not sent on slaves with link status down:
	 *
	 * 1. Generate test bursts of traffic
	 * 2. Add bursts on to virtual eth_devs
	 * 3. Rx burst on bonded eth_dev, expected (burst_ size *
	 *    TEST_RR_LINK_STATUS_EXPECTED_ACTIVE_SLAVE_COUNT) received
	 * 4. Verify stats for bonded eth_dev
	 * 6. Verify stats for slave eth_devs (s0 = 11, s1 = 0, s2 = 10, s3 = 0)
	 */
	for (i = 0; i < TEST_RR_LINK_STATUS_SLAVE_COUNT; i++) {
		if (generate_test_burst(&gen_pkt_burst[i][0], burst_size, 0, 1, 0, 0, 0)
				!= burst_size) {
			return -1;
		}
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[i][0], burst_size);
	}

	if (rte_eth_rx_burst(test_params->bonded_port_id, 0, rx_pkt_burst,
			MAX_PKT_BURST) != burst_size + burst_size) {
		printf("rte_eth_rx_burst\n");
		return -1;
	}

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.ipackets != (uint64_t)(burst_size + burst_size)) {
		printf("(%d) port_stats.ipackets not as expected\n",
				test_params->bonded_port_id);
		return -1;
	}

	/* free mbufs */
	for (i = 0; i < MAX_PKT_BURST; i++) {
		if (rx_pkt_burst[i] != NULL)
			rte_pktmbuf_free(rx_pkt_burst[i]);

		if (gen_pkt_burst[1][i] != NULL)
			rte_pktmbuf_free(gen_pkt_burst[1][i]);

		if (gen_pkt_burst[3][i] != NULL)
			rte_pktmbuf_free(gen_pkt_burst[1][i]);
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

/** Active Backup Mode Tests */

static int
test_activebackup_tx_burst(void)
{
	int i, retval, pktlen, primary_port, burst_size, generated_burst_size, nb_tx;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_eth_stats port_stats;

	retval = initialize_bonded_device_with_slaves(BONDING_MODE_ACTIVE_BACKUP, 2, 1);
	if (retval != 0) {
		printf("Failed to initialize_bonded_device_with_slaves.\n");
		return -1;
	}

	initialize_eth_header(test_params->pkt_eth_hdr,
			(struct ether_addr *)src_mac, (struct ether_addr *)dst_mac_0, 0, 0);
	pktlen = initialize_udp_header(test_params->pkt_udp_hdr, src_port,
			dst_port_0, 16);
	pktlen = initialize_ipv4_header(test_params->pkt_ipv4_hdr, src_addr,
			dst_addr_0, pktlen);

	burst_size = 20 * test_params->bonded_slave_count;

	if (burst_size > MAX_PKT_BURST) {
		printf("Burst size specified is greater than supported.\n");
		return -1;
	}

	/* Generate a burst of packets to transmit */
	generated_burst_size = generate_packet_burst(test_params->mbuf_pool,
			pkts_burst,	test_params->pkt_eth_hdr, 0, test_params->pkt_ipv4_hdr,
			1, test_params->pkt_udp_hdr, burst_size);
	if (generated_burst_size != burst_size)
		return -1;

	/* Send burst on bonded port */
	nb_tx = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst,
			burst_size);
	if (nb_tx != burst_size)
		return -1;

	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size) {
		printf("Bonded Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->bonded_port_id, (unsigned int)port_stats.opackets,
				burst_size);
		return -1;
	}

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);

	/* Verify slave ports tx stats */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_stats_get(test_params->slave_port_ids[i], &port_stats);
		if (test_params->slave_port_ids[i] == primary_port) {
			if (port_stats.opackets != (uint64_t)burst_size) {
				printf("Slave Port (%d) opackets value (%u) not as expected (%d)\n",
						test_params->bonded_port_id,
						(unsigned int)port_stats.opackets,
						burst_size / test_params->bonded_slave_count);
				return -1;
			}
		} else {
			if (port_stats.opackets != 0) {
				printf("Slave Port (%d) opackets value (%u) not as expected (%d)\n",
						test_params->bonded_port_id,
						(unsigned int)port_stats.opackets, 0);
				return -1;
			}
		}
	}

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	nb_tx = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst,
			burst_size);
	if (nb_tx != 0)
		return -1;

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

	int i, j, nb_rx, burst_size = 17;

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_ACTIVE_BACKUP,
			TEST_ACTIVE_BACKUP_RX_BURST_SLAVE_COUNT, 1)
			!= 0)
		return -1;


	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	if (primary_port < 0) {
		printf("failed to get primary slave for bonded port (%d)",
				test_params->bonded_port_id);
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		/* Generate test bursts of packets to transmit */
		if (generate_test_burst(&gen_pkt_burst[0], burst_size, 0, 1, 0, 0, 0)
				!= burst_size) {
			return -1;
		}

		/* Add rx data to slave */
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[0], burst_size);

		/* Call rx burst on bonded device */
		nb_rx = rte_eth_rx_burst(test_params->bonded_port_id, 0,
				&rx_pkt_burst[0], MAX_PKT_BURST);
		if (nb_rx < 0) {
			printf("rte_eth_rx_burst failed\n");
			return -1;
		}

		if (test_params->slave_port_ids[i] == primary_port) {
			/* Verify bonded device rx count */
			rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
			if (port_stats.ipackets != (uint64_t)burst_size) {
				printf("Bonded Port (%d) ipackets value (%u) not as expected (%d)\n",
						test_params->bonded_port_id,
						(unsigned int)port_stats.ipackets, burst_size);
				return -1;
			}

			/* Verify bonded slave devices rx count */
			for (j = 0; j < test_params->bonded_slave_count; j++) {
				rte_eth_stats_get(test_params->slave_port_ids[j], &port_stats);
				if (i == j) {
					if (port_stats.ipackets != (uint64_t)burst_size) {
						printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
								test_params->slave_port_ids[i],
								(unsigned int)port_stats.ipackets, burst_size);
						return -1;
					}
				} else {
					if (port_stats.ipackets != 0) {
						printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
								test_params->slave_port_ids[i],
								(unsigned int)port_stats.ipackets, 0);
						return -1;
					}
				}
			}
		} else {
			for (j = 0; j < test_params->bonded_slave_count; j++) {
				rte_eth_stats_get(test_params->slave_port_ids[j], &port_stats);
				if (port_stats.ipackets != 0) {
					printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
							test_params->slave_port_ids[i],
							(unsigned int)port_stats.ipackets, 0);
					return -1;
				}
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

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_ACTIVE_BACKUP, 4, 1)
			!= 0)
		return -1;

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	if (primary_port < 0) {
		printf("failed to get primary slave for bonded port (%d)",
				test_params->bonded_port_id);
	}

	rte_eth_promiscuous_enable(test_params->bonded_port_id);

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	if (promiscuous_en != 1) {
		printf("Port (%d) promiscuous mode not enabled\n",
				test_params->bonded_port_id);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		if (primary_port == test_params->slave_port_ids[i]) {
			if (promiscuous_en != 1) {
				printf("slave port (%d) promiscuous mode not enabled\n",
						test_params->slave_port_ids[i]);
				return -1;
			}
		} else {
			if (promiscuous_en != 0) {
				printf("slave port (%d) promiscuous mode enabled\n",
						test_params->slave_port_ids[i]);
				return -1;
			}
		}

	}

	rte_eth_promiscuous_disable(test_params->bonded_port_id);

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	if (promiscuous_en != 0) {
		printf("Port (%d) promiscuous mode not disabled\n",
				test_params->bonded_port_id);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		if (promiscuous_en != 0) {
			printf("slave port (%d) promiscuous mode not disabled\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_activebackup_verify_mac_assignment(void)
{
	struct ether_addr read_mac_addr, expected_mac_addr_0, expected_mac_addr_1;

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &expected_mac_addr_0);
	rte_eth_macaddr_get(test_params->slave_port_ids[1], &expected_mac_addr_1);

	/* Initialize bonded device with 2 slaves in active backup mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_ACTIVE_BACKUP, 2, 1)
			!= 0)
		return -1;

	/* Verify that bonded MACs is that of first slave and that the other slave
	 * MAC hasn't been changed */
	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of primary port\n",
				test_params->bonded_port_id);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not set to that of primary port\n",
				test_params->slave_port_ids[0]);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr);
	if (memcmp(&expected_mac_addr_1, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not as expected\n",
				test_params->slave_port_ids[1]);
		return -1;
	}

	/* change primary and verify that MAC addresses haven't changed */
	if (rte_eth_bond_primary_set(test_params->bonded_port_id,
			test_params->slave_port_ids[1]) != 0) {
		printf("Failed to set bonded port (%d) primary port to (%d)\n",
				test_params->bonded_port_id, test_params->slave_port_ids[1]);
		return -1;
	}

	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of primary port\n",
				test_params->bonded_port_id);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not set to that of primary port\n",
				test_params->slave_port_ids[0]);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr);
	if (memcmp(&expected_mac_addr_1, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not as expected\n",
				test_params->slave_port_ids[1]);
		return -1;
	}

	/* stop / start bonded device and verify that primary MAC address is
	 * propagated to bonded device and slaves */

	rte_eth_dev_stop(test_params->bonded_port_id);

	if (rte_eth_dev_start(test_params->bonded_port_id) != 0)
		return -1;

	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(&expected_mac_addr_1, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of primary port\n",
				test_params->bonded_port_id);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not as expected\n",
				test_params->slave_port_ids[0]);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr);
	if (memcmp(&expected_mac_addr_1, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not set to that of primary port\n",
				test_params->slave_port_ids[1]);
		return -1;
	}

	/* Set explicit MAC address */
	if (rte_eth_bond_mac_address_set(test_params->bonded_port_id,
			(struct ether_addr *)bonded_mac) != 0) {
		return -1;
	}

	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(&bonded_mac, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of bonded port\n",
				test_params->bonded_port_id);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not as expected\n",
				test_params->slave_port_ids[0]);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr);
	if (memcmp(&bonded_mac, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not set to that of bonded port\n",
				test_params->slave_port_ids[1]);
		return -1;
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_activebackup_verify_slave_link_status_change_failover(void)
{
	struct rte_mbuf *pkt_burst[TEST_ACTIVE_BACKUP_RX_BURST_SLAVE_COUNT][MAX_PKT_BURST];
	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	uint8_t slaves[RTE_MAX_ETHPORTS];

	int i, j, burst_size, slave_count, primary_port;

	burst_size = 21;

	memset(pkt_burst, 0, sizeof(pkt_burst));

	/* Generate packet burst for testing */
	if (generate_test_burst(&pkt_burst[0][0], burst_size, 0, 1, 0, 0, 0) !=
			burst_size) {
		printf("generate_test_burst failed\n");
		return -1;
	}

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_ACTIVE_BACKUP,
			TEST_ACTIVE_BACKUP_RX_BURST_SLAVE_COUNT, 1)
			!= 0)
		return -1;

	/* Verify Current Slaves Count /Active Slave Count is */
	slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS);
	if (slave_count != 4) {
		printf("Number of slaves (%d) is not as expected (%d).\n",
				slave_count, 4);
		return -1;
	}

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	if (slave_count != 4) {
		printf("Number of active slaves (%d) is not as expected (%d).\n",
				slave_count, 4);
		return -1;
	}

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	if (primary_port != test_params->slave_port_ids[0])
		printf("Primary port not as expected");

	/* Bring 2 slaves down and verify active slave count */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 0);

	if (rte_eth_bond_active_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS) != 2) {
		printf("Number of active slaves (%d) is not as expected (%d).\n",
				slave_count, 2);
		return -1;
	}

	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 1);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 1);


	/* Bring primary port down, verify that active slave count is 3 and primary
	 *  has changed */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[0], 0);

	if (rte_eth_bond_active_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS) != 3) {
		printf("Number of active slaves (%d) is not as expected (%d).\n",
				slave_count, 3);
		return -1;
	}

	primary_port = rte_eth_bond_primary_get(test_params->bonded_port_id);
	if (primary_port != test_params->slave_port_ids[2])
		printf("Primary port not as expected");

	/* Verify that pkts are sent on new primary slave */

	if (rte_eth_tx_burst(test_params->bonded_port_id, 0, &pkt_burst[0][0], burst_size)
			!= burst_size) {
		printf("rte_eth_tx_burst failed\n");
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[2]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.opackets != 0) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[0]);
		return -1;
	}
	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	if (port_stats.opackets != 0) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[1]);
		return -1;
	}
	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	if (port_stats.opackets != 0) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[3]);
		return -1;
	}

	/* Generate packet burst for testing */

	for (i = 0; i < TEST_ACTIVE_BACKUP_RX_BURST_SLAVE_COUNT; i++) {
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
	if (port_stats.ipackets != (uint64_t)burst_size) {
		printf("(%d) port_stats.ipackets not as expected\n",
				test_params->bonded_port_id);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[2]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.opackets != 0) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[0]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	if (port_stats.opackets != 0) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[1]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	if (port_stats.opackets != 0) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[3]);
		return -1;
	}

	/* free mbufs */

	for (i = 0; i < TEST_ACTIVE_BACKUP_RX_BURST_SLAVE_COUNT; i++) {
		for (j = 0; j < MAX_PKT_BURST; j++) {
			if (pkt_burst[i][j] != NULL) {
				rte_pktmbuf_free(pkt_burst[i][j]);
				pkt_burst[i][j] = NULL;
			}
		}
	}


	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

/** Balance Mode Tests */

static int
test_balance_xmit_policy_configuration(void)
{
	int retval;

	retval = initialize_bonded_device_with_slaves(BONDING_MODE_ACTIVE_BACKUP,
			2, 1);
	if (retval != 0) {
		printf("Failed to initialize_bonded_device_with_slaves.\n");
		return -1;
	}

	/* Invalid port id */
	retval = rte_eth_bond_xmit_policy_set(INVALID_PORT_ID,
			BALANCE_XMIT_POLICY_LAYER2);
	if (retval == 0) {
		printf("Expected call to failed as invalid port specified.\n");
		return -1;
	}

	/* Set xmit policy on non bonded device */
	retval = rte_eth_bond_xmit_policy_set(test_params->slave_port_ids[0],
			BALANCE_XMIT_POLICY_LAYER2);
	if (retval == 0) {
		printf("Expected call to failed as invalid port specified.\n");
		return -1;
	}

	retval = rte_eth_bond_xmit_policy_set(test_params->bonded_port_id,
			BALANCE_XMIT_POLICY_LAYER2);
	if (retval != 0) {
		printf("Failed to set balance xmit policy.\n");
		return -1;
	}
	if (rte_eth_bond_xmit_policy_get(test_params->bonded_port_id) !=
			BALANCE_XMIT_POLICY_LAYER2) {
		printf("balance xmit policy not as expected.\n");
		return -1;
	}

	retval = rte_eth_bond_xmit_policy_set(test_params->bonded_port_id,
			BALANCE_XMIT_POLICY_LAYER23);
	if (retval != 0) {
		printf("Failed to set balance xmit policy.\n");
		return -1;
	}
	if (rte_eth_bond_xmit_policy_get(test_params->bonded_port_id) !=
			BALANCE_XMIT_POLICY_LAYER23) {
		printf("balance xmit policy not as expected.\n");
		return -1;
	}

	retval = rte_eth_bond_xmit_policy_set(test_params->bonded_port_id,
			BALANCE_XMIT_POLICY_LAYER34);
	if (retval != 0) {
		printf("Failed to set balance xmit policy.\n");
		return -1;
	}
	if (rte_eth_bond_xmit_policy_get(test_params->bonded_port_id) !=
			BALANCE_XMIT_POLICY_LAYER34) {
		printf("balance xmit policy not as expected.\n");
		return -1;
	}

	/* Invalid port id */
	if (rte_eth_bond_xmit_policy_get(INVALID_PORT_ID) >= 0)
		return -1;

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

	int retval, i;
	struct rte_eth_stats port_stats;

	retval = initialize_bonded_device_with_slaves(BONDING_MODE_BALANCE,
			TEST_BALANCE_L2_TX_BURST_SLAVE_COUNT, 1);
	if (retval != 0) {
		printf("Failed to initialize_bonded_device_with_slaves.\n");
		return -1;
	}

	retval = rte_eth_bond_xmit_policy_set(test_params->bonded_port_id,
			BALANCE_XMIT_POLICY_LAYER2);
	if (retval != 0) {
		printf("Failed to set balance xmit policy.\n");
		return -1;
	}


	initialize_eth_header(test_params->pkt_eth_hdr,
			(struct ether_addr *)src_mac, (struct ether_addr *)dst_mac_0, 0, 0);
	pktlen = initialize_udp_header(test_params->pkt_udp_hdr, src_port,
			dst_port_0, 16);
	pktlen = initialize_ipv4_header(test_params->pkt_ipv4_hdr, src_addr,
			dst_addr_0, pktlen);

	/* Generate a burst 1 of packets to transmit */
	if (generate_packet_burst(test_params->mbuf_pool, &pkts_burst[0][0],
			test_params->pkt_eth_hdr, 0, test_params->pkt_ipv4_hdr, 1,
			test_params->pkt_udp_hdr, burst_size[0]) != burst_size[0])
		return -1;

	initialize_eth_header(test_params->pkt_eth_hdr,
			(struct ether_addr *)src_mac, (struct ether_addr *)dst_mac_1, 0, 0);

	/* Generate a burst 2 of packets to transmit */
	if (generate_packet_burst(test_params->mbuf_pool, &pkts_burst[1][0],
			test_params->pkt_eth_hdr, 0, test_params->pkt_ipv4_hdr, 1,
			test_params->pkt_udp_hdr, burst_size[1]) != burst_size[1])
		return -1;

	/* Send burst 1 on bonded port */
	for (i = 0; i < TEST_BALANCE_L2_TX_BURST_SLAVE_COUNT; i++) {
		if (rte_eth_tx_burst(test_params->bonded_port_id, 0, &pkts_burst[i][0],
			burst_size[i]) != burst_size[i])
			return -1;
	}
	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.opackets != (uint64_t)(burst_size[0] + burst_size[1])) {
		printf("Bonded Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->bonded_port_id,
				(unsigned int)port_stats.opackets, burst_size[0] + burst_size[1]);
		return -1;
	}


	/* Verify slave ports tx stats */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size[0]) {
		printf("Slave Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[0],
				(unsigned int)port_stats.opackets, burst_size[0]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size[1]) {
		printf("Slave Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[1], (
						unsigned int)port_stats.opackets, burst_size[1]);
		return -1;
	}

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	if (rte_eth_tx_burst(test_params->bonded_port_id, 0, &pkts_burst[0][0],
			burst_size[0]) != 0)
		return -1;

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
balance_l23_tx_burst(uint8_t vlan_enabled, uint8_t ipv4,
		uint8_t toggle_mac_addr, uint8_t toggle_ip_addr)
{
	int retval, i;
	int burst_size_1, burst_size_2, nb_tx_1, nb_tx_2;

	struct rte_mbuf *pkts_burst_1[MAX_PKT_BURST];
	struct rte_mbuf *pkts_burst_2[MAX_PKT_BURST];

	struct rte_eth_stats port_stats;

	retval = initialize_bonded_device_with_slaves(BONDING_MODE_BALANCE, 2, 1);
	if (retval != 0) {
		printf("Failed to initialize_bonded_device_with_slaves.\n");
		return -1;
	}

	retval = rte_eth_bond_xmit_policy_set(test_params->bonded_port_id,
			BALANCE_XMIT_POLICY_LAYER23);
	if (retval != 0) {
		printf("Failed to set balance xmit policy.\n");
		return -1;
	}

	burst_size_1 = 20;
	burst_size_2 = 10;

	if (burst_size_1 > MAX_PKT_BURST || burst_size_2 > MAX_PKT_BURST) {
		printf("Burst size specified is greater than supported.\n");
		return -1;
	}

	/* Generate test bursts of packets to transmit */
	if (generate_test_burst(pkts_burst_1, burst_size_1, vlan_enabled, ipv4,
			0, 0, 0) != burst_size_1)
		return -1;

	if (generate_test_burst(pkts_burst_2, burst_size_2, vlan_enabled, ipv4,
			toggle_mac_addr, toggle_ip_addr, 0) != burst_size_2)
		return -1;

	/* Send burst 1 on bonded port */
	nb_tx_1 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_1,
			burst_size_1);
	if (nb_tx_1 != burst_size_1)
		return -1;

	/* Send burst 2 on bonded port */
	nb_tx_2 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_2,
			burst_size_2);
	if (nb_tx_2 != burst_size_2)
		return -1;

	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.opackets != (uint64_t)(nb_tx_1 + nb_tx_2)) {
		printf("Bonded Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->bonded_port_id,
				(unsigned int)port_stats.opackets, nb_tx_1 + nb_tx_2);
		return -1;
	}

	/* Verify slave ports tx stats */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.opackets != (uint64_t)nb_tx_1) {
		printf("Slave Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[0],
				(unsigned int)port_stats.opackets, nb_tx_1);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	if (port_stats.opackets != (uint64_t)nb_tx_2) {
		printf("Slave Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[1],
				(unsigned int)port_stats.opackets, nb_tx_2);
		return -1;
	}

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	nb_tx_1 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_1,
			burst_size_1);
	if (nb_tx_1 != 0)
		return -1;

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_balance_l23_tx_burst_ipv4_toggle_ip_addr(void)
{
	return balance_l23_tx_burst(0, 1, 1, 0);
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
	int retval, i;
	int burst_size_1, burst_size_2, nb_tx_1, nb_tx_2;

	struct rte_mbuf *pkts_burst_1[MAX_PKT_BURST];
	struct rte_mbuf *pkts_burst_2[MAX_PKT_BURST];

	struct rte_eth_stats port_stats;

	retval = initialize_bonded_device_with_slaves(BONDING_MODE_BALANCE, 2, 1);
	if (retval != 0) {
		printf("Failed to initialize_bonded_device_with_slaves.\n");
		return -1;
	}

	retval = rte_eth_bond_xmit_policy_set(test_params->bonded_port_id,
			BALANCE_XMIT_POLICY_LAYER34);
	if (retval != 0) {
		printf("Failed to set balance xmit policy.\n");
		return -1;
	}

	burst_size_1 = 20;
	burst_size_2 = 10;

	if (burst_size_1 > MAX_PKT_BURST || burst_size_2 > MAX_PKT_BURST) {
		printf("Burst size specified is greater than supported.\n");
		return -1;
	}

	/* Generate test bursts of packets to transmit */
	if (generate_test_burst(pkts_burst_1, burst_size_1, vlan_enabled, ipv4, 0,
			0, 0) != burst_size_1)
		return -1;

	if (generate_test_burst(pkts_burst_2, burst_size_2, vlan_enabled, ipv4,
			toggle_mac_addr, toggle_ip_addr, toggle_udp_port) != burst_size_2)
		return -1;

	/* Send burst 1 on bonded port */
	nb_tx_1 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_1,
			burst_size_1);
	if (nb_tx_1 != burst_size_1)
		return -1;

	/* Send burst 2 on bonded port */
	nb_tx_2 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_2,
			burst_size_2);
	if (nb_tx_2 != burst_size_2)
		return -1;


	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.opackets != (uint64_t)(nb_tx_1 + nb_tx_2)) {
		printf("Bonded Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->bonded_port_id,
				(unsigned int)port_stats.opackets, nb_tx_1 + nb_tx_2);
		return -1;
	}

	/* Verify slave ports tx stats */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.opackets != (uint64_t)nb_tx_1) {
		printf("Slave Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[0],
				(unsigned int)port_stats.opackets, nb_tx_1);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	if (port_stats.opackets != (uint64_t)nb_tx_2) {
		printf("Slave Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[1],
				(unsigned int)port_stats.opackets, nb_tx_2);
		return -1;
	}

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	nb_tx_1 = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst_1,
			burst_size_1);
	if (nb_tx_1 != 0)
		return -1;

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

#define TEST_BALANCE_RX_BURST_SLAVE_COUNT (3)

static int
test_balance_rx_burst(void)
{
	struct rte_mbuf *gen_pkt_burst[TEST_BALANCE_RX_BURST_SLAVE_COUNT][MAX_PKT_BURST];

	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST] = { NULL };
	struct rte_eth_stats port_stats;

	int burst_size[TEST_BALANCE_RX_BURST_SLAVE_COUNT] = { 10, 5, 30 };
	int i, j, nb_rx;

	memset(gen_pkt_burst, 0, sizeof(gen_pkt_burst));

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_BALANCE, 3, 1)
			!= 0)
		return -1;

	/* Generate test bursts of packets to transmit */
	for (i = 0; i < TEST_BALANCE_RX_BURST_SLAVE_COUNT; i++) {
		if (generate_test_burst(&gen_pkt_burst[i][0], burst_size[i], 0, 0, 1,
				0, 0) != burst_size[i])
			return -1;
	}
	/* Add rx data to slaves */
	for (i = 0; i < TEST_BALANCE_RX_BURST_SLAVE_COUNT; i++) {
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[i][0], burst_size[i]);
	}

	/* Call rx burst on bonded device */
	/* Send burst on bonded port */
	nb_rx = rte_eth_rx_burst(test_params->bonded_port_id, 0, rx_pkt_burst,
			MAX_PKT_BURST);
	if (nb_rx != burst_size[0] + burst_size[1] + burst_size[2]) {
		printf("balance rx burst failed\n");
		return -1;
	}

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.ipackets != (uint64_t)(burst_size[0] + burst_size[1] +
			burst_size[2])) {
		printf("Bonded Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->bonded_port_id, (unsigned int)port_stats.ipackets,
				burst_size[0] + burst_size[1] + burst_size[2]);
		return -1;
	}


	/* Verify bonded slave devices rx counts */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.ipackets != (uint64_t)burst_size[0]) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[0],
				(unsigned int)port_stats.ipackets, burst_size[0]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	if (port_stats.ipackets != (uint64_t)burst_size[1]) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[1],
				(unsigned int)port_stats.ipackets, burst_size[1]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	if (port_stats.ipackets != (uint64_t)burst_size[2]) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[2],
				(unsigned int)port_stats.ipackets, burst_size[2]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	if (port_stats.ipackets != 0) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[3],
				(unsigned int)port_stats.ipackets, 0);
		return -1;
	}

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
	int i, promiscuous_en;

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_BALANCE, 4, 1) != 0)
		return -1;

	rte_eth_promiscuous_enable(test_params->bonded_port_id);

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	if (promiscuous_en != 1) {
		printf("Port (%d) promiscuous mode not enabled\n",
				test_params->bonded_port_id);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		if (promiscuous_en != 1) {
			printf("slave port (%d) promiscuous mode not enabled\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	rte_eth_promiscuous_disable(test_params->bonded_port_id);

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	if (promiscuous_en != 0) {
		printf("Port (%d) promiscuous mode not disabled\n",
				test_params->bonded_port_id);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		if (promiscuous_en != 0) {
			printf("slave port (%d) promiscuous mode not disabled\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_balance_verify_mac_assignment(void)
{
	struct ether_addr read_mac_addr, expected_mac_addr_0, expected_mac_addr_1;

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &expected_mac_addr_0);
	rte_eth_macaddr_get(test_params->slave_port_ids[1], &expected_mac_addr_1);

	/* Initialize bonded device with 2 slaves in active backup mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_BALANCE, 2, 1) != 0)
		return -1;

	/* Verify that bonded MACs is that of first slave and that the other slave
	 * MAC hasn't been changed */
	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of primary port\n",
				test_params->bonded_port_id);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not set to that of primary port\n",
				test_params->slave_port_ids[0]);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not set to that of primary port\n",
				test_params->slave_port_ids[1]);
		return -1;
	}

	/* change primary and verify that MAC addresses haven't changed */
	if (rte_eth_bond_primary_set(test_params->bonded_port_id,
			test_params->slave_port_ids[1]) != 0) {
		printf("Failed to set bonded port (%d) primary port to (%d)\n",
				test_params->bonded_port_id, test_params->slave_port_ids[1]);
		return -1;
	}

	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of primary port\n",
				test_params->bonded_port_id);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not set to that of primary port\n",
				test_params->slave_port_ids[0]);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr);
	if (memcmp(&expected_mac_addr_0, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not set to that of primary port\n",
				test_params->slave_port_ids[1]);
		return -1;
	}

	/* stop / start bonded device and verify that primary MAC address is
	 * propagated to bonded device and slaves */

	rte_eth_dev_stop(test_params->bonded_port_id);

	if (rte_eth_dev_start(test_params->bonded_port_id) != 0)
		return -1;

	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(&expected_mac_addr_1, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of primary port\n",
				test_params->bonded_port_id);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr);
	if (memcmp(&expected_mac_addr_1, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not set to that of primary port\n",
				test_params->slave_port_ids[0]);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr);
	if (memcmp(&expected_mac_addr_1, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not set to that of primary port\n",
				test_params->slave_port_ids[1]);
		return -1;
	}

	/* Set explicit MAC address */
	if (rte_eth_bond_mac_address_set(test_params->bonded_port_id,
			(struct ether_addr *)bonded_mac) != 0)
		return -1;

	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(&bonded_mac, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of bonded port\n",
				test_params->bonded_port_id);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &read_mac_addr);
	if (memcmp(&bonded_mac, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not as expected\n",
				test_params->slave_port_ids[0]);
		return -1;
	}

	rte_eth_macaddr_get(test_params->slave_port_ids[1], &read_mac_addr);
	if (memcmp(&bonded_mac, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("slave port (%d) mac address not set to that of bonded port\n",
				test_params->slave_port_ids[1]);
		return -1;
	}

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

	uint8_t slaves[RTE_MAX_ETHPORTS];

	int i, j, burst_size, slave_count;

	memset(pkt_burst, 0, sizeof(pkt_burst));

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_BALANCE,
			TEST_BALANCE_LINK_STATUS_SLAVE_COUNT, 1) != 0)
		return -1;

	if (rte_eth_bond_xmit_policy_set(test_params->bonded_port_id,
			BALANCE_XMIT_POLICY_LAYER2)) {
		printf("Failed to set balance xmit policy.\n");
		return -1;
	}

	/* Verify Current Slaves Count /Active Slave Count is */
	slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS);
	if (slave_count != TEST_BALANCE_LINK_STATUS_SLAVE_COUNT) {
		printf("Number of slaves (%d) is not as expected (%d).\n", slave_count,
				TEST_BALANCE_LINK_STATUS_SLAVE_COUNT);
		return -1;
	}

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	if (slave_count != TEST_BALANCE_LINK_STATUS_SLAVE_COUNT) {
		printf("Number of active slaves (%d) is not as expected (%d).\n",
				slave_count, TEST_BALANCE_LINK_STATUS_SLAVE_COUNT);
		return -1;
	}

	/* Set 2 slaves link status to down */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 0);

	if (rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS) != 2) {
		printf("Number of active slaves (%d) is not as expected (%d).\n",
				slave_count, 2);
		return -1;
	}

	/* Send to sets of packet burst and verify that they are balanced across
	 *  slaves */
	burst_size = 21;

	if (generate_test_burst(&pkt_burst[0][0], burst_size, 0, 1, 0, 0, 0) !=
			burst_size) {
		printf("generate_test_burst failed\n");
		return -1;
	}

	if (generate_test_burst(&pkt_burst[1][0], burst_size, 0, 1, 1, 0, 0) !=
			burst_size) {
		printf("generate_test_burst failed\n");
		return -1;
	}

	if (rte_eth_tx_burst(test_params->bonded_port_id, 0, &pkt_burst[0][0],
			burst_size) != burst_size) {
		printf("rte_eth_tx_burst failed\n");
		return -1;
	}

	if (rte_eth_tx_burst(test_params->bonded_port_id, 0, &pkt_burst[1][0],
			burst_size) != burst_size) {
		printf("rte_eth_tx_burst failed\n");
		return -1;
	}

	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.opackets != (uint64_t)(burst_size + burst_size)) {
		printf("(%d) port_stats.opackets (%d) not as expected (%d).\n",
				test_params->bonded_port_id, (int)port_stats.opackets,
				burst_size + burst_size);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size) {
		printf("(%d) port_stats.opackets (%d) not as expected (%d).\n",
				test_params->slave_port_ids[0], (int)port_stats.opackets,
				burst_size);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size) {
		printf("(%d) port_stats.opackets (%d) not as expected (%d).\n",
				test_params->slave_port_ids[2], (int)port_stats.opackets,
				burst_size);
		return -1;
	}

	/* verify that all packets get send on primary slave when no other slaves
	 * are available */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[2], 0);

	if (rte_eth_bond_active_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS) != 1) {
		printf("Number of active slaves (%d) is not as expected (%d).\n",
				slave_count, 1);
		return -1;
	}

	if (generate_test_burst(&pkt_burst[1][0], burst_size, 0, 1, 1, 0, 0) !=
			burst_size) {
		printf("generate_test_burst failed\n");
		return -1;
	}

	if (rte_eth_tx_burst(test_params->bonded_port_id, 0, &pkt_burst[1][0],
			burst_size) != burst_size) {
		printf("rte_eth_tx_burst failed\n");
		return -1;
	}

	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.opackets != (uint64_t)(burst_size + burst_size +
			burst_size)) {
		printf("(%d) port_stats.opackets (%d) not as expected (%d).\n",
				test_params->bonded_port_id, (int)port_stats.opackets,
				burst_size + burst_size + burst_size);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.opackets != (uint64_t)(burst_size + burst_size)) {
		printf("(%d) port_stats.opackets (%d) not as expected (%d).\n",
				test_params->slave_port_ids[0], (int)port_stats.opackets,
				burst_size + burst_size);
		return -1;
	}


	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[0], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 1);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[2], 1);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 1);

	for (i = 0; i < TEST_BALANCE_LINK_STATUS_SLAVE_COUNT; i++) {
		if (generate_test_burst(&pkt_burst[i][0], burst_size, 0, 1, 0, 0, 0) !=
				burst_size)
			return -1;

		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&pkt_burst[i][0], burst_size);
	}



	/* Verify that pkts are not received on slaves with link status down */

	rte_eth_rx_burst(test_params->bonded_port_id, 0, rx_pkt_burst,
			MAX_PKT_BURST);

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.ipackets != (uint64_t)(burst_size * 3)) {
		printf("(%d) port_stats.ipackets (%d) not as expected (%d)\n",
				test_params->bonded_port_id, (int)port_stats.ipackets,
				burst_size * 3);
		return -1;
	}

	/* free mbufs allocate for rx testing */
	for (i = 0; i < TEST_BALANCE_RX_BURST_SLAVE_COUNT; i++) {
		for (j = 0; j < MAX_PKT_BURST; j++) {
			if (pkt_burst[i][j] != NULL) {
				rte_pktmbuf_free(pkt_burst[i][j]);
				pkt_burst[i][j] = NULL;
			}
		}
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

/** Broadcast Mode Tests */

static int
test_broadcast_tx_burst(void)
{
	int i, pktlen, retval, burst_size, generated_burst_size, nb_tx;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	struct rte_eth_stats port_stats;

	retval = initialize_bonded_device_with_slaves(BONDING_MODE_BROADCAST, 2, 1);
	if (retval != 0) {
		printf("Failed to initialize_bonded_device_with_slaves.\n");
		return -1;
	}

	initialize_eth_header(test_params->pkt_eth_hdr,
			(struct ether_addr *)src_mac, (struct ether_addr *)dst_mac_0, 0, 0);

	pktlen = initialize_udp_header(test_params->pkt_udp_hdr, src_port,
			dst_port_0, 16);
	pktlen = initialize_ipv4_header(test_params->pkt_ipv4_hdr, src_addr,
			dst_addr_0, pktlen);

	burst_size = 20 * test_params->bonded_slave_count;

	if (burst_size > MAX_PKT_BURST) {
		printf("Burst size specified is greater than supported.\n");
		return -1;
	}

	/* Generate a burst of packets to transmit */
	generated_burst_size = generate_packet_burst(test_params->mbuf_pool,
			pkts_burst,	test_params->pkt_eth_hdr, 0, test_params->pkt_ipv4_hdr,
			1, test_params->pkt_udp_hdr, burst_size);
	if (generated_burst_size != burst_size)
		return -1;

	/* Send burst on bonded port */
	nb_tx = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst,
			burst_size);
	if (nb_tx != burst_size * test_params->bonded_slave_count) {
		printf("Bonded Port (%d) rx burst failed, packets transmitted value (%u) not as expected (%d)\n",
				test_params->bonded_port_id,
				nb_tx, burst_size);
		return -1;
	}

	/* Verify bonded port tx stats */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size *
			test_params->bonded_slave_count) {
		printf("Bonded Port (%d) opackets value (%u) not as expected (%d)\n",
				test_params->bonded_port_id, (unsigned int)port_stats.opackets,
				burst_size);
	}

	/* Verify slave ports tx stats */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_stats_get(test_params->slave_port_ids[i], &port_stats);
		if (port_stats.opackets != (uint64_t)burst_size) {
			printf("Slave Port (%d) opackets value (%u) not as expected (%d)\n",
					test_params->bonded_port_id,
					(unsigned int)port_stats.opackets, burst_size);
		}
	}

	/* Put all slaves down and try and transmit */
	for (i = 0; i < test_params->bonded_slave_count; i++) {

		virtual_ethdev_simulate_link_status_interrupt(
				test_params->slave_port_ids[i], 0);
	}

	/* Send burst on bonded port */
	nb_tx = rte_eth_tx_burst(test_params->bonded_port_id, 0, pkts_burst,
			burst_size);
	if (nb_tx != 0)
		return -1;

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
	int i, j, nb_rx;

	memset(gen_pkt_burst, 0, sizeof(gen_pkt_burst));

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_BROADCAST, 3, 1) != 0)
		return -1;


	/* Generate test bursts of packets to transmit */
	for (i = 0; i < BROADCAST_RX_BURST_NUM_OF_SLAVES; i++) {
		if (generate_test_burst(&gen_pkt_burst[i][0], burst_size[i], 0, 0, 1, 0,
				0) != burst_size[i])
			return -1;
	}

	/* Add rx data to slave 0 */
	for (i = 0; i < BROADCAST_RX_BURST_NUM_OF_SLAVES; i++) {
		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&gen_pkt_burst[i][0], burst_size[i]);
	}


	/* Call rx burst on bonded device */
	/* Send burst on bonded port */
	nb_rx = rte_eth_rx_burst(test_params->bonded_port_id, 0, rx_pkt_burst,
			MAX_PKT_BURST);
	if (nb_rx != burst_size[0] + burst_size[1] + burst_size[2]) {
		printf("round-robin rx burst failed");
		return -1;
	}

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.ipackets != (uint64_t)(burst_size[0] + burst_size[1] +
			burst_size[2])) {
		printf("Bonded Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->bonded_port_id, (unsigned int)port_stats.ipackets,
				burst_size[0] + burst_size[1] + burst_size[2]);
		return -1;
	}


	/* Verify bonded slave devices rx counts */
	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.ipackets != (uint64_t)burst_size[0]) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[0],
				(unsigned int)port_stats.ipackets, burst_size[0]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	if (port_stats.ipackets != (uint64_t)burst_size[1]) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[1],
				(unsigned int)port_stats.ipackets, burst_size[1]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	if (port_stats.ipackets != (uint64_t)burst_size[2]) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[2],
				(unsigned int)port_stats.ipackets,
				burst_size[2]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	if (port_stats.ipackets != 0) {
		printf("Slave Port (%d) ipackets value (%u) not as expected (%d)\n",
				test_params->slave_port_ids[3],
				(unsigned int)port_stats.ipackets, 0);
		return -1;
	}

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
	int i, promiscuous_en;

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_BALANCE, 4, 1) != 0)
		return -1;

	rte_eth_promiscuous_enable(test_params->bonded_port_id);

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	if (promiscuous_en != 1) {
		printf("Port (%d) promiscuous mode not enabled\n",
				test_params->bonded_port_id);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		if (promiscuous_en != 1) {
			printf("slave port (%d) promiscuous mode not enabled\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	rte_eth_promiscuous_disable(test_params->bonded_port_id);

	promiscuous_en = rte_eth_promiscuous_get(test_params->bonded_port_id);
	if (promiscuous_en != 0) {
		printf("Port (%d) promiscuous mode not disabled\n",
				test_params->bonded_port_id);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		promiscuous_en = rte_eth_promiscuous_get(
				test_params->slave_port_ids[i]);
		if (promiscuous_en != 0) {
			printf("slave port (%d) promiscuous mode not disabled\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_broadcast_verify_mac_assignment(void)
{
	struct ether_addr read_mac_addr, expected_mac_addr_0, expected_mac_addr_1;

	int i, retval;

	rte_eth_macaddr_get(test_params->slave_port_ids[0], &expected_mac_addr_0);
	rte_eth_macaddr_get(test_params->slave_port_ids[2], &expected_mac_addr_1);

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_BROADCAST, 4, 1) != 0)
		return -1;

	/* Verify that all MACs are the same as first slave added to bonded
	 * device */
	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr);
		if (memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr))) {
			printf("slave port (%d) mac address not set to that of primary port\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	/* change primary and verify that MAC addresses haven't changed */
	retval = rte_eth_bond_primary_set(test_params->bonded_port_id,
			test_params->slave_port_ids[2]);
	if (retval != 0) {
		printf("Failed to set bonded port (%d) primary port to (%d)\n",
				test_params->bonded_port_id, test_params->slave_port_ids[i]);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr);
		if (memcmp(&expected_mac_addr_0, &read_mac_addr,
				sizeof(read_mac_addr))) {
			printf("slave port (%d) mac address has changed to that of primary port without stop/start toggle of bonded device\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	/* stop / start bonded device and verify that primary MAC address is
	 * propagated to bonded device and slaves */

	rte_eth_dev_stop(test_params->bonded_port_id);

	if (rte_eth_dev_start(test_params->bonded_port_id) != 0)
		return -1;

	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(&expected_mac_addr_1, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of new primary port\n",
				test_params->slave_port_ids[i]);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr);
		if (memcmp(&expected_mac_addr_1, &read_mac_addr,
				sizeof(read_mac_addr))) {
			printf("slave port (%d) mac address not set to that of new primary port\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
	}

	/* Set explicit MAC address */
	if (rte_eth_bond_mac_address_set(test_params->bonded_port_id,
			(struct ether_addr *)bonded_mac) != 0)
		return -1;

	rte_eth_macaddr_get(test_params->bonded_port_id, &read_mac_addr);
	if (memcmp(bonded_mac, &read_mac_addr, sizeof(read_mac_addr))) {
		printf("bonded port (%d) mac address not set to that of new primary port\n",
				test_params->slave_port_ids[i]);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++) {
		rte_eth_macaddr_get(test_params->slave_port_ids[i], &read_mac_addr);
		if (memcmp(bonded_mac, &read_mac_addr, sizeof(read_mac_addr))) {
			printf("slave port (%d) mac address not set to that of new primary port\n",
					test_params->slave_port_ids[i]);
			return -1;
		}
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

	uint8_t slaves[RTE_MAX_ETHPORTS];

	int i, j, burst_size, slave_count;

	memset(pkt_burst, 0, sizeof(pkt_burst));

	/* Initialize bonded device with 4 slaves in round robin mode */
	if (initialize_bonded_device_with_slaves(BONDING_MODE_BROADCAST,
			BROADCAST_LINK_STATUS_NUM_OF_SLAVES, 1) != 0)
		return -1;

	/* Verify Current Slaves Count /Active Slave Count is */
	slave_count = rte_eth_bond_slaves_get(test_params->bonded_port_id, slaves,
			RTE_MAX_ETHPORTS);
	if (slave_count != 4) {
		printf("Number of slaves (%d) is not as expected (%d).\n",
				slave_count, 4);
		return -1;
	}

	slave_count = rte_eth_bond_active_slaves_get(
			test_params->bonded_port_id, slaves, RTE_MAX_ETHPORTS);
	if (slave_count != 4) {
		printf("Number of active slaves (%d) is not as expected (%d).\n",
				slave_count, 4);
		return -1;
	}

	/* Set 2 slaves link status to down */
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[1], 0);
	virtual_ethdev_simulate_link_status_interrupt(
			test_params->slave_port_ids[3], 0);

	slave_count = rte_eth_bond_active_slaves_get(test_params->bonded_port_id,
			slaves, RTE_MAX_ETHPORTS);
	if (slave_count != 2) {
		printf("Number of active slaves (%d) is not as expected (%d).\n",
				slave_count, 2);
		return -1;
	}

	for (i = 0; i < test_params->bonded_slave_count; i++)
		rte_eth_stats_reset(test_params->slave_port_ids[i]);

	/* Verify that pkts are not sent on slaves with link status down */
	burst_size = 21;

	if (generate_test_burst(&pkt_burst[0][0], burst_size, 0, 0, 1, 0, 0) !=
			burst_size) {
		printf("generate_test_burst failed\n");
		return -1;
	}

	if (rte_eth_tx_burst(test_params->bonded_port_id, 0, &pkt_burst[0][0],
			burst_size) != (burst_size * slave_count)) {
		printf("rte_eth_tx_burst failed\n");
		return -1;
	}

	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.opackets != (uint64_t)(burst_size * slave_count)) {
		printf("(%d) port_stats.opackets (%d) not as expected (%d)\n",
				test_params->bonded_port_id, (int)port_stats.opackets,
				burst_size * slave_count);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[0], &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[0]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[1], &port_stats);
	if (port_stats.opackets != 0) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[1]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[2], &port_stats);
	if (port_stats.opackets != (uint64_t)burst_size) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[2]);
		return -1;
	}

	rte_eth_stats_get(test_params->slave_port_ids[3], &port_stats);
	if (port_stats.opackets != 0) {
		printf("(%d) port_stats.opackets not as expected\n",
				test_params->slave_port_ids[3]);
		return -1;
	}

	for (i = 0; i < BROADCAST_LINK_STATUS_NUM_OF_SLAVES; i++) {
		if (generate_test_burst(&pkt_burst[i][0], burst_size, 0, 0, 1, 0, 0) !=
				burst_size) {
			return -1;
		}

		virtual_ethdev_add_mbufs_to_rx_queue(test_params->slave_port_ids[i],
				&pkt_burst[i][0], burst_size);
	}

	/* Verify that pkts are not received on slaves with link status down */

	if (rte_eth_rx_burst(test_params->bonded_port_id, 0, rx_pkt_burst,
			MAX_PKT_BURST) !=
			burst_size + burst_size) {
		printf("rte_eth_rx_burst\n");
		return -1;
	}

	/* Verify bonded device rx count */
	rte_eth_stats_get(test_params->bonded_port_id, &port_stats);
	if (port_stats.ipackets != (uint64_t)(burst_size + burst_size)) {
		printf("(%d) port_stats.ipackets not as expected\n",
				test_params->bonded_port_id);
		return -1;
	}

	/* free mbufs allocate for rx testing */
	for (i = 0; i < BROADCAST_LINK_STATUS_NUM_OF_SLAVES; i++) {
		for (j = 0; j < MAX_PKT_BURST; j++) {
			if (pkt_burst[i][j] != NULL) {
				rte_pktmbuf_free(pkt_burst[i][j]);
				pkt_burst[i][j] = NULL;
			}
		}
	}

	/* Clean up and remove slaves from bonded device */
	return remove_slaves_and_stop_bonded_device();
}

static int
test_reconfigure_bonded_device(void)
{
	test_params->nb_rx_q = 4;
	test_params->nb_tx_q = 4;

	if (configure_ethdev(test_params->bonded_port_id, 0)  != 0) {
		printf("failed to reconfigure bonded device");
		return -1;
	}


	test_params->nb_rx_q = 2;
	test_params->nb_tx_q = 2;

	if (configure_ethdev(test_params->bonded_port_id, 0)  != 0) {
		printf("failed to reconfigure bonded device with less rx/tx queues");
		return -1;
	}

	return 0;
}


static int
test_close_bonded_device(void)
{
	rte_eth_dev_close(test_params->bonded_port_id);
	return 0;
}

static int
testsuite_teardown(void)
{
	if (test_params->pkt_eth_hdr != NULL)
		free(test_params->pkt_eth_hdr);

	return 0;
}

struct unittest {
	int (*test_function)(void);
	const char *success_msg;
	const char *fail_msg;
};

struct unittest_suite {
	int (*setup_function)(void);
	int (*teardown_function)(void);
	struct unittest unittests[];
};

static struct unittest_suite link_bonding_test_suite  = {
	.setup_function = test_setup,
	.teardown_function = testsuite_teardown,
	.unittests = {
		{ test_create_bonded_device, "test_create_bonded_device succeeded",
			"test_create_bonded_device failed" },
		{ test_create_bonded_device_with_invalid_params,
			"test_create_bonded_device_with_invalid_params succeeded",
			"test_create_bonded_device_with_invalid_params failed" },
		{ test_add_slave_to_bonded_device,
			"test_add_slave_to_bonded_device succeeded",
			"test_add_slave_to_bonded_device failed" },
		{ test_add_slave_to_invalid_bonded_device,
			"test_add_slave_to_invalid_bonded_device succeeded",
			"test_add_slave_to_invalid_bonded_device failed" },
		{ test_remove_slave_from_bonded_device,
			"test_remove_slave_from_bonded_device succeeded ",
			"test_remove_slave_from_bonded_device failed" },
		{ test_remove_slave_from_invalid_bonded_device,
			"test_remove_slave_from_invalid_bonded_device succeeded",
			"test_remove_slave_from_invalid_bonded_device failed" },
		{ test_get_slaves_from_bonded_device,
			"test_get_slaves_from_bonded_device succeeded",
			"test_get_slaves_from_bonded_device failed" },
		{ test_add_already_bonded_slave_to_bonded_device,
			"test_add_already_bonded_slave_to_bonded_device succeeded",
			"test_add_already_bonded_slave_to_bonded_device failed" },
		{ test_add_remove_multiple_slaves_to_from_bonded_device,
			"test_add_remove_multiple_slaves_to_from_bonded_device succeeded",
			"test_add_remove_multiple_slaves_to_from_bonded_device failed" },
		{ test_start_bonded_device,
			"test_start_bonded_device succeeded",
			"test_start_bonded_device failed" },
		{ test_stop_bonded_device,
			"test_stop_bonded_device succeeded",
			"test_stop_bonded_device failed" },
		{ test_set_bonding_mode,
			"test_set_bonding_mode succeeded",
			"test_set_bonding_mode failed" },
		{ test_set_primary_slave,
			"test_set_primary_slave succeeded",
			"test_set_primary_slave failed" },
		{ test_set_explicit_bonded_mac,
			"test_set_explicit_bonded_mac succeeded",
			"test_set_explicit_bonded_mac failed" },
		{ test_adding_slave_after_bonded_device_started,
			"test_adding_slave_after_bonded_device_started succeeded",
			"test_adding_slave_after_bonded_device_started failed" },
		{ test_roundrobin_tx_burst,
			"test_roundrobin_tx_burst succeeded",
			"test_roundrobin_tx_burst failed" },
		{ test_roundrobin_rx_burst_on_single_slave,
			"test_roundrobin_rx_burst_on_single_slave succeeded",
			"test_roundrobin_rx_burst_on_single_slave failed" },
		{ test_roundrobin_rx_burst_on_multiple_slaves,
			"test_roundrobin_rx_burst_on_multiple_slaves succeeded",
			"test_roundrobin_rx_burst_on_multiple_slaves failed" },
		{ test_roundrobin_verify_promiscuous_enable_disable,
			"test_roundrobin_verify_promiscuous_enable_disable succeeded",
			"test_roundrobin_verify_promiscuous_enable_disable failed" },
		{ test_roundrobin_verify_mac_assignment,
			"test_roundrobin_verify_mac_assignment succeeded",
			"test_roundrobin_verify_mac_assignment failed" },
		{ test_roundrobin_verify_slave_link_status_change_behaviour,
			"test_roundrobin_verify_slave_link_status_change_behaviour succeeded",
			"test_roundrobin_verify_slave_link_status_change_behaviour failed" },
		{ test_activebackup_tx_burst,
			"test_activebackup_tx_burst succeeded",
			"test_activebackup_tx_burst failed" },
		{ test_activebackup_rx_burst,
			"test_activebackup_rx_burst succeeded",
			"test_activebackup_rx_burst failed" },
		{ test_activebackup_verify_promiscuous_enable_disable,
			"test_activebackup_verify_promiscuous_enable_disable succeeded",
			"test_activebackup_verify_promiscuous_enable_disable failed" },
		{ test_activebackup_verify_mac_assignment,
			"test_activebackup_verify_mac_assignment succeeded",
			"test_activebackup_verify_mac_assignment failed" },
		{ test_activebackup_verify_slave_link_status_change_failover,
			"test_activebackup_verify_slave_link_status_change_failover succeeded",
			"test_activebackup_verify_slave_link_status_change_failover failed" },
		{ test_balance_xmit_policy_configuration,
			"test_balance_xmit_policy_configuration succeeded",
			"test_balance_xmit_policy_configuration failed" },
		{ test_balance_l2_tx_burst,
			"test_balance_l2_tx_burst succeeded",
			"test_balance_l2_tx_burst failed" },
		{ test_balance_l23_tx_burst_ipv4_toggle_ip_addr,
			"test_balance_l23_tx_burst_ipv4_toggle_ip_addr succeeded",
			"test_balance_l23_tx_burst_ipv4_toggle_ip_addr failed" },
		{ test_balance_l23_tx_burst_vlan_ipv4_toggle_ip_addr,
			"test_balance_l23_tx_burst_vlan_ipv4_toggle_ip_addr succeeded",
			"test_balance_l23_tx_burst_vlan_ipv4_toggle_ip_addr failed" },
		{ test_balance_l23_tx_burst_ipv6_toggle_ip_addr,
			"test_balance_l23_tx_burst_ipv6_toggle_ip_addr succeeded",
			"test_balance_l23_tx_burst_ipv6_toggle_ip_addr failed" },
		{ test_balance_l23_tx_burst_vlan_ipv6_toggle_ip_addr,
			"test_balance_l23_tx_burst_vlan_ipv6_toggle_ip_addr succeeded",
			"test_balance_l23_tx_burst_vlan_ipv6_toggle_ip_addr failed" },
		{ test_balance_l23_tx_burst_toggle_mac_addr,
			"test_balance_l23_tx_burst_toggle_mac_addr succeeded",
			"test_balance_l23_tx_burst_toggle_mac_addr failed" },
		{ test_balance_l34_tx_burst_ipv4_toggle_ip_addr,
			"test_balance_l34_tx_burst_ipv4_toggle_ip_addr succeeded",
			"test_balance_l34_tx_burst_ipv4_toggle_ip_addr failed" },
		{ test_balance_l34_tx_burst_ipv4_toggle_udp_port,
			"test_balance_l34_tx_burst_ipv4_toggle_udp_port succeeded",
			"test_balance_l34_tx_burst_ipv4_toggle_udp_port failed" },
		{ test_balance_l34_tx_burst_vlan_ipv4_toggle_ip_addr,
			"test_balance_l34_tx_burst_vlan_ipv4_toggle_ip_addr succeeded",
			"test_balance_l34_tx_burst_vlan_ipv4_toggle_ip_addr failed" },
		{ test_balance_l34_tx_burst_ipv6_toggle_ip_addr,
			"test_balance_l34_tx_burst_ipv6_toggle_ip_addr succeeded",
			"test_balance_l34_tx_burst_ipv6_toggle_ip_addr failed" },
		{ test_balance_l34_tx_burst_vlan_ipv6_toggle_ip_addr,
			"test_balance_l34_tx_burst_vlan_ipv6_toggle_ip_addr succeeded",
			"test_balance_l34_tx_burst_vlan_ipv6_toggle_ip_addr failed" },
		{ test_balance_l34_tx_burst_ipv6_toggle_udp_port,
			"test_balance_l34_tx_burst_ipv6_toggle_udp_port succeeded",
			"test_balance_l34_tx_burst_ipv6_toggle_udp_port failed" },
		{ test_balance_rx_burst,
			"test_balance_rx_burst succeeded",
			"test_balance_rx_burst failed" },
		{ test_balance_verify_promiscuous_enable_disable,
			"test_balance_verify_promiscuous_enable_disable succeeded",
			"test_balance_verify_promiscuous_enable_disable failed" },
		{ test_balance_verify_mac_assignment,
			"test_balance_verify_mac_assignment succeeded",
			"test_balance_verify_mac_assignment failed" },
		{ test_balance_verify_slave_link_status_change_behaviour,
			"test_balance_verify_slave_link_status_change_behaviour succeeded",
			"test_balance_verify_slave_link_status_change_behaviour failed" },
		{ test_broadcast_tx_burst,
			"test_broadcast_tx_burst succeeded",
			"test_broadcast_tx_burst failed" },
		{ test_broadcast_rx_burst,
			"test_broadcast_rx_burst succeeded",
			"test_broadcast_rx_burst failed" },
		{ test_broadcast_verify_promiscuous_enable_disable,
			"test_broadcast_verify_promiscuous_enable_disable succeeded",
			"test_broadcast_verify_promiscuous_enable_disable failed" },
		{ test_broadcast_verify_mac_assignment,
			"test_broadcast_verify_mac_assignment succeeded",
			"test_broadcast_verify_mac_assignment failed" },
		{ test_broadcast_verify_slave_link_status_change_behaviour,
			"test_broadcast_verify_slave_link_status_change_behaviour succeeded",
			"test_broadcast_verify_slave_link_status_change_behaviour failed" },
		{ test_reconfigure_bonded_device,
			"test_reconfigure_bonded_device succeeded",
			"test_reconfigure_bonded_device failed" },
		{ test_close_bonded_device,
			"test_close_bonded_device succeeded",
			"test_close_bonded_device failed" },

		{ NULL , NULL, NULL } /**< NULL terminate unit test array */
	}
};


int
test_link_bonding(void)
{
	int i = 0;

	if (link_bonding_test_suite.setup_function) {
		if (link_bonding_test_suite.setup_function() != 0)
			return -1;
	}

	while (link_bonding_test_suite.unittests[i].test_function) {
		if (link_bonding_test_suite.unittests[i].test_function() == 0) {
			printf("%s", link_bonding_test_suite.unittests[i].success_msg ?
					link_bonding_test_suite.unittests[i].success_msg :
					"unit test succeeded");
		} else {
			printf("%s", link_bonding_test_suite.unittests[i].fail_msg ?
					link_bonding_test_suite.unittests[i].fail_msg :
					"unit test failed");
			return -1;
		}
		printf("\n");
		i++;
	}

	if (link_bonding_test_suite.teardown_function) {
		if (link_bonding_test_suite.teardown_function() != 0)
			return -1;
	}

	return 0;
}
