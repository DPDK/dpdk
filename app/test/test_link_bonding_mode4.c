/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <rte_cycles.h>
#include <sys/queue.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_lcore.h>
#include <rte_memory.h>

#include <rte_string_fns.h>

#include <rte_eth_ring.h>
#include <rte_errno.h>
#include <rte_eth_bond.h>
#include <rte_eth_bond_8023ad.h>

#include "packet_burst_generator.h"

#include "test.h"

#define SLAVE_COUNT (4)

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define MBUF_CACHE_SIZE         (250)
#define BURST_SIZE              (32)

#define TEST_RX_DESC_MAX        (2048)
#define TEST_TX_DESC_MAX        (2048)
#define MAX_PKT_BURST           (32)
#define DEF_PKT_BURST           (16)

#define BONDED_DEV_NAME         ("net_bonding_m4_bond_dev")

#define SLAVE_DEV_NAME_FMT      ("net_virt_%d")
#define SLAVE_RX_QUEUE_FMT      ("net_virt_%d_rx")
#define SLAVE_TX_QUEUE_FMT      ("net_virt_%d_tx")

#define INVALID_SOCKET_ID       (-1)
#define INVALID_PORT_ID         (0xFF)
#define INVALID_BONDING_MODE    (-1)

static const struct rte_ether_addr slave_mac_default = {
	{ 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00 }
};

static const struct rte_ether_addr parnter_mac_default = {
	{ 0x22, 0xBB, 0xFF, 0xBB, 0x00, 0x00 }
};

static const struct rte_ether_addr parnter_system = {
	{ 0x33, 0xFF, 0xBB, 0xFF, 0x00, 0x00 }
};

static const struct rte_ether_addr slow_protocol_mac_addr = {
	{ 0x01, 0x80, 0xC2, 0x00, 0x00, 0x02 }
};

struct slave_conf {
	struct rte_ring *rx_queue;
	struct rte_ring *tx_queue;
	uint16_t port_id;
	uint8_t bonded : 1;

	uint8_t lacp_parnter_state;
};

struct ether_vlan_hdr {
	struct rte_ether_hdr pkt_eth_hdr;
	struct rte_vlan_hdr vlan_hdr;
};

struct link_bonding_unittest_params {
	uint8_t bonded_port_id;
	struct slave_conf slave_ports[SLAVE_COUNT];

	struct rte_mempool *mbuf_pool;
};

#define TEST_DEFAULT_SLAVE_COUNT     RTE_DIM(test_params.slave_ports)
#define TEST_RX_SLAVE_COUT           TEST_DEFAULT_SLAVE_COUNT
#define TEST_TX_SLAVE_COUNT          TEST_DEFAULT_SLAVE_COUNT
#define TEST_MARKER_SLAVE_COUT       TEST_DEFAULT_SLAVE_COUNT
#define TEST_EXPIRED_SLAVE_COUNT     TEST_DEFAULT_SLAVE_COUNT
#define TEST_PROMISC_SLAVE_COUNT     TEST_DEFAULT_SLAVE_COUNT

static struct link_bonding_unittest_params test_params  = {
	.bonded_port_id = INVALID_PORT_ID,
	.slave_ports = { [0 ... SLAVE_COUNT - 1] = { .port_id = INVALID_PORT_ID} },

	.mbuf_pool = NULL,
};

static struct rte_eth_conf default_pmd_conf = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_NONE,
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
		.split_hdr_size = 0,
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
	.lpbk_mode = 0,
};

static uint8_t lacpdu_rx_count[RTE_MAX_ETHPORTS] = {0, };

#define FOR_EACH(_i, _item, _array, _size) \
	for (_i = 0, _item = &_array[0]; _i < _size && (_item = &_array[_i]); _i++)

/* Macro for iterating over every port that can be used as a slave
 * in this test.
 * _i variable used as an index in test_params->slave_ports
 * _slave pointer to &test_params->slave_ports[_idx]
 */
#define FOR_EACH_PORT(_i, _port) \
	FOR_EACH(_i, _port, test_params.slave_ports, \
		RTE_DIM(test_params.slave_ports))

/* Macro for iterating over every port that can be used as a slave
 * in this test and satisfy given condition.
 *
 * _i variable used as an index in test_params->slave_ports
 * _slave pointer to &test_params->slave_ports[_idx]
 * _condition condition that need to be checked
 */
#define FOR_EACH_PORT_IF(_i, _port, _condition) FOR_EACH_PORT((_i), (_port)) \
	if (!!(_condition))

/* Macro for iterating over every port that is currently a slave of a bonded
 * device.
 * _i variable used as an index in test_params->slave_ports
 * _slave pointer to &test_params->slave_ports[_idx]
 * */
#define FOR_EACH_SLAVE(_i, _slave) \
	FOR_EACH_PORT_IF(_i, _slave, (_slave)->bonded != 0)

/*
 * Returns packets from slaves TX queue.
 * slave slave port
 * buffer for packets
 * size size of buffer
 * return number of packets or negative error number
 */
static int
slave_get_pkts(struct slave_conf *slave, struct rte_mbuf **buf, uint16_t size)
{
	return rte_ring_dequeue_burst(slave->tx_queue, (void **)buf,
			size, NULL);
}

/*
 * Injects given packets into slaves RX queue.
 * slave slave port
 * buffer for packets
 * size number of packets to be injected
 * return number of queued packets or negative error number
 */
static int
slave_put_pkts(struct slave_conf *slave, struct rte_mbuf **buf, uint16_t size)
{
	return rte_ring_enqueue_burst(slave->rx_queue, (void **)buf,
			size, NULL);
}

static uint16_t
bond_rx(struct rte_mbuf **buf, uint16_t size)
{
	return rte_eth_rx_burst(test_params.bonded_port_id, 0, buf, size);
}

static uint16_t
bond_tx(struct rte_mbuf **buf, uint16_t size)
{
	return rte_eth_tx_burst(test_params.bonded_port_id, 0, buf, size);
}

static void
free_pkts(struct rte_mbuf **pkts, uint16_t count)
{
	uint16_t i;

	for (i = 0; i < count; i++) {
		if (pkts[i] != NULL)
			rte_pktmbuf_free(pkts[i]);
	}
}

static int
configure_ethdev(uint16_t port_id, uint8_t start)
{
	TEST_ASSERT(rte_eth_dev_configure(port_id, 1, 1, &default_pmd_conf) == 0,
		"Failed to configure device %u", port_id);

	TEST_ASSERT(rte_eth_rx_queue_setup(port_id, 0, RX_RING_SIZE,
		rte_eth_dev_socket_id(port_id), NULL, test_params.mbuf_pool) == 0,
		"Failed to setup rx queue.");

	TEST_ASSERT(rte_eth_tx_queue_setup(port_id, 0, TX_RING_SIZE,
		rte_eth_dev_socket_id(port_id), NULL) == 0,
		"Failed to setup tx queue.");

	if (start) {
		TEST_ASSERT(rte_eth_dev_start(port_id) == 0,
		"Failed to start device (%d).", port_id);
	}
	return 0;
}

static int
add_slave(struct slave_conf *slave, uint8_t start)
{
	struct rte_ether_addr addr, addr_check;
	int retval;

	/* Some sanity check */
	RTE_VERIFY(test_params.slave_ports <= slave &&
		slave - test_params.slave_ports < (int)RTE_DIM(test_params.slave_ports));
	RTE_VERIFY(slave->bonded == 0);
	RTE_VERIFY(slave->port_id != INVALID_PORT_ID);

	rte_ether_addr_copy(&slave_mac_default, &addr);
	addr.addr_bytes[RTE_ETHER_ADDR_LEN - 1] = slave->port_id;

	rte_eth_dev_mac_addr_remove(slave->port_id, &addr);

	TEST_ASSERT_SUCCESS(rte_eth_dev_mac_addr_add(slave->port_id, &addr, 0),
		"Failed to set slave MAC address");

	TEST_ASSERT_SUCCESS(rte_eth_bond_slave_add(test_params.bonded_port_id,
		slave->port_id),
			"Failed to add slave (idx=%u, id=%u) to bonding (id=%u)",
			(uint8_t)(slave - test_params.slave_ports), slave->port_id,
			test_params.bonded_port_id);

	slave->bonded = 1;
	if (start) {
		TEST_ASSERT_SUCCESS(rte_eth_dev_start(slave->port_id),
			"Failed to start slave %u", slave->port_id);
	}

	retval = rte_eth_macaddr_get(slave->port_id, &addr_check);
	TEST_ASSERT_SUCCESS(retval, "Failed to get slave mac address: %s",
			    strerror(-retval));
	TEST_ASSERT_EQUAL(rte_is_same_ether_addr(&addr, &addr_check), 1,
			"Slave MAC address is not as expected");

	RTE_VERIFY(slave->lacp_parnter_state == 0);
	return 0;
}

static int
remove_slave(struct slave_conf *slave)
{
	ptrdiff_t slave_idx = slave - test_params.slave_ports;

	RTE_VERIFY(test_params.slave_ports <= slave &&
		slave_idx < (ptrdiff_t)RTE_DIM(test_params.slave_ports));

	RTE_VERIFY(slave->bonded == 1);
	RTE_VERIFY(slave->port_id != INVALID_PORT_ID);

	TEST_ASSERT_EQUAL(rte_ring_count(slave->rx_queue), 0,
		"Slave %u tx queue not empty while removing from bonding.",
		slave->port_id);

	TEST_ASSERT_EQUAL(rte_ring_count(slave->rx_queue), 0,
		"Slave %u tx queue not empty while removing from bonding.",
		slave->port_id);

	TEST_ASSERT_EQUAL(rte_eth_bond_slave_remove(test_params.bonded_port_id,
			slave->port_id), 0,
			"Failed to remove slave (idx=%u, id=%u) from bonding (id=%u)",
			(uint8_t)slave_idx, slave->port_id,
			test_params.bonded_port_id);

	slave->bonded = 0;
	slave->lacp_parnter_state = 0;
	return 0;
}

static void
lacp_recv_cb(uint16_t slave_id, struct rte_mbuf *lacp_pkt)
{
	struct rte_ether_hdr *hdr;
	struct slow_protocol_frame *slow_hdr;

	RTE_VERIFY(lacp_pkt != NULL);

	hdr = rte_pktmbuf_mtod(lacp_pkt, struct rte_ether_hdr *);
	RTE_VERIFY(hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_SLOW));

	slow_hdr = rte_pktmbuf_mtod(lacp_pkt, struct slow_protocol_frame *);
	RTE_VERIFY(slow_hdr->slow_protocol.subtype == SLOW_SUBTYPE_LACP);

	lacpdu_rx_count[slave_id]++;
	rte_pktmbuf_free(lacp_pkt);
}

static int
initialize_bonded_device_with_slaves(uint16_t slave_count, uint8_t external_sm)
{
	uint8_t i;
	int ret;

	RTE_VERIFY(test_params.bonded_port_id != INVALID_PORT_ID);

	for (i = 0; i < slave_count; i++) {
		TEST_ASSERT_SUCCESS(add_slave(&test_params.slave_ports[i], 1),
			"Failed to add port %u to bonded device.\n",
			test_params.slave_ports[i].port_id);
	}

	/* Reset mode 4 configuration */
	rte_eth_bond_8023ad_setup(test_params.bonded_port_id, NULL);
	ret = rte_eth_promiscuous_disable(test_params.bonded_port_id);
	TEST_ASSERT_SUCCESS(ret,
		"Failed disable promiscuous mode for port %d: %s",
		test_params.bonded_port_id, rte_strerror(-ret));

	if (external_sm) {
		struct rte_eth_bond_8023ad_conf conf;

		rte_eth_bond_8023ad_conf_get(test_params.bonded_port_id, &conf);
		conf.slowrx_cb = lacp_recv_cb;
		rte_eth_bond_8023ad_setup(test_params.bonded_port_id, &conf);

	}

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params.bonded_port_id),
		"Failed to start bonded device");

	return TEST_SUCCESS;
}

static int
remove_slaves_and_stop_bonded_device(void)
{
	struct slave_conf *slave;
	int retval;
	uint16_t slaves[RTE_MAX_ETHPORTS];
	uint16_t i;

	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params.bonded_port_id),
			"Failed to stop bonded port %u",
			test_params.bonded_port_id);

	FOR_EACH_SLAVE(i, slave)
		remove_slave(slave);

	retval = rte_eth_bond_slaves_get(test_params.bonded_port_id, slaves,
		RTE_DIM(slaves));

	TEST_ASSERT_EQUAL(retval, 0,
		"Expected bonded device %u have 0 slaves but returned %d.",
			test_params.bonded_port_id, retval);

	FOR_EACH_PORT(i, slave) {
		TEST_ASSERT_SUCCESS(rte_eth_dev_stop(slave->port_id),
				"Failed to stop bonded port %u",
				slave->port_id);

		TEST_ASSERT(slave->bonded == 0,
			"Port id=%u is still marked as enslaved.", slave->port_id);
	}

	return TEST_SUCCESS;
}

static int
test_setup(void)
{
	int retval, nb_mbuf_per_pool;
	char name[RTE_ETH_NAME_MAX_LEN];
	struct slave_conf *port;
	const uint8_t socket_id = rte_socket_id();
	uint16_t i;

	if (test_params.mbuf_pool == NULL) {
		nb_mbuf_per_pool = TEST_RX_DESC_MAX + DEF_PKT_BURST +
					TEST_TX_DESC_MAX + MAX_PKT_BURST;
		test_params.mbuf_pool = rte_pktmbuf_pool_create("TEST_MODE4",
			nb_mbuf_per_pool, MBUF_CACHE_SIZE, 0,
			RTE_MBUF_DEFAULT_BUF_SIZE, socket_id);

		TEST_ASSERT(test_params.mbuf_pool != NULL,
			"rte_mempool_create failed\n");
	}

	/* Create / initialize ring eth devs. */
	FOR_EACH_PORT(i, port) {
		port = &test_params.slave_ports[i];

		if (port->rx_queue == NULL) {
			retval = snprintf(name, RTE_DIM(name), SLAVE_RX_QUEUE_FMT, i);
			TEST_ASSERT(retval <= (int)RTE_DIM(name) - 1, "Name too long");
			port->rx_queue = rte_ring_create(name, RX_RING_SIZE, socket_id, 0);
			TEST_ASSERT(port->rx_queue != NULL,
				"Failed to allocate rx ring '%s': %s", name,
				rte_strerror(rte_errno));
		}

		if (port->tx_queue == NULL) {
			retval = snprintf(name, RTE_DIM(name), SLAVE_TX_QUEUE_FMT, i);
			TEST_ASSERT(retval <= (int)RTE_DIM(name) - 1, "Name too long");
			port->tx_queue = rte_ring_create(name, TX_RING_SIZE, socket_id, 0);
			TEST_ASSERT_NOT_NULL(port->tx_queue,
				"Failed to allocate tx ring '%s': %s", name,
				rte_strerror(rte_errno));
		}

		if (port->port_id == INVALID_PORT_ID) {
			retval = snprintf(name, RTE_DIM(name), SLAVE_DEV_NAME_FMT, i);
			TEST_ASSERT(retval < (int)RTE_DIM(name) - 1, "Name too long");
			retval = rte_eth_from_rings(name, &port->rx_queue, 1,
					&port->tx_queue, 1, socket_id);
			TEST_ASSERT(retval >= 0,
				"Failed to create ring ethdev '%s'\n", name);

			port->port_id = rte_eth_dev_count_avail() - 1;
		}

		retval = configure_ethdev(port->port_id, 1);
		TEST_ASSERT_SUCCESS(retval, "Failed to configure virtual ethdev %s\n",
			name);
	}

	if (test_params.bonded_port_id == INVALID_PORT_ID) {
		retval = rte_eth_bond_create(BONDED_DEV_NAME, BONDING_MODE_8023AD,
				socket_id);

		TEST_ASSERT(retval >= 0, "Failed to create bonded ethdev %s",
				BONDED_DEV_NAME);

		test_params.bonded_port_id = retval;
		TEST_ASSERT_SUCCESS(configure_ethdev(test_params.bonded_port_id, 0),
				"Failed to configure bonded ethdev %s", BONDED_DEV_NAME);
	} else if (rte_eth_bond_mode_get(test_params.bonded_port_id) !=
			BONDING_MODE_8023AD) {
		TEST_ASSERT(rte_eth_bond_mode_set(test_params.bonded_port_id,
			BONDING_MODE_8023AD) == 0,
			"Failed to set ethdev %d to mode %d",
			test_params.bonded_port_id, BONDING_MODE_8023AD);
	}

	return 0;
}

static void
testsuite_teardown(void)
{
	struct slave_conf *port;
	uint8_t i;

	/* Only stop ports.
	 * Any cleanup/reset state is done when particular test is
	 * started. */

	rte_eth_dev_stop(test_params.bonded_port_id);

	FOR_EACH_PORT(i, port)
		rte_eth_dev_stop(port->port_id);
}

/*
 * Check if given LACP packet. If it is, make make replay packet to force
 * COLLECTING state.
 * return 0 when pkt is LACP frame, 1 if it is not slow frame, 2 if it is slow
 * frame but not LACP
 */
static int
make_lacp_reply(struct slave_conf *slave, struct rte_mbuf *pkt)
{
	struct rte_ether_hdr *hdr;
	struct slow_protocol_frame *slow_hdr;
	struct lacpdu *lacp;

	/* look for LACP */
	hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	if (hdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_SLOW))
		return 1;

	slow_hdr = rte_pktmbuf_mtod(pkt, struct slow_protocol_frame *);
	/* ignore packets of other types */
	if (slow_hdr->slow_protocol.subtype != SLOW_SUBTYPE_LACP)
		return 2;

	slow_hdr = rte_pktmbuf_mtod(pkt, struct slow_protocol_frame *);

	/* Change source address to partner address */
	rte_ether_addr_copy(&parnter_mac_default, &slow_hdr->eth_hdr.s_addr);
	slow_hdr->eth_hdr.s_addr.addr_bytes[RTE_ETHER_ADDR_LEN - 1] =
		slave->port_id;

	lacp = (struct lacpdu *) &slow_hdr->slow_protocol;
	/* Save last received state */
	slave->lacp_parnter_state = lacp->actor.state;
	/* Change it into LACP replay by matching parameters. */
	memcpy(&lacp->partner.port_params, &lacp->actor.port_params,
		sizeof(struct port_params));

	lacp->partner.state = lacp->actor.state;

	rte_ether_addr_copy(&parnter_system, &lacp->actor.port_params.system);
	lacp->actor.state = STATE_LACP_ACTIVE |
						STATE_SYNCHRONIZATION |
						STATE_AGGREGATION |
						STATE_COLLECTING |
						STATE_DISTRIBUTING;

	return 0;
}

/*
 * Reads packets from given slave, search for LACP packet and reply them.
 *
 * Receives burst of packets from slave. Looks for LACP packet. Drops
 * all other packets. Prepares response LACP and sends it back.
 *
 * return number of LACP received and replied, -1 on error.
 */
static int
bond_handshake_reply(struct slave_conf *slave)
{
	int retval;
	struct rte_mbuf *rx_buf[MAX_PKT_BURST];
	struct rte_mbuf *lacp_tx_buf[MAX_PKT_BURST];
	uint16_t lacp_tx_buf_cnt = 0, i;

	retval = slave_get_pkts(slave, rx_buf, RTE_DIM(rx_buf));
	TEST_ASSERT(retval >= 0, "Getting slave %u packets failed.",
			slave->port_id);

	for (i = 0; i < (uint16_t)retval; i++) {
		if (make_lacp_reply(slave, rx_buf[i]) == 0) {
			/* reply with actor's LACP */
			lacp_tx_buf[lacp_tx_buf_cnt++] = rx_buf[i];
		} else
			rte_pktmbuf_free(rx_buf[i]);
	}

	if (lacp_tx_buf_cnt == 0)
		return 0;

	retval = slave_put_pkts(slave, lacp_tx_buf, lacp_tx_buf_cnt);
	if (retval <= lacp_tx_buf_cnt) {
		/* retval might be negative */
		for (i = RTE_MAX(0, retval); retval < lacp_tx_buf_cnt; retval++)
			rte_pktmbuf_free(lacp_tx_buf[i]);
	}

	TEST_ASSERT_EQUAL(retval, lacp_tx_buf_cnt,
		"Failed to equeue lacp packets into slave %u tx queue.",
		slave->port_id);

	return lacp_tx_buf_cnt;
}

/*
 * Function check if given slave tx queue contains packets that make mode 4
 * handshake complete. It will drain slave queue.
 * return 0 if handshake not completed, 1 if handshake was complete,
 */
static int
bond_handshake_done(struct slave_conf *slave)
{
	const uint8_t expected_state = STATE_LACP_ACTIVE | STATE_SYNCHRONIZATION |
			STATE_AGGREGATION | STATE_COLLECTING | STATE_DISTRIBUTING;

	return slave->lacp_parnter_state == expected_state;
}

static unsigned
bond_get_update_timeout_ms(void)
{
	struct rte_eth_bond_8023ad_conf conf;

	if (rte_eth_bond_8023ad_conf_get(test_params.bonded_port_id, &conf) < 0) {
		RTE_LOG(DEBUG, EAL, "Failed to get bonding configuration: "
				    "%s at %d\n", __func__, __LINE__);
		RTE_TEST_TRACE_FAILURE(__FILE__, __LINE__, __func__);
		return 0;
	}

	return conf.update_timeout_ms;
}

/*
 * Exchanges LACP packets with partner to achieve dynamic port configuration.
 * return TEST_SUCCESS if initial handshake succeed, TEST_FAILED otherwise.
 */
static int
bond_handshake(void)
{
	struct slave_conf *slave;
	struct rte_mbuf *buf[MAX_PKT_BURST];
	uint16_t nb_pkts;
	uint8_t all_slaves_done, i, j;
	uint8_t status[RTE_DIM(test_params.slave_ports)] = { 0 };
	const unsigned delay = bond_get_update_timeout_ms();

	/* Exchange LACP frames */
	all_slaves_done = 0;
	for (i = 0; i < 30 && all_slaves_done == 0; ++i) {
		rte_delay_ms(delay);

		all_slaves_done = 1;
		FOR_EACH_SLAVE(j, slave) {
			/* If response already send, skip slave */
			if (status[j] != 0)
				continue;

			if (bond_handshake_reply(slave) < 0) {
				all_slaves_done = 0;
				break;
			}

			status[j] = bond_handshake_done(slave);
			if (status[j] == 0)
				all_slaves_done = 0;
		}

		nb_pkts = bond_tx(NULL, 0);
		TEST_ASSERT_EQUAL(nb_pkts, 0, "Packets transmitted unexpectedly");

		nb_pkts = bond_rx(buf, RTE_DIM(buf));
		free_pkts(buf, nb_pkts);
		TEST_ASSERT_EQUAL(nb_pkts, 0, "Packets received unexpectedly");
	}
	/* If response didn't send - report failure */
	TEST_ASSERT_EQUAL(all_slaves_done, 1, "Bond handshake failed\n");

	/* If flags doesn't match - report failure */
	return all_slaves_done == 1 ? TEST_SUCCESS : TEST_FAILED;
}

#define TEST_LACP_SLAVE_COUT RTE_DIM(test_params.slave_ports)
static int
test_mode4_lacp(void)
{
	int retval;

	retval = initialize_bonded_device_with_slaves(TEST_LACP_SLAVE_COUT, 0);
	TEST_ASSERT_SUCCESS(retval, "Failed to initialize bonded device");

	/* Test LACP handshake function */
	retval = bond_handshake();
	TEST_ASSERT_SUCCESS(retval, "Initial handshake failed");

	retval = remove_slaves_and_stop_bonded_device();
	TEST_ASSERT_SUCCESS(retval, "Test cleanup failed.");

	return TEST_SUCCESS;
}
static int
test_mode4_agg_mode_selection(void)
{
	int retval;
	/* Test and verify for Stable mode */
	retval = initialize_bonded_device_with_slaves(TEST_LACP_SLAVE_COUT, 0);
	TEST_ASSERT_SUCCESS(retval, "Failed to initialize bonded device");


	retval = rte_eth_bond_8023ad_agg_selection_set(
			test_params.bonded_port_id, AGG_STABLE);
	TEST_ASSERT_SUCCESS(retval, "Failed to initialize bond aggregation mode");
	retval = bond_handshake();
	TEST_ASSERT_SUCCESS(retval, "Initial handshake failed");


	retval = rte_eth_bond_8023ad_agg_selection_get(
			test_params.bonded_port_id);
	TEST_ASSERT_EQUAL(retval, AGG_STABLE,
			"Wrong agg mode received from bonding device");

	retval = remove_slaves_and_stop_bonded_device();
	TEST_ASSERT_SUCCESS(retval, "Test cleanup failed.");


	/* test and verify for Bandwidth mode */
	retval = initialize_bonded_device_with_slaves(TEST_LACP_SLAVE_COUT, 0);
	TEST_ASSERT_SUCCESS(retval, "Failed to initialize bonded device");


	retval = rte_eth_bond_8023ad_agg_selection_set(
			test_params.bonded_port_id,
			AGG_BANDWIDTH);
	TEST_ASSERT_SUCCESS(retval,
			"Failed to initialize bond aggregation mode");
	retval = bond_handshake();
	TEST_ASSERT_SUCCESS(retval, "Initial handshake failed");

	retval = rte_eth_bond_8023ad_agg_selection_get(
			test_params.bonded_port_id);
	TEST_ASSERT_EQUAL(retval, AGG_BANDWIDTH,
			"Wrong agg mode received from bonding device");

	retval = remove_slaves_and_stop_bonded_device();
	TEST_ASSERT_SUCCESS(retval, "Test cleanup failed.");

	/* test and verify selection for count mode */
	retval = initialize_bonded_device_with_slaves(TEST_LACP_SLAVE_COUT, 0);
	TEST_ASSERT_SUCCESS(retval, "Failed to initialize bonded device");


	retval = rte_eth_bond_8023ad_agg_selection_set(
			test_params.bonded_port_id, AGG_COUNT);
	TEST_ASSERT_SUCCESS(retval,
			"Failed to initialize bond aggregation mode");
	retval = bond_handshake();
	TEST_ASSERT_SUCCESS(retval, "Initial handshake failed");

	retval = rte_eth_bond_8023ad_agg_selection_get(
			test_params.bonded_port_id);
	TEST_ASSERT_EQUAL(retval, AGG_COUNT,
			"Wrong agg mode received from bonding device");

	retval = remove_slaves_and_stop_bonded_device();
	TEST_ASSERT_SUCCESS(retval, "Test cleanup failed.");

	return TEST_SUCCESS;
}

static int
generate_packets(struct rte_ether_addr *src_mac,
	struct rte_ether_addr *dst_mac, uint16_t count, struct rte_mbuf **buf)
{
	uint16_t pktlen = PACKET_BURST_GEN_PKT_LEN;
	uint8_t vlan_enable = 0;
	uint16_t vlan_id = 0;
	uint8_t ip4_type = 1; /* 0 - ipv6 */

	uint16_t src_port = 10, dst_port = 20;

	uint32_t ip_src[4] = { [0 ... 2] = 0xDEADBEEF, [3] = RTE_IPV4(192, 168, 0, 1) };
	uint32_t ip_dst[4] = { [0 ... 2] = 0xFEEDFACE, [3] = RTE_IPV4(192, 168, 0, 2) };

	struct rte_ether_hdr pkt_eth_hdr;
	struct rte_udp_hdr pkt_udp_hdr;
	union {
		struct rte_ipv4_hdr v4;
		struct rte_ipv6_hdr v6;
	} pkt_ip_hdr;

	int retval;

	initialize_eth_header(&pkt_eth_hdr, src_mac, dst_mac, ip4_type,
			vlan_enable, vlan_id);

	if (ip4_type)
		initialize_ipv4_header(&pkt_ip_hdr.v4, ip_src[3], ip_dst[3], pktlen);
	else
		initialize_ipv6_header(&pkt_ip_hdr.v6, (uint8_t *)ip_src,
			(uint8_t *)&ip_dst, pktlen);

	initialize_udp_header(&pkt_udp_hdr, src_port, dst_port, 16);

	retval = generate_packet_burst(test_params.mbuf_pool, buf,
			&pkt_eth_hdr, vlan_enable, &pkt_ip_hdr, 1, &pkt_udp_hdr,
			count, pktlen, 1);

	if (retval > 0 && retval != count)
		free_pkts(&buf[count - retval], retval);

	TEST_ASSERT_EQUAL(retval, count, "Failed to generate %u packets",
		count);

	return count;
}

static int
generate_and_put_packets(struct slave_conf *slave,
			struct rte_ether_addr *src_mac,
			struct rte_ether_addr *dst_mac, uint16_t count)
{
	struct rte_mbuf *pkts[MAX_PKT_BURST];
	int retval;

	retval = generate_packets(src_mac, dst_mac, count, pkts);
	if (retval != (int)count)
		return retval;

	retval = slave_put_pkts(slave, pkts, count);
	if (retval > 0 && retval != count)
		free_pkts(&pkts[retval], count - retval);

	TEST_ASSERT_EQUAL(retval, count,
		"Failed to enqueue packets into slave %u RX queue", slave->port_id);

	return TEST_SUCCESS;
}

static int
test_mode4_rx(void)
{
	struct slave_conf *slave;
	uint16_t i, j;

	uint16_t expected_pkts_cnt;
	struct rte_mbuf *pkts[MAX_PKT_BURST];
	int retval;
	unsigned delay;

	struct rte_ether_hdr *hdr;

	struct rte_ether_addr src_mac = {
		{ 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00 } };
	struct rte_ether_addr dst_mac;
	struct rte_ether_addr bonded_mac;

	retval = initialize_bonded_device_with_slaves(TEST_PROMISC_SLAVE_COUNT,
						      0);
	TEST_ASSERT_SUCCESS(retval, "Failed to initialize bonded device");

	retval = bond_handshake();
	TEST_ASSERT_SUCCESS(retval, "Initial handshake failed");

	retval = rte_eth_macaddr_get(test_params.bonded_port_id, &bonded_mac);
	TEST_ASSERT_SUCCESS(retval, "Failed to get mac address: %s",
			    strerror(-retval));
	rte_ether_addr_copy(&bonded_mac, &dst_mac);

	/* Assert that dst address is not bonding address.  Do not set the
	 * least significant bit of the zero byte as this would create a
	 * multicast address.
	 */
	dst_mac.addr_bytes[0] += 2;

	/* First try with promiscuous mode enabled.
	 * Add 2 packets to each slave. First with bonding MAC address, second with
	 * different. Check if we received all of them. */
	retval = rte_eth_promiscuous_enable(test_params.bonded_port_id);
	TEST_ASSERT_SUCCESS(retval,
			"Failed to enable promiscuous mode for port %d: %s",
			test_params.bonded_port_id, rte_strerror(-retval));

	expected_pkts_cnt = 0;
	FOR_EACH_SLAVE(i, slave) {
		retval = generate_and_put_packets(slave, &src_mac, &bonded_mac, 1);
		TEST_ASSERT_SUCCESS(retval, "Failed to enqueue packets to slave %u",
			slave->port_id);

		retval = generate_and_put_packets(slave, &src_mac, &dst_mac, 1);
		TEST_ASSERT_SUCCESS(retval, "Failed to enqueue packets to slave %u",
			slave->port_id);

		/* Expect 2 packets per slave */
		expected_pkts_cnt += 2;
	}

	retval = rte_eth_rx_burst(test_params.bonded_port_id, 0, pkts,
		RTE_DIM(pkts));

	if (retval == expected_pkts_cnt) {
		int cnt[2] = { 0, 0 };

		for (i = 0; i < expected_pkts_cnt; i++) {
			hdr = rte_pktmbuf_mtod(pkts[i], struct rte_ether_hdr *);
			cnt[rte_is_same_ether_addr(&hdr->d_addr,
							&bonded_mac)]++;
		}

		free_pkts(pkts, expected_pkts_cnt);

		/* For division by 2 expected_pkts_cnt must be even */
		RTE_VERIFY((expected_pkts_cnt & 1) == 0);
		TEST_ASSERT(cnt[0] == expected_pkts_cnt / 2 &&
			cnt[1] == expected_pkts_cnt / 2,
			"Expected %u packets with the same MAC and %u with different but "
			"got %u with the same and %u with different MAC",
			expected_pkts_cnt / 2, expected_pkts_cnt / 2, cnt[1], cnt[0]);
	} else if (retval > 0)
		free_pkts(pkts, retval);

	TEST_ASSERT_EQUAL(retval, expected_pkts_cnt,
		"Expected %u packets but received only %d", expected_pkts_cnt, retval);

	/* Now, disable promiscuous mode. When promiscuous mode is disabled we
	 * expect to receive only packets that are directed to bonding port. */
	retval = rte_eth_promiscuous_disable(test_params.bonded_port_id);
	TEST_ASSERT_SUCCESS(retval,
		"Failed to disable promiscuous mode for port %d: %s",
		test_params.bonded_port_id, rte_strerror(-retval));

	expected_pkts_cnt = 0;
	FOR_EACH_SLAVE(i, slave) {
		retval = generate_and_put_packets(slave, &src_mac, &bonded_mac, 1);
		TEST_ASSERT_SUCCESS(retval, "Failed to enqueue packets to slave %u",
			slave->port_id);

		retval = generate_and_put_packets(slave, &src_mac, &dst_mac, 1);
		TEST_ASSERT_SUCCESS(retval, "Failed to enqueue packets to slave %u",
			slave->port_id);

		/* Expect only one packet per slave */
		expected_pkts_cnt += 1;
	}

	retval = rte_eth_rx_burst(test_params.bonded_port_id, 0, pkts,
		RTE_DIM(pkts));

	if (retval == expected_pkts_cnt) {
		int eq_cnt = 0;

		for (i = 0; i < expected_pkts_cnt; i++) {
			hdr = rte_pktmbuf_mtod(pkts[i], struct rte_ether_hdr *);
			eq_cnt += rte_is_same_ether_addr(&hdr->d_addr,
							&bonded_mac);
		}

		free_pkts(pkts, expected_pkts_cnt);
		TEST_ASSERT_EQUAL(eq_cnt, expected_pkts_cnt, "Packet address mismatch");
	} else if (retval > 0)
		free_pkts(pkts, retval);

	TEST_ASSERT_EQUAL(retval, expected_pkts_cnt,
		"Expected %u packets but received only %d", expected_pkts_cnt, retval);

	/* Link down test: simulate link down for first slave. */
	delay = bond_get_update_timeout_ms();

	uint8_t slave_down_id = INVALID_PORT_ID;

	/* Find first slave and make link down on it*/
	FOR_EACH_SLAVE(i, slave) {
		rte_eth_dev_set_link_down(slave->port_id);
		slave_down_id = slave->port_id;
		break;
	}

	RTE_VERIFY(slave_down_id != INVALID_PORT_ID);

	/* Give some time to rearrange bonding */
	for (i = 0; i < 3; i++) {
		rte_delay_ms(delay);
		bond_handshake();
	}

	TEST_ASSERT_SUCCESS(bond_handshake(), "Handshake after link down failed");

	/* Put packet to each slave */
	FOR_EACH_SLAVE(i, slave) {
		void *pkt = NULL;

		dst_mac.addr_bytes[RTE_ETHER_ADDR_LEN - 1] = slave->port_id;
		retval = generate_and_put_packets(slave, &src_mac, &dst_mac, 1);
		TEST_ASSERT_SUCCESS(retval, "Failed to generate test packet burst.");

		src_mac.addr_bytes[RTE_ETHER_ADDR_LEN - 1] = slave->port_id;
		retval = generate_and_put_packets(slave, &src_mac, &bonded_mac, 1);
		TEST_ASSERT_SUCCESS(retval, "Failed to generate test packet burst.");

		retval = bond_rx(pkts, RTE_DIM(pkts));

		/* Clean anything */
		if (retval > 0)
			free_pkts(pkts, retval);

		while (rte_ring_dequeue(slave->rx_queue, (void **)&pkt) == 0)
			rte_pktmbuf_free(pkt);

		if (slave_down_id == slave->port_id)
			TEST_ASSERT_EQUAL(retval, 0, "Packets received unexpectedly.");
		else
			TEST_ASSERT_NOT_EQUAL(retval, 0,
				"Expected to receive some packets on slave %u.",
				slave->port_id);
		rte_eth_dev_start(slave->port_id);

		for (j = 0; j < 5; j++) {
			TEST_ASSERT(bond_handshake_reply(slave) >= 0,
				"Handshake after link up");

			if (bond_handshake_done(slave) == 1)
				break;
		}

		TEST_ASSERT(j < 5, "Failed to aggregate slave after link up");
	}

	remove_slaves_and_stop_bonded_device();
	return TEST_SUCCESS;
}

static int
test_mode4_tx_burst(void)
{
	struct slave_conf *slave;
	uint16_t i, j;

	uint16_t exp_pkts_cnt, pkts_cnt = 0;
	struct rte_mbuf *pkts[MAX_PKT_BURST];
	int retval;
	unsigned delay;

	struct rte_ether_addr dst_mac = {
		{ 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00 } };
	struct rte_ether_addr bonded_mac;

	retval = initialize_bonded_device_with_slaves(TEST_TX_SLAVE_COUNT, 0);
	TEST_ASSERT_SUCCESS(retval, "Failed to initialize bonded device");

	retval = bond_handshake();
	TEST_ASSERT_SUCCESS(retval, "Initial handshake failed");

	retval = rte_eth_macaddr_get(test_params.bonded_port_id, &bonded_mac);
	TEST_ASSERT_SUCCESS(retval, "Failed to get mac address: %s",
			    strerror(-retval));
	/* Prepare burst */
	for (pkts_cnt = 0; pkts_cnt < RTE_DIM(pkts); pkts_cnt++) {
		dst_mac.addr_bytes[RTE_ETHER_ADDR_LEN - 1] = pkts_cnt;
		retval = generate_packets(&bonded_mac, &dst_mac, 1, &pkts[pkts_cnt]);

		if (retval != 1)
			free_pkts(pkts, pkts_cnt);

		TEST_ASSERT_EQUAL(retval, 1, "Failed to generate packet %u", pkts_cnt);
	}
	exp_pkts_cnt = pkts_cnt;

	/* Transmit packets on bonded device */
	retval = bond_tx(pkts, pkts_cnt);
	if (retval > 0 && retval < pkts_cnt)
		free_pkts(&pkts[retval], pkts_cnt - retval);

	TEST_ASSERT_EQUAL(retval, pkts_cnt, "TX on bonded device failed");

	/* Check if packets were transmitted properly. Every slave should have
	 * at least one packet, and sum must match. Under normal operation
	 * there should be no LACP nor MARKER frames. */
	pkts_cnt = 0;
	FOR_EACH_SLAVE(i, slave) {
		uint16_t normal_cnt, slow_cnt;

		retval = slave_get_pkts(slave, pkts, RTE_DIM(pkts));
		normal_cnt = 0;
		slow_cnt = 0;

		for (j = 0; j < retval; j++) {
			if (make_lacp_reply(slave, pkts[j]) == 1)
				normal_cnt++;
			else
				slow_cnt++;
		}

		free_pkts(pkts, normal_cnt + slow_cnt);
		TEST_ASSERT_EQUAL(slow_cnt, 0,
			"slave %u unexpectedly transmitted %d SLOW packets", slave->port_id,
			slow_cnt);

		TEST_ASSERT_NOT_EQUAL(normal_cnt, 0,
			"slave %u did not transmitted any packets", slave->port_id);

		pkts_cnt += normal_cnt;
	}

	TEST_ASSERT_EQUAL(exp_pkts_cnt, pkts_cnt,
		"Expected %u packets but transmitted only %d", exp_pkts_cnt, pkts_cnt);

	/* Link down test:
	 * simulate link down for first slave. */
	delay = bond_get_update_timeout_ms();

	uint8_t slave_down_id = INVALID_PORT_ID;

	FOR_EACH_SLAVE(i, slave) {
		rte_eth_dev_set_link_down(slave->port_id);
		slave_down_id = slave->port_id;
		break;
	}

	RTE_VERIFY(slave_down_id != INVALID_PORT_ID);

	/* Give some time to rearrange bonding. */
	for (i = 0; i < 3; i++) {
		bond_handshake();
		rte_delay_ms(delay);
	}

	TEST_ASSERT_SUCCESS(bond_handshake(), "Handshake after link down failed");

	/* Prepare burst. */
	for (pkts_cnt = 0; pkts_cnt < RTE_DIM(pkts); pkts_cnt++) {
		dst_mac.addr_bytes[RTE_ETHER_ADDR_LEN - 1] = pkts_cnt;
		retval = generate_packets(&bonded_mac, &dst_mac, 1, &pkts[pkts_cnt]);

		if (retval != 1)
			free_pkts(pkts, pkts_cnt);

		TEST_ASSERT_EQUAL(retval, 1, "Failed to generate test packet %u",
			pkts_cnt);
	}
	exp_pkts_cnt = pkts_cnt;

	/* Transmit packets on bonded device. */
	retval = bond_tx(pkts, pkts_cnt);
	if (retval > 0 && retval < pkts_cnt)
		free_pkts(&pkts[retval], pkts_cnt - retval);

	TEST_ASSERT_EQUAL(retval, pkts_cnt, "TX on bonded device failed");

	/* Check if packets was transmitted properly. Every slave should have
	 * at least one packet, and sum must match. Under normal operation
	 * there should be no LACP nor MARKER frames. */
	pkts_cnt = 0;
	FOR_EACH_SLAVE(i, slave) {
		uint16_t normal_cnt, slow_cnt;

		retval = slave_get_pkts(slave, pkts, RTE_DIM(pkts));
		normal_cnt = 0;
		slow_cnt = 0;

		for (j = 0; j < retval; j++) {
			if (make_lacp_reply(slave, pkts[j]) == 1)
				normal_cnt++;
			else
				slow_cnt++;
		}

		free_pkts(pkts, normal_cnt + slow_cnt);

		if (slave_down_id == slave->port_id) {
			TEST_ASSERT_EQUAL(normal_cnt + slow_cnt, 0,
				"slave %u enexpectedly transmitted %u packets",
				normal_cnt + slow_cnt, slave->port_id);
		} else {
			TEST_ASSERT_EQUAL(slow_cnt, 0,
				"slave %u unexpectedly transmitted %d SLOW packets",
				slave->port_id, slow_cnt);

			TEST_ASSERT_NOT_EQUAL(normal_cnt, 0,
				"slave %u did not transmitted any packets", slave->port_id);
		}

		pkts_cnt += normal_cnt;
	}

	TEST_ASSERT_EQUAL(exp_pkts_cnt, pkts_cnt,
		"Expected %u packets but transmitted only %d", exp_pkts_cnt, pkts_cnt);

	return remove_slaves_and_stop_bonded_device();
}

static void
init_marker(struct rte_mbuf *pkt, struct slave_conf *slave)
{
	struct marker_header *marker_hdr = rte_pktmbuf_mtod(pkt,
			struct marker_header *);

	/* Copy multicast destination address */
	rte_ether_addr_copy(&slow_protocol_mac_addr,
			&marker_hdr->eth_hdr.d_addr);

	/* Init source address */
	rte_ether_addr_copy(&parnter_mac_default, &marker_hdr->eth_hdr.s_addr);
	marker_hdr->eth_hdr.s_addr.addr_bytes[RTE_ETHER_ADDR_LEN - 1] =
		slave->port_id;

	marker_hdr->eth_hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_SLOW);

	marker_hdr->marker.subtype = SLOW_SUBTYPE_MARKER;
	marker_hdr->marker.version_number = 1;
	marker_hdr->marker.tlv_type_marker = MARKER_TLV_TYPE_INFO;
	marker_hdr->marker.info_length =
			offsetof(struct marker, reserved_90) -
			offsetof(struct marker, requester_port);
	RTE_VERIFY(marker_hdr->marker.info_length == 16);
	marker_hdr->marker.requester_port = slave->port_id + 1;
	marker_hdr->marker.tlv_type_terminator = TLV_TYPE_TERMINATOR_INFORMATION;
	marker_hdr->marker.terminator_length = 0;
}

static int
test_mode4_marker(void)
{
	struct slave_conf *slave;
	struct rte_mbuf *pkts[MAX_PKT_BURST];
	struct rte_mbuf *marker_pkt;
	struct marker_header *marker_hdr;

	unsigned delay;
	int retval;
	uint16_t nb_pkts;
	uint8_t i, j;
	const uint16_t ethtype_slow_be = rte_be_to_cpu_16(RTE_ETHER_TYPE_SLOW);

	retval = initialize_bonded_device_with_slaves(TEST_MARKER_SLAVE_COUT,
						      0);
	TEST_ASSERT_SUCCESS(retval, "Failed to initialize bonded device");

	/* Test LACP handshake function */
	retval = bond_handshake();
	TEST_ASSERT_SUCCESS(retval, "Initial handshake failed");

	delay = bond_get_update_timeout_ms();
	FOR_EACH_SLAVE(i, slave) {
		marker_pkt = rte_pktmbuf_alloc(test_params.mbuf_pool);
		TEST_ASSERT_NOT_NULL(marker_pkt, "Failed to allocate marker packet");
		init_marker(marker_pkt, slave);

		retval = slave_put_pkts(slave, &marker_pkt, 1);
		if (retval != 1)
			rte_pktmbuf_free(marker_pkt);

		TEST_ASSERT_EQUAL(retval, 1,
			"Failed to send marker packet to slave %u", slave->port_id);

		for (j = 0; j < 20; ++j) {
			rte_delay_ms(delay);
			retval = rte_eth_rx_burst(test_params.bonded_port_id, 0, pkts,
				RTE_DIM(pkts));

			if (retval > 0)
				free_pkts(pkts, retval);

			TEST_ASSERT_EQUAL(retval, 0, "Received packets unexpectedly");

			retval = rte_eth_tx_burst(test_params.bonded_port_id, 0, NULL, 0);
			TEST_ASSERT_EQUAL(retval, 0,
				"Requested TX of 0 packets but %d transmitted", retval);

			/* Check if LACP packet was send by state machines
			   First and only packet must be a maker response */
			retval = slave_get_pkts(slave, pkts, MAX_PKT_BURST);
			if (retval == 0)
				continue;
			if (retval > 1)
				free_pkts(pkts, retval);

			TEST_ASSERT_EQUAL(retval, 1, "failed to get slave packets");
			nb_pkts = retval;

			marker_hdr = rte_pktmbuf_mtod(pkts[0], struct marker_header *);
			/* Check if it's slow packet*/
			if (marker_hdr->eth_hdr.ether_type != ethtype_slow_be)
				retval = -1;
			/* Check if it's marker packet */
			else if (marker_hdr->marker.subtype != SLOW_SUBTYPE_MARKER)
				retval = -2;
			else if (marker_hdr->marker.tlv_type_marker != MARKER_TLV_TYPE_RESP)
				retval = -3;

			free_pkts(pkts, nb_pkts);

			TEST_ASSERT_NOT_EQUAL(retval, -1, "Unexpected protocol type");
			TEST_ASSERT_NOT_EQUAL(retval, -2, "Unexpected sub protocol type");
			TEST_ASSERT_NOT_EQUAL(retval, -3, "Unexpected marker type");
			break;
		}

		TEST_ASSERT(j < 20, "Marker response not found");
	}

	retval = remove_slaves_and_stop_bonded_device();
	TEST_ASSERT_SUCCESS(retval,	"Test cleanup failed.");

	return TEST_SUCCESS;
}

static int
test_mode4_expired(void)
{
	struct slave_conf *slave, *exp_slave = NULL;
	struct rte_mbuf *pkts[MAX_PKT_BURST];
	int retval;
	uint32_t old_delay;

	uint8_t i;
	uint16_t j;

	struct rte_eth_bond_8023ad_conf conf;

	retval = initialize_bonded_device_with_slaves(TEST_EXPIRED_SLAVE_COUNT,
						      0);
	/* Set custom timeouts to make test last shorter. */
	rte_eth_bond_8023ad_conf_get(test_params.bonded_port_id, &conf);
	conf.fast_periodic_ms = 100;
	conf.slow_periodic_ms = 600;
	conf.short_timeout_ms = 300;
	conf.long_timeout_ms = 900;
	conf.aggregate_wait_timeout_ms = 200;
	conf.tx_period_ms = 100;
	old_delay = conf.update_timeout_ms;
	conf.update_timeout_ms = 10;
	rte_eth_bond_8023ad_setup(test_params.bonded_port_id, &conf);

	/* Wait for new settings to be applied. */
	for (i = 0; i < old_delay/conf.update_timeout_ms * 2; i++) {
		FOR_EACH_SLAVE(j, slave)
			bond_handshake_reply(slave);

		rte_delay_ms(conf.update_timeout_ms);
	}

	retval = bond_handshake();
	TEST_ASSERT_SUCCESS(retval, "Initial handshake failed");

	/* Find first slave */
	FOR_EACH_SLAVE(i, slave) {
		exp_slave = slave;
		break;
	}

	RTE_VERIFY(exp_slave != NULL);

	/* When one of partners do not send or respond to LACP frame in
	 * conf.long_timeout_ms time, internal state machines should detect this
	 * and transit to expired state. */
	for (j = 0; j < conf.long_timeout_ms/conf.update_timeout_ms + 2; j++) {
		rte_delay_ms(conf.update_timeout_ms);

		retval = bond_tx(NULL, 0);
		TEST_ASSERT_EQUAL(retval, 0, "Unexpectedly received %d packets",
			retval);

		FOR_EACH_SLAVE(i, slave) {
			retval = bond_handshake_reply(slave);
			TEST_ASSERT(retval >= 0, "Handshake failed");

			/* Remove replay for slave that suppose to be expired. */
			if (slave == exp_slave) {
				while (rte_ring_count(slave->rx_queue) > 0) {
					void *pkt = NULL;

					rte_ring_dequeue(slave->rx_queue, &pkt);
					rte_pktmbuf_free(pkt);
				}
			}
		}

		retval = bond_rx(pkts, RTE_DIM(pkts));
		if (retval > 0)
			free_pkts(pkts, retval);

		TEST_ASSERT_EQUAL(retval, 0, "Unexpectedly received %d packets",
			retval);
	}

	/* After test only expected slave should be in EXPIRED state */
	FOR_EACH_SLAVE(i, slave) {
		if (slave == exp_slave)
			TEST_ASSERT(slave->lacp_parnter_state & STATE_EXPIRED,
				"Slave %u should be in expired.", slave->port_id);
		else
			TEST_ASSERT_EQUAL(bond_handshake_done(slave), 1,
				"Slave %u should be operational.", slave->port_id);
	}

	retval = remove_slaves_and_stop_bonded_device();
	TEST_ASSERT_SUCCESS(retval, "Test cleanup failed.");

	return TEST_SUCCESS;
}

static int
test_mode4_ext_ctrl(void)
{
	/*
	 * configure bonded interface without the external sm enabled
	 *   . try to transmit lacpdu (should fail)
	 *   . try to set collecting and distributing flags (should fail)
	 * reconfigure w/external sm
	 *   . transmit one lacpdu on each slave using new api
	 *   . make sure each slave receives one lacpdu using the callback api
	 *   . transmit one data pdu on each slave (should fail)
	 *   . enable distribution and collection, send one data pdu each again
	 */

	int retval;
	struct slave_conf *slave = NULL;
	uint8_t i;

	struct rte_mbuf *lacp_tx_buf[SLAVE_COUNT];
	struct rte_ether_addr src_mac, dst_mac;
	struct lacpdu_header lacpdu = {
		.lacpdu = {
			.subtype = SLOW_SUBTYPE_LACP,
		},
	};

	rte_ether_addr_copy(&parnter_system, &src_mac);
	rte_ether_addr_copy(&slow_protocol_mac_addr, &dst_mac);

	initialize_eth_header(&lacpdu.eth_hdr, &src_mac, &dst_mac,
			      RTE_ETHER_TYPE_SLOW, 0, 0);

	for (i = 0; i < SLAVE_COUNT; i++) {
		lacp_tx_buf[i] = rte_pktmbuf_alloc(test_params.mbuf_pool);
		rte_memcpy(rte_pktmbuf_mtod(lacp_tx_buf[i], char *),
			   &lacpdu, sizeof(lacpdu));
		rte_pktmbuf_pkt_len(lacp_tx_buf[i]) = sizeof(lacpdu);
	}

	retval = initialize_bonded_device_with_slaves(TEST_TX_SLAVE_COUNT, 0);
	TEST_ASSERT_SUCCESS(retval, "Failed to initialize bonded device");

	FOR_EACH_SLAVE(i, slave) {
		TEST_ASSERT_FAIL(rte_eth_bond_8023ad_ext_slowtx(
						test_params.bonded_port_id,
						slave->port_id, lacp_tx_buf[i]),
				 "Slave should not allow manual LACP xmit");
		TEST_ASSERT_FAIL(rte_eth_bond_8023ad_ext_collect(
						test_params.bonded_port_id,
						slave->port_id, 1),
				 "Slave should not allow external state controls");
	}

	free_pkts(lacp_tx_buf, RTE_DIM(lacp_tx_buf));

	retval = remove_slaves_and_stop_bonded_device();
	TEST_ASSERT_SUCCESS(retval, "Bonded device cleanup failed.");

	return TEST_SUCCESS;
}


static int
test_mode4_ext_lacp(void)
{
	int retval;
	struct slave_conf *slave = NULL;
	uint8_t all_slaves_done = 0, i;
	uint16_t nb_pkts;
	const unsigned int delay = bond_get_update_timeout_ms();

	struct rte_mbuf *lacp_tx_buf[SLAVE_COUNT];
	struct rte_mbuf *buf[SLAVE_COUNT];
	struct rte_ether_addr src_mac, dst_mac;
	struct lacpdu_header lacpdu = {
		.lacpdu = {
			.subtype = SLOW_SUBTYPE_LACP,
		},
	};

	rte_ether_addr_copy(&parnter_system, &src_mac);
	rte_ether_addr_copy(&slow_protocol_mac_addr, &dst_mac);

	initialize_eth_header(&lacpdu.eth_hdr, &src_mac, &dst_mac,
			      RTE_ETHER_TYPE_SLOW, 0, 0);

	for (i = 0; i < SLAVE_COUNT; i++) {
		lacp_tx_buf[i] = rte_pktmbuf_alloc(test_params.mbuf_pool);
		rte_memcpy(rte_pktmbuf_mtod(lacp_tx_buf[i], char *),
			   &lacpdu, sizeof(lacpdu));
		rte_pktmbuf_pkt_len(lacp_tx_buf[i]) = sizeof(lacpdu);
	}

	retval = initialize_bonded_device_with_slaves(TEST_TX_SLAVE_COUNT, 1);
	TEST_ASSERT_SUCCESS(retval, "Failed to initialize bonded device");

	memset(lacpdu_rx_count, 0, sizeof(lacpdu_rx_count));

	/* Wait for new settings to be applied. */
	for (i = 0; i < 30; ++i)
		rte_delay_ms(delay);

	FOR_EACH_SLAVE(i, slave) {
		retval = rte_eth_bond_8023ad_ext_slowtx(
						test_params.bonded_port_id,
						slave->port_id, lacp_tx_buf[i]);
		TEST_ASSERT_SUCCESS(retval,
				    "Slave should allow manual LACP xmit");
	}

	nb_pkts = bond_tx(NULL, 0);
	TEST_ASSERT_EQUAL(nb_pkts, 0, "Packets transmitted unexpectedly");

	FOR_EACH_SLAVE(i, slave) {
		nb_pkts = slave_get_pkts(slave, buf, RTE_DIM(buf));
		TEST_ASSERT_EQUAL(nb_pkts, 1, "found %u packets on slave %d\n",
				  nb_pkts, i);
		slave_put_pkts(slave, buf, nb_pkts);
	}

	nb_pkts = bond_rx(buf, RTE_DIM(buf));
	free_pkts(buf, nb_pkts);
	TEST_ASSERT_EQUAL(nb_pkts, 0, "Packets received unexpectedly");

	/* wait for the periodic callback to run */
	for (i = 0; i < 30 && all_slaves_done == 0; ++i) {
		uint8_t s, total = 0;

		rte_delay_ms(delay);
		FOR_EACH_SLAVE(s, slave) {
			total += lacpdu_rx_count[slave->port_id];
		}

		if (total >= SLAVE_COUNT)
			all_slaves_done = 1;
	}

	FOR_EACH_SLAVE(i, slave) {
		TEST_ASSERT_EQUAL(lacpdu_rx_count[slave->port_id], 1,
				  "Slave port %u should have received 1 lacpdu (count=%u)",
				  slave->port_id,
				  lacpdu_rx_count[slave->port_id]);
	}

	retval = remove_slaves_and_stop_bonded_device();
	TEST_ASSERT_SUCCESS(retval, "Test cleanup failed.");

	return TEST_SUCCESS;
}

static int
check_environment(void)
{
	struct slave_conf *port;
	uint8_t i, env_state;
	uint16_t slaves[RTE_DIM(test_params.slave_ports)];
	int slaves_count;

	env_state = 0;
	FOR_EACH_PORT(i, port) {
		if (rte_ring_count(port->rx_queue) != 0)
			env_state |= 0x01;

		if (rte_ring_count(port->tx_queue) != 0)
			env_state |= 0x02;

		if (port->bonded != 0)
			env_state |= 0x04;

		if (port->lacp_parnter_state != 0)
			env_state |= 0x08;

		if (env_state != 0)
			break;
	}

	slaves_count = rte_eth_bond_slaves_get(test_params.bonded_port_id,
			slaves, RTE_DIM(slaves));

	if (slaves_count != 0)
		env_state |= 0x10;

	TEST_ASSERT_EQUAL(env_state, 0,
		"Environment not clean (port %u):%s%s%s%s%s",
		port->port_id,
		env_state & 0x01 ? " slave rx queue not clean" : "",
		env_state & 0x02 ? " slave tx queue not clean" : "",
		env_state & 0x04 ? " port marked as enslaved" : "",
		env_state & 0x80 ? " slave state is not reset" : "",
		env_state & 0x10 ? " slave count not equal 0" : ".");


	return TEST_SUCCESS;
}

static int
test_mode4_executor(int (*test_func)(void))
{
	struct slave_conf *port;
	int test_result;
	uint8_t i;
	void *pkt;

	/* Check if environment is clean. Fail to launch a test if there was
	 * a critical error before that prevented to reset environment. */
	TEST_ASSERT_SUCCESS(check_environment(),
		"Refusing to launch test in dirty environment.");

	RTE_VERIFY(test_func != NULL);
	test_result = (*test_func)();

	/* If test succeed check if environment wast left in good condition. */
	if (test_result == TEST_SUCCESS)
		test_result = check_environment();

	/* Reset environment in case test failed to do that. */
	if (test_result != TEST_SUCCESS) {
		TEST_ASSERT_SUCCESS(remove_slaves_and_stop_bonded_device(),
			"Failed to stop bonded device");

		FOR_EACH_PORT(i, port) {
			while (rte_ring_count(port->rx_queue) != 0) {
				if (rte_ring_dequeue(port->rx_queue, &pkt) == 0)
					rte_pktmbuf_free(pkt);
			}

			while (rte_ring_count(port->tx_queue) != 0) {
				if (rte_ring_dequeue(port->tx_queue, &pkt) == 0)
					rte_pktmbuf_free(pkt);
			}
		}
	}

	return test_result;
}

static int
test_mode4_agg_mode_selection_wrapper(void){
	return test_mode4_executor(&test_mode4_agg_mode_selection);
}

static int
test_mode4_lacp_wrapper(void)
{
	return test_mode4_executor(&test_mode4_lacp);
}

static int
test_mode4_marker_wrapper(void)
{
	return test_mode4_executor(&test_mode4_marker);
}

static int
test_mode4_rx_wrapper(void)
{
	return test_mode4_executor(&test_mode4_rx);
}

static int
test_mode4_tx_burst_wrapper(void)
{
	return test_mode4_executor(&test_mode4_tx_burst);
}

static int
test_mode4_expired_wrapper(void)
{
	return test_mode4_executor(&test_mode4_expired);
}

static int
test_mode4_ext_ctrl_wrapper(void)
{
	return test_mode4_executor(&test_mode4_ext_ctrl);
}

static int
test_mode4_ext_lacp_wrapper(void)
{
	return test_mode4_executor(&test_mode4_ext_lacp);
}

static struct unit_test_suite link_bonding_mode4_test_suite  = {
	.suite_name = "Link Bonding mode 4 Unit Test Suite",
	.setup = test_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_NAMED("test_mode4_agg_mode_selection",
				test_mode4_agg_mode_selection_wrapper),
		TEST_CASE_NAMED("test_mode4_lacp", test_mode4_lacp_wrapper),
		TEST_CASE_NAMED("test_mode4_rx", test_mode4_rx_wrapper),
		TEST_CASE_NAMED("test_mode4_tx_burst", test_mode4_tx_burst_wrapper),
		TEST_CASE_NAMED("test_mode4_marker", test_mode4_marker_wrapper),
		TEST_CASE_NAMED("test_mode4_expired", test_mode4_expired_wrapper),
		TEST_CASE_NAMED("test_mode4_ext_ctrl",
				test_mode4_ext_ctrl_wrapper),
		TEST_CASE_NAMED("test_mode4_ext_lacp",
				test_mode4_ext_lacp_wrapper),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_link_bonding_mode4(void)
{
	return unit_test_suite_runner(&link_bonding_mode4_test_suite);
}

REGISTER_TEST_COMMAND(link_bonding_mode4_autotest, test_link_bonding_mode4);
