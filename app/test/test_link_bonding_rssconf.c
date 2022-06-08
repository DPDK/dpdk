/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
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
#include <rte_bus_vdev.h>

#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_eth_bond.h>

#include "test.h"

#define SLAVE_COUNT (4)

#define RXTX_RING_SIZE			1024
#define RXTX_QUEUE_COUNT		4

#define BONDED_DEV_NAME         ("net_bonding_rss")

#define SLAVE_DEV_NAME_FMT      ("net_null%d")
#define SLAVE_RXTX_QUEUE_FMT      ("rssconf_slave%d_q%d")

#define NUM_MBUFS 8191
#define MBUF_SIZE (1600 + RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

#define INVALID_SOCKET_ID       (-1)
#define INVALID_PORT_ID         (0xFF)
#define INVALID_BONDING_MODE    (-1)

struct slave_conf {
	uint16_t port_id;
	struct rte_eth_dev_info dev_info;

	struct rte_eth_rss_conf rss_conf;
	uint8_t rss_key[40];
	struct rte_eth_rss_reta_entry64 reta_conf[512 / RTE_ETH_RETA_GROUP_SIZE];

	uint8_t is_slave;
	struct rte_ring *rxtx_queue[RXTX_QUEUE_COUNT];
};

struct link_bonding_rssconf_unittest_params {
	uint8_t bond_port_id;
	struct rte_eth_dev_info bond_dev_info;
	struct rte_eth_rss_reta_entry64 bond_reta_conf[512 / RTE_ETH_RETA_GROUP_SIZE];
	struct slave_conf slave_ports[SLAVE_COUNT];

	struct rte_mempool *mbuf_pool;
};

static struct link_bonding_rssconf_unittest_params test_params  = {
	.bond_port_id = INVALID_PORT_ID,
	.slave_ports = {
		[0 ... SLAVE_COUNT - 1] = { .port_id = INVALID_PORT_ID, .is_slave = 0}
	},
	.mbuf_pool = NULL,
};

/**
 * Default port configuration with RSS turned off
 */
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

static struct rte_eth_conf rss_pmd_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_RSS,
		.split_hdr_size = 0,
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = RTE_ETH_RSS_IPV6,
		},
	},
	.lpbk_mode = 0,
};

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

static int
configure_ethdev(uint16_t port_id, struct rte_eth_conf *eth_conf,
		 uint8_t start)
{
	int rxq, txq;

	TEST_ASSERT(rte_eth_dev_configure(port_id, RXTX_QUEUE_COUNT,
			RXTX_QUEUE_COUNT, eth_conf) == 0, "Failed to configure device %u",
			port_id);

	int ret = rte_eth_dev_set_mtu(port_id, 1550);
	RTE_TEST_ASSERT(ret == 0 || ret == -ENOTSUP,
			"rte_eth_dev_set_mtu for port %d failed", port_id);

	for (rxq = 0; rxq < RXTX_QUEUE_COUNT; rxq++) {
		TEST_ASSERT(rte_eth_rx_queue_setup(port_id, rxq, RXTX_RING_SIZE,
				rte_eth_dev_socket_id(port_id), NULL,
				test_params.mbuf_pool) == 0, "Failed to setup rx queue.");
	}

	for (txq = 0; txq < RXTX_QUEUE_COUNT; txq++) {
		TEST_ASSERT(rte_eth_tx_queue_setup(port_id, txq, RXTX_RING_SIZE,
				rte_eth_dev_socket_id(port_id), NULL) == 0,
				"Failed to setup tx queue.");
	}

	if (start) {
		TEST_ASSERT(rte_eth_dev_start(port_id) == 0,
		"Failed to start device (%d).", port_id);
	}

	return 0;
}

/**
 * Remove all slaves from bonding
 */
static int
remove_slaves(void)
{
	unsigned n;
	struct slave_conf *port;

	FOR_EACH_PORT(n, port) {
		port = &test_params.slave_ports[n];
		if (port->is_slave) {
			TEST_ASSERT_SUCCESS(rte_eth_bond_slave_remove(
					test_params.bond_port_id, port->port_id),
					"Cannot remove slave %d from bonding", port->port_id);
			port->is_slave = 0;
		}
	}

	return 0;
}

static int
remove_slaves_and_stop_bonded_device(void)
{
	TEST_ASSERT_SUCCESS(remove_slaves(), "Removing slaves");
	TEST_ASSERT_SUCCESS(rte_eth_dev_stop(test_params.bond_port_id),
			"Failed to stop port %u", test_params.bond_port_id);
	return TEST_SUCCESS;
}

/**
 * Add all slaves to bonding
 */
static int
bond_slaves(void)
{
	unsigned n;
	struct slave_conf *port;

	FOR_EACH_PORT(n, port) {
		port = &test_params.slave_ports[n];
		if (!port->is_slave) {
			TEST_ASSERT_SUCCESS(rte_eth_bond_slave_add(test_params.bond_port_id,
					port->port_id), "Cannot attach slave %d to the bonding",
					port->port_id);
			port->is_slave = 1;
		}
	}

	return 0;
}

/**
 * Set all RETA values in port_id to value
 */
static int
reta_set(uint16_t port_id, uint8_t value, int reta_size)
{
	struct rte_eth_rss_reta_entry64 reta_conf[512/RTE_ETH_RETA_GROUP_SIZE];
	int i, j;

	for (i = 0; i < reta_size / RTE_ETH_RETA_GROUP_SIZE; i++) {
		/* select all fields to set */
		reta_conf[i].mask = ~0LL;
		for (j = 0; j < RTE_ETH_RETA_GROUP_SIZE; j++)
			reta_conf[i].reta[j] = value;
	}

	return rte_eth_dev_rss_reta_update(port_id, reta_conf, reta_size);
}

/**
 * Check if slaves RETA is synchronized with bonding port. Returns 1 if slave
 * port is synced with bonding port.
 */
static int
reta_check_synced(struct slave_conf *port)
{
	unsigned i;

	for (i = 0; i < test_params.bond_dev_info.reta_size;
			i++) {

		int index = i / RTE_ETH_RETA_GROUP_SIZE;
		int shift = i % RTE_ETH_RETA_GROUP_SIZE;

		if (port->reta_conf[index].reta[shift] !=
				test_params.bond_reta_conf[index].reta[shift])
			return 0;

	}

	return 1;
}

/**
 * Fetch bonding ports RETA
 */
static int
bond_reta_fetch(void) {
	unsigned j;

	for (j = 0; j < test_params.bond_dev_info.reta_size / RTE_ETH_RETA_GROUP_SIZE;
			j++)
		test_params.bond_reta_conf[j].mask = ~0LL;

	TEST_ASSERT_SUCCESS(rte_eth_dev_rss_reta_query(test_params.bond_port_id,
			test_params.bond_reta_conf, test_params.bond_dev_info.reta_size),
			"Cannot take bonding ports RSS configuration");
	return 0;
}

/**
 * Fetch slaves RETA
 */
static int
slave_reta_fetch(struct slave_conf *port) {
	unsigned j;

	for (j = 0; j < port->dev_info.reta_size / RTE_ETH_RETA_GROUP_SIZE; j++)
		port->reta_conf[j].mask = ~0LL;

	TEST_ASSERT_SUCCESS(rte_eth_dev_rss_reta_query(port->port_id,
			port->reta_conf, port->dev_info.reta_size),
			"Cannot take bonding ports RSS configuration");
	return 0;
}

/**
 * Remove and add slave to check if slaves configuration is synced with
 * the bonding ports values after adding new slave.
 */
static int
slave_remove_and_add(void)
{
	struct slave_conf *port = &(test_params.slave_ports[0]);

	/* 1. Remove first slave from bonding */
	TEST_ASSERT_SUCCESS(rte_eth_bond_slave_remove(test_params.bond_port_id,
			port->port_id), "Cannot remove slave #d from bonding");

	/* 2. Change removed (ex-)slave and bonding configuration to different
	 *    values
	 */
	reta_set(test_params.bond_port_id, 1, test_params.bond_dev_info.reta_size);
	bond_reta_fetch();

	reta_set(port->port_id, 2, port->dev_info.reta_size);
	slave_reta_fetch(port);

	TEST_ASSERT(reta_check_synced(port) == 0,
			"Removed slave didn't should be synchronized with bonding port");

	/* 3. Add (ex-)slave and check if configuration changed*/
	TEST_ASSERT_SUCCESS(rte_eth_bond_slave_add(test_params.bond_port_id,
			port->port_id), "Cannot add slave");

	bond_reta_fetch();
	slave_reta_fetch(port);

	return reta_check_synced(port);
}

/**
 * Test configuration propagation over slaves.
 */
static int
test_propagate(void)
{
	unsigned i;
	uint8_t n;
	struct slave_conf *port;
	uint8_t bond_rss_key[40];
	struct rte_eth_rss_conf bond_rss_conf;

	int retval = 0;
	uint64_t rss_hf = 0;
	uint64_t default_rss_hf = 0;

	retval = rte_eth_dev_info_get(test_params.bond_port_id,
						&test_params.bond_dev_info);
	TEST_ASSERT((retval == 0),
			"Error during getting device (port %u) info: %s\n",
			test_params.bond_port_id, strerror(-retval));

	/*
	 *  Test hash function propagation
	 */
	for (i = 0; i < sizeof(test_params.bond_dev_info.flow_type_rss_offloads)*8;
			i++) {

		rss_hf = test_params.bond_dev_info.flow_type_rss_offloads & (1<<i);
		if (rss_hf) {
			bond_rss_conf.rss_key = NULL;
			bond_rss_conf.rss_hf = rss_hf;

			retval = rte_eth_dev_rss_hash_update(test_params.bond_port_id,
					&bond_rss_conf);
			TEST_ASSERT_SUCCESS(retval, "Cannot set slaves hash function");

			FOR_EACH_PORT(n, port) {
				port = &test_params.slave_ports[n];

				retval = rte_eth_dev_rss_hash_conf_get(port->port_id,
						&port->rss_conf);
				TEST_ASSERT_SUCCESS(retval,
						"Cannot take slaves RSS configuration");

				TEST_ASSERT(port->rss_conf.rss_hf == rss_hf,
						"Hash function not propagated for slave %d",
						port->port_id);
			}

			default_rss_hf = rss_hf;
		}

	}

	/*
	 *  Test key propagation
	 */
	for (i = 1; i < 10; i++) {

		/* Set all keys to zero */
		FOR_EACH_PORT(n, port) {
			port = &test_params.slave_ports[n];
			memset(port->rss_conf.rss_key, 0, 40);
			retval = rte_eth_dev_rss_hash_update(port->port_id,
					&port->rss_conf);
			TEST_ASSERT_SUCCESS(retval, "Cannot set slaves RSS keys");
		}

		memset(bond_rss_key, i, sizeof(bond_rss_key));
		bond_rss_conf.rss_hf = default_rss_hf,
		bond_rss_conf.rss_key = bond_rss_key;
		bond_rss_conf.rss_key_len = 40;

		retval = rte_eth_dev_rss_hash_update(test_params.bond_port_id,
				&bond_rss_conf);
		TEST_ASSERT_SUCCESS(retval, "Cannot set bonded port RSS keys");

		FOR_EACH_PORT(n, port) {
			port = &test_params.slave_ports[n];

			retval = rte_eth_dev_rss_hash_conf_get(port->port_id,
					&(port->rss_conf));

			TEST_ASSERT_SUCCESS(retval,
					"Cannot take slaves RSS configuration");

			/* compare keys */
			retval = memcmp(port->rss_conf.rss_key, bond_rss_key,
					sizeof(bond_rss_key));
			TEST_ASSERT(retval == 0, "Key value not propagated for slave %d",
					port->port_id);
		}
	}

	/*
	 *  Test RETA propagation
	 */
	for (i = 0; i < RXTX_QUEUE_COUNT; i++) {

		/* Set all keys to zero */
		FOR_EACH_PORT(n, port) {
			port = &test_params.slave_ports[n];
			retval = reta_set(port->port_id, (i + 1) % RXTX_QUEUE_COUNT,
					port->dev_info.reta_size);
			TEST_ASSERT_SUCCESS(retval, "Cannot set slaves RETA");
		}

		TEST_ASSERT_SUCCESS(reta_set(test_params.bond_port_id,
				i % RXTX_QUEUE_COUNT, test_params.bond_dev_info.reta_size),
				"Cannot set bonded port RETA");

		bond_reta_fetch();

		FOR_EACH_PORT(n, port) {
			port = &test_params.slave_ports[n];

			slave_reta_fetch(port);
			TEST_ASSERT(reta_check_synced(port) == 1, "RETAs inconsistent");
		}
	}

	return TEST_SUCCESS;
}

/**
 * Test propagation logic, when RX_RSS mq_mode is turned on for bonding port
 */
static int
test_rss(void)
{
	/**
	 * Configure bonding port in RSS mq mode
	 */
	int ret;

	TEST_ASSERT_SUCCESS(configure_ethdev(test_params.bond_port_id,
			&rss_pmd_conf, 0), "Failed to configure bonding device\n");

	ret = rte_eth_dev_info_get(test_params.bond_port_id,
					&test_params.bond_dev_info);
	TEST_ASSERT((ret == 0),
			"Error during getting device (port %u) info: %s\n",
			test_params.bond_port_id, strerror(-ret));

	TEST_ASSERT_SUCCESS(bond_slaves(), "Bonding slaves failed");

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params.bond_port_id),
			"Failed to start bonding port (%d).", test_params.bond_port_id);

	TEST_ASSERT_SUCCESS(test_propagate(), "Propagation test failed");

	TEST_ASSERT(slave_remove_and_add() == 1, "remove and add slaves success.");

	remove_slaves_and_stop_bonded_device();

	return TEST_SUCCESS;
}


/**
 * Test RSS configuration over bonded and slaves.
 */
static int
test_rss_config_lazy(void)
{
	struct rte_eth_rss_conf bond_rss_conf = {0};
	struct slave_conf *port;
	uint8_t rss_key[40];
	uint64_t rss_hf;
	int retval;
	uint16_t i;
	uint8_t n;

	retval = rte_eth_dev_info_get(test_params.bond_port_id,
				      &test_params.bond_dev_info);
	TEST_ASSERT((retval == 0), "Error during getting device (port %u) info: %s\n",
		    test_params.bond_port_id, strerror(-retval));

	rss_hf = test_params.bond_dev_info.flow_type_rss_offloads;
	if (rss_hf != 0) {
		bond_rss_conf.rss_key = NULL;
		bond_rss_conf.rss_hf = rss_hf;
		retval = rte_eth_dev_rss_hash_update(test_params.bond_port_id,
						     &bond_rss_conf);
		TEST_ASSERT(retval != 0, "Succeeded in setting bonded port hash function");
	}

	/* Set all keys to zero for all slaves */
	FOR_EACH_PORT(n, port) {
		port = &test_params.slave_ports[n];
		retval = rte_eth_dev_rss_hash_conf_get(port->port_id,
						       &port->rss_conf);
		TEST_ASSERT_SUCCESS(retval, "Cannot get slaves RSS configuration");
		memset(port->rss_key, 0, sizeof(port->rss_key));
		port->rss_conf.rss_key = port->rss_key;
		port->rss_conf.rss_key_len = sizeof(port->rss_key);
		retval = rte_eth_dev_rss_hash_update(port->port_id,
						     &port->rss_conf);
		TEST_ASSERT(retval != 0, "Succeeded in setting slaves RSS keys");
	}

	/* Set RSS keys for bonded port */
	memset(rss_key, 1, sizeof(rss_key));
	bond_rss_conf.rss_hf = rss_hf;
	bond_rss_conf.rss_key = rss_key;
	bond_rss_conf.rss_key_len = sizeof(rss_key);

	retval = rte_eth_dev_rss_hash_update(test_params.bond_port_id,
					     &bond_rss_conf);
	TEST_ASSERT(retval != 0, "Succeeded in setting bonded port RSS keys");

	/*  Test RETA propagation */
	for (i = 0; i < RXTX_QUEUE_COUNT; i++) {
		FOR_EACH_PORT(n, port) {
			port = &test_params.slave_ports[n];
			retval = reta_set(port->port_id, (i + 1) % RXTX_QUEUE_COUNT,
					  port->dev_info.reta_size);
			TEST_ASSERT(retval != 0, "Succeeded in setting slaves RETA");
		}

		retval = reta_set(test_params.bond_port_id, i % RXTX_QUEUE_COUNT,
				  test_params.bond_dev_info.reta_size);
		TEST_ASSERT(retval != 0, "Succeeded in setting bonded port RETA");
	}

	return TEST_SUCCESS;
}

/**
 * Test RSS function logic, when RX_RSS mq_mode is turned off for bonding port
 */
static int
test_rss_lazy(void)
{
	int ret;

	TEST_ASSERT_SUCCESS(configure_ethdev(test_params.bond_port_id,
			&default_pmd_conf, 0), "Failed to configure bonding device\n");

	ret = rte_eth_dev_info_get(test_params.bond_port_id,
						&test_params.bond_dev_info);
	TEST_ASSERT((ret == 0),
			"Error during getting device (port %u) info: %s\n",
			test_params.bond_port_id, strerror(-ret));

	TEST_ASSERT_SUCCESS(bond_slaves(), "Bonding slaves failed");

	TEST_ASSERT_SUCCESS(rte_eth_dev_start(test_params.bond_port_id),
			"Failed to start bonding port (%d).", test_params.bond_port_id);

	TEST_ASSERT_SUCCESS(test_rss_config_lazy(), "Succeeded in setting RSS hash when RX_RSS mq_mode is turned off");

	remove_slaves_and_stop_bonded_device();

	return TEST_SUCCESS;
}

static int
test_setup(void)
{
	unsigned n;
	int retval;
	int port_id;
	char name[256];
	struct slave_conf *port;
	struct rte_ether_addr mac_addr = { .addr_bytes = {0} };

	if (test_params.mbuf_pool == NULL) {

		test_params.mbuf_pool = rte_pktmbuf_pool_create(
			"RSS_MBUF_POOL", NUM_MBUFS * SLAVE_COUNT,
			MBUF_CACHE_SIZE, 0, MBUF_SIZE, rte_socket_id());

		TEST_ASSERT(test_params.mbuf_pool != NULL,
				"rte_pktmbuf_pool_create failed\n");
	}

	/* Create / initialize ring eth devs. */
	FOR_EACH_PORT(n, port) {
		port = &test_params.slave_ports[n];

		port_id = rte_eth_dev_count_avail();
		snprintf(name, sizeof(name), SLAVE_DEV_NAME_FMT, port_id);

		retval = rte_vdev_init(name, "size=64,copy=0");
		TEST_ASSERT_SUCCESS(retval, "Failed to create null device '%s'\n",
				name);

		port->port_id = port_id;

		port->rss_conf.rss_key = port->rss_key;
		port->rss_conf.rss_key_len = 40;

		retval = configure_ethdev(port->port_id, &default_pmd_conf, 0);
		TEST_ASSERT_SUCCESS(retval, "Failed to configure virtual ethdev %s\n",
				name);

		/* assign a non-zero MAC */
		mac_addr.addr_bytes[5] = 0x10 + port->port_id;
		rte_eth_dev_default_mac_addr_set(port->port_id, &mac_addr);

		rte_eth_dev_info_get(port->port_id, &port->dev_info);
		retval = rte_eth_dev_info_get(port->port_id, &port->dev_info);
		TEST_ASSERT((retval == 0),
				"Error during getting device (port %u) info: %s\n",
				test_params.bond_port_id, strerror(-retval));
	}

	if (test_params.bond_port_id == INVALID_PORT_ID) {
		retval = rte_eth_bond_create(BONDED_DEV_NAME, 0, rte_socket_id());

		TEST_ASSERT(retval >= 0, "Failed to create bonded ethdev %s",
				BONDED_DEV_NAME);

		test_params.bond_port_id = retval;

		TEST_ASSERT_SUCCESS(configure_ethdev(test_params.bond_port_id,
				&default_pmd_conf, 0), "Failed to configure bonding device\n");

		retval = rte_eth_dev_info_get(test_params.bond_port_id,
						&test_params.bond_dev_info);
		TEST_ASSERT((retval == 0),
				"Error during getting device (port %u) info: %s\n",
				test_params.bond_port_id, strerror(-retval));
	}

	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
	struct slave_conf *port;
	uint8_t i;

	/* Only stop ports.
	 * Any cleanup/reset state is done when particular test is
	 * started. */

	rte_eth_dev_stop(test_params.bond_port_id);

	FOR_EACH_PORT(i, port)
		rte_eth_dev_stop(port->port_id);
}

static int
check_environment(void)
{
	return TEST_SUCCESS;
}

static int
test_rssconf_executor(int (*test_func)(void))
{
	int test_result;

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
	}

	return test_result;
}

static int
test_setup_wrapper(void)
{
	return test_rssconf_executor(&test_setup);
}

static int
test_rss_wrapper(void)
{
	return test_rssconf_executor(&test_rss);
}

static int
test_rss_lazy_wrapper(void)
{
	return test_rssconf_executor(&test_rss_lazy);
}

static struct unit_test_suite link_bonding_rssconf_test_suite  = {
	.suite_name = "RSS Dynamic Configuration for Bonding Unit Test Suite",
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_NAMED("test_setup", test_setup_wrapper),
		TEST_CASE_NAMED("test_rss", test_rss_wrapper),
		TEST_CASE_NAMED("test_rss_lazy", test_rss_lazy_wrapper),

		TEST_CASES_END()
	}
};

static int
test_link_bonding_rssconf(void)
{
	return unit_test_suite_runner(&link_bonding_rssconf_test_suite);
}

REGISTER_TEST_COMMAND(link_bonding_rssconf_autotest, test_link_bonding_rssconf);
