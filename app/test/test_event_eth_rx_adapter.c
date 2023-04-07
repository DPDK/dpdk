/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include "test.h"

#include <string.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_event_eth_rx_adapter_common(void)
{
	printf("event_eth_rx_adapter not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

static int
test_event_eth_rx_intr_adapter_common(void)
{
	printf("event_eth_rx_intr_adapter not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_eventdev.h>
#include <rte_bus_vdev.h>

#include <rte_event_eth_rx_adapter.h>

#define MAX_NUM_RX_QUEUE	64
#define NB_MBUFS		(8192 * num_ports * MAX_NUM_RX_QUEUE)
#define MBUF_CACHE_SIZE		512
#define MBUF_PRIV_SIZE		0
#define TEST_INST_ID		0
#define TEST_DEV_ID		0
#define TEST_ETHDEV_ID		0
#define TEST_ETH_QUEUE_ID	0

struct event_eth_rx_adapter_test_params {
	struct rte_mempool *mp;
	uint16_t rx_rings, tx_rings;
	uint32_t caps;
	int rx_intr_port_inited;
	uint16_t rx_intr_port;
};

static struct event_eth_rx_adapter_test_params default_params;
static bool event_dev_created;
static bool eth_dev_created;

static inline int
port_init_common(uint16_t port, const struct rte_eth_conf *port_conf,
		struct rte_mempool *mp)
{
	const uint16_t rx_ring_size = 512, tx_ring_size = 512;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	retval = rte_eth_dev_configure(port, 0, 0, port_conf);

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0)
		return retval;

	default_params.rx_rings = RTE_MIN(dev_info.max_rx_queues,
					MAX_NUM_RX_QUEUE);
	default_params.tx_rings = 1;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, default_params.rx_rings,
				default_params.tx_rings, port_conf);
	if (retval != 0)
		return retval;

	for (q = 0; q < default_params.rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, rx_ring_size,
				rte_eth_dev_socket_id(port), NULL, mp);
		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < default_params.tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, tx_ring_size,
				rte_eth_dev_socket_id(port), NULL);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(port, &addr);
	if (retval < 0)
		return retval;
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned int)port, RTE_ETHER_ADDR_BYTES(&addr));

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;

	return 0;
}

static inline int
port_init_rx_intr(uint16_t port, struct rte_mempool *mp)
{
	static const struct rte_eth_conf port_conf_default = {
		.rxmode = {
			.mq_mode = RTE_ETH_MQ_RX_NONE,
		},
		.intr_conf = {
			.rxq = 1,
		},
	};

	return port_init_common(port, &port_conf_default, mp);
}

static inline int
port_init(uint16_t port, struct rte_mempool *mp)
{
	static const struct rte_eth_conf port_conf_default = {
		.rxmode = {
			.mq_mode = RTE_ETH_MQ_RX_NONE,
		},
	};

	return port_init_common(port, &port_conf_default, mp);
}

static int
init_port_rx_intr(int num_ports)
{
	int retval;
	uint16_t portid;
	int err;

	default_params.mp = rte_pktmbuf_pool_create("packet_pool",
						   NB_MBUFS,
						   MBUF_CACHE_SIZE,
						   MBUF_PRIV_SIZE,
						   RTE_MBUF_DEFAULT_BUF_SIZE,
						   rte_socket_id());
	if (!default_params.mp)
		return -ENOMEM;

	RTE_ETH_FOREACH_DEV(portid) {
		retval = port_init_rx_intr(portid, default_params.mp);
		if (retval)
			continue;
		err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID, portid,
							&default_params.caps);
		if (err)
			continue;
		if (!(default_params.caps &
			RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT)) {
			default_params.rx_intr_port_inited = 1;
			default_params.rx_intr_port = portid;
			return 0;
		}
		retval = rte_eth_dev_stop(portid);
		TEST_ASSERT(retval == 0, "Failed to stop port %u: %d\n",
					portid, retval);
	}
	return 0;
}

static int
init_ports(int num_ports)
{
	uint16_t portid;
	int retval;

	struct rte_mempool *ptr = rte_mempool_lookup("packet_pool");

	if (ptr == NULL)
		default_params.mp = rte_pktmbuf_pool_create("packet_pool",
						NB_MBUFS,
						MBUF_CACHE_SIZE,
						MBUF_PRIV_SIZE,
						RTE_MBUF_DEFAULT_BUF_SIZE,
						rte_socket_id());
	else
		default_params.mp = ptr;

	if (!default_params.mp)
		return -ENOMEM;

	RTE_ETH_FOREACH_DEV(portid) {
		retval = port_init(portid, default_params.mp);
		if (retval)
			return retval;
	}

	return 0;
}

static int
testsuite_setup(void)
{
	int err;
	uint8_t count;
	struct rte_event_dev_info dev_info;

	count = rte_event_dev_count();
	if (!count) {
		printf("Failed to find a valid event device,"
			" testing with event_skeleton device\n");
		err = rte_vdev_init("event_skeleton", NULL);
		TEST_ASSERT(err == 0, "Failed to create event_skeleton. err=%d",
			    err);
		event_dev_created = true;
	}

	struct rte_event_dev_config config = {
			.nb_event_queues = 1,
			.nb_event_ports = 1,
	};

	err = rte_event_dev_info_get(TEST_DEV_ID, &dev_info);
	config.nb_event_queue_flows = dev_info.max_event_queue_flows;
	config.nb_event_port_dequeue_depth =
			dev_info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth =
			dev_info.max_event_port_enqueue_depth;
	config.nb_events_limit =
			dev_info.max_num_events;
	err = rte_event_dev_configure(TEST_DEV_ID, &config);
	TEST_ASSERT(err == 0, "Event device initialization failed err %d\n",
			err);

	count = rte_eth_dev_count_total();
	if (!count) {
		printf("Testing with net_null device\n");
		err = rte_vdev_init("net_null", NULL);
		TEST_ASSERT(err == 0, "Failed to create net_null. err=%d",
			    err);
		eth_dev_created = true;
	}

	/*
	 * eth devices like octeontx use event device to receive packets
	 * so rte_eth_dev_start invokes rte_event_dev_start internally, so
	 * call init_ports after rte_event_dev_configure
	 */
	err = init_ports(rte_eth_dev_count_total());
	TEST_ASSERT(err == 0, "Port initialization failed err %d\n", err);

	err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID, TEST_ETHDEV_ID,
						&default_params.caps);
	TEST_ASSERT(err == 0, "Failed to get adapter cap err %d\n",
			err);

	return err;
}

static int
testsuite_setup_rx_intr(void)
{
	int err;
	uint8_t count;
	struct rte_event_dev_info dev_info;

	count = rte_event_dev_count();
	if (!count) {
		printf("Failed to find a valid event device,"
			" testing with event_skeleton device\n");
		err = rte_vdev_init("event_skeleton", NULL);
		TEST_ASSERT(err == 0, "Failed to create event_skeleton. err=%d",
			    err);
		event_dev_created = true;
	}

	struct rte_event_dev_config config = {
		.nb_event_queues = 1,
		.nb_event_ports = 1,
	};

	err = rte_event_dev_info_get(TEST_DEV_ID, &dev_info);
	config.nb_event_queue_flows = dev_info.max_event_queue_flows;
	config.nb_event_port_dequeue_depth =
			dev_info.max_event_port_dequeue_depth;
	config.nb_event_port_enqueue_depth =
			dev_info.max_event_port_enqueue_depth;
	config.nb_events_limit =
			dev_info.max_num_events;

	err = rte_event_dev_configure(TEST_DEV_ID, &config);
	TEST_ASSERT(err == 0, "Event device initialization failed err %d\n",
			err);

	count = rte_eth_dev_count_total();
	if (!count) {
		printf("Testing with net_null device\n");
		err = rte_vdev_init("net_null", NULL);
		TEST_ASSERT(err == 0, "Failed to create net_null. err=%d",
			    err);
		eth_dev_created = true;
	}

	/*
	 * eth devices like octeontx use event device to receive packets
	 * so rte_eth_dev_start invokes rte_event_dev_start internally, so
	 * call init_ports after rte_event_dev_configure
	 */
	err = init_port_rx_intr(rte_eth_dev_count_total());
	TEST_ASSERT(err == 0, "Port initialization failed err %d\n", err);

	if (!default_params.rx_intr_port_inited)
		return 0;

	err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID,
						default_params.rx_intr_port,
						&default_params.caps);
	TEST_ASSERT(err == 0, "Failed to get adapter cap err %d\n", err);

	return err;
}

static void
testsuite_teardown(void)
{
	int err;
	uint32_t i;
	RTE_ETH_FOREACH_DEV(i)
		rte_eth_dev_stop(i);

	if (eth_dev_created) {
		err = rte_vdev_uninit("net_null");
		if (err)
			printf("Failed to delete net_null. err=%d", err);
		eth_dev_created = false;
	}

	rte_mempool_free(default_params.mp);
	if (event_dev_created) {
		err = rte_vdev_uninit("event_skeleton");
		if (err)
			printf("Failed to delete event_skeleton. err=%d", err);
		event_dev_created = false;
	}

	memset(&default_params, 0, sizeof(default_params));
}

static void
testsuite_teardown_rx_intr(void)
{
	int err;
	if (!default_params.rx_intr_port_inited)
		return;

	rte_eth_dev_stop(default_params.rx_intr_port);
	if (eth_dev_created) {
		err = rte_vdev_uninit("net_null");
		if (err)
			printf("Failed to delete net_null. err=%d", err);
		eth_dev_created = false;
	}
	rte_mempool_free(default_params.mp);
	if (event_dev_created) {
		err = rte_vdev_uninit("event_skeleton");
		if (err)
			printf("Failed to delete event_skeleton. err=%d", err);
		event_dev_created = false;
	}

	memset(&default_params, 0, sizeof(default_params));
}

static int
adapter_create(void)
{
	int err;
	struct rte_event_dev_info dev_info;
	struct rte_event_port_conf rx_p_conf;

	memset(&rx_p_conf, 0, sizeof(rx_p_conf));

	err = rte_event_dev_info_get(TEST_DEV_ID, &dev_info);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	rx_p_conf.new_event_threshold = dev_info.max_num_events;
	rx_p_conf.dequeue_depth = dev_info.max_event_port_dequeue_depth;
	rx_p_conf.enqueue_depth = dev_info.max_event_port_enqueue_depth;
	err = rte_event_eth_rx_adapter_create(TEST_INST_ID, TEST_DEV_ID,
					&rx_p_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return err;
}

static int
adapter_create_with_params(void)
{
	int err;
	struct rte_event_dev_info dev_info;
	struct rte_event_port_conf rx_p_conf;
	struct rte_event_eth_rx_adapter_params rxa_params;

	memset(&rx_p_conf, 0, sizeof(rx_p_conf));

	err = rte_event_dev_info_get(TEST_DEV_ID, &dev_info);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	rx_p_conf.new_event_threshold = dev_info.max_num_events;
	rx_p_conf.dequeue_depth = dev_info.max_event_port_dequeue_depth;
	rx_p_conf.enqueue_depth = dev_info.max_event_port_enqueue_depth;

	rxa_params.use_queue_event_buf = false;
	rxa_params.event_buf_size = 0;

	err = rte_event_eth_rx_adapter_create_with_params(TEST_INST_ID,
				TEST_DEV_ID, &rx_p_conf, &rxa_params);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	rxa_params.use_queue_event_buf = true;

	err = rte_event_eth_rx_adapter_create_with_params(TEST_INST_ID,
				TEST_DEV_ID, &rx_p_conf, &rxa_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_create_with_params(TEST_INST_ID,
				TEST_DEV_ID, &rx_p_conf, &rxa_params);
	TEST_ASSERT(err == -EEXIST, "Expected -EEXIST got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_queue_event_buf_test(void)
{
	int err;
	struct rte_event ev;
	uint32_t cap;

	struct rte_event_eth_rx_adapter_queue_conf queue_config = {0};

	err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID, TEST_ETHDEV_ID,
					 &cap);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	ev.queue_id = 0;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.priority = 0;

	queue_config.rx_queue_flags = 0;
	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID) {
		ev.flow_id = 1;
		queue_config.rx_queue_flags =
			RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID;
	}
	queue_config.ev = ev;
	queue_config.servicing_weight = 1;
	queue_config.event_buf_size = 0;

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
					TEST_ETHDEV_ID, 0,
					&queue_config);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	queue_config.event_buf_size = 1024;

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
					TEST_ETHDEV_ID, 0,
					&queue_config);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
						TEST_ETHDEV_ID,
						0);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_queue_stats_test(void)
{
	int err;
	struct rte_event ev;
	uint32_t cap;
	struct rte_event_eth_rx_adapter_queue_conf queue_config = {0};
	struct rte_event_eth_rx_adapter_queue_stats q_stats;

	err = rte_event_eth_rx_adapter_queue_stats_get(TEST_INST_ID,
						TEST_ETHDEV_ID, 0,
						&q_stats);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_queue_stats_reset(TEST_INST_ID,
						TEST_ETHDEV_ID, 0);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID, TEST_ETHDEV_ID,
					 &cap);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	ev.queue_id = 0;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.priority = 0;

	queue_config.rx_queue_flags = 0;
	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID) {
		ev.flow_id = 1;
		queue_config.rx_queue_flags =
			RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID;
	}
	queue_config.ev = ev;
	queue_config.servicing_weight = 1;
	queue_config.event_buf_size = 1024;

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
					TEST_ETHDEV_ID, 0,
					&queue_config);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_queue_stats_get(TEST_INST_ID,
						TEST_ETHDEV_ID, 0,
						&q_stats);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_queue_stats_reset(TEST_INST_ID,
						TEST_ETHDEV_ID, 0);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
						TEST_ETHDEV_ID,
						0);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return TEST_SUCCESS;
}

static void
adapter_free(void)
{
	rte_event_eth_rx_adapter_free(TEST_INST_ID);
}

static int
adapter_create_free(void)
{
	int err;

	struct rte_event_port_conf rx_p_conf = {
			.dequeue_depth = 8,
			.enqueue_depth = 8,
			.new_event_threshold = 1200,
	};

	err = rte_event_eth_rx_adapter_create(TEST_INST_ID, TEST_DEV_ID,
					NULL);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_create(TEST_INST_ID, TEST_DEV_ID,
					&rx_p_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_create(TEST_INST_ID,
					TEST_DEV_ID, &rx_p_conf);
	TEST_ASSERT(err == -EEXIST, "Expected -EEXIST %d got %d", -EEXIST, err);

	err = rte_event_eth_rx_adapter_free(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_free(TEST_INST_ID);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL %d got %d", -EINVAL, err);

	err = rte_event_eth_rx_adapter_free(1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL %d got %d", -EINVAL, err);

	return TEST_SUCCESS;
}

static int
adapter_create_free_with_params(void)
{
	int err;

	struct rte_event_port_conf rx_p_conf = {
			.dequeue_depth = 8,
			.enqueue_depth = 8,
			.new_event_threshold = 1200,
	};

	struct rte_event_eth_rx_adapter_params rxa_params = {
			.event_buf_size = 1024
	};

	err = rte_event_eth_rx_adapter_create_with_params(TEST_INST_ID,
				TEST_DEV_ID, NULL, NULL);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_create_with_params(TEST_INST_ID,
				TEST_DEV_ID, &rx_p_conf, &rxa_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_create_with_params(TEST_INST_ID,
				TEST_DEV_ID, &rx_p_conf, &rxa_params);
	TEST_ASSERT(err == -EEXIST, "Expected -EEXIST %d got %d", -EEXIST, err);

	rxa_params.event_buf_size = 0;
	err = rte_event_eth_rx_adapter_create_with_params(TEST_INST_ID,
				TEST_DEV_ID, &rx_p_conf, &rxa_params);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_free(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_free(TEST_INST_ID);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL %d got %d", -EINVAL, err);

	err = rte_event_eth_rx_adapter_free(1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL %d got %d", -EINVAL, err);

	return TEST_SUCCESS;
}

static int
adapter_queue_add_del(void)
{
	int err;
	struct rte_event ev;
	uint32_t cap;

	struct rte_event_eth_rx_adapter_queue_conf queue_config = {0};

	err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID, TEST_ETHDEV_ID,
					 &cap);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	ev.queue_id = 0;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.priority = 0;

	queue_config.rx_queue_flags = 0;
	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID) {
		ev.flow_id = 1;
		queue_config.rx_queue_flags =
			RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID;
	}
	queue_config.ev = ev;
	queue_config.servicing_weight = 1;

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						rte_eth_dev_count_total(),
						-1, &queue_config);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ) {
		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID, 0,
							&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID, 0);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID,
							-1,
							&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID,
							-1);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	} else {
		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID,
							0,
							&queue_config);
		TEST_ASSERT(err == -EINVAL, "Expected EINVAL got %d", err);

		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID, -1,
							&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID, 0);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID, -1);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);

		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID, -1);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	err = rte_event_eth_rx_adapter_queue_add(1, TEST_ETHDEV_ID, -1,
						&queue_config);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_queue_del(1, TEST_ETHDEV_ID, -1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_multi_eth_add_del(void)
{
	int err;
	struct rte_event ev;

	uint16_t port_index, port_index_base, drv_id = 0;
	char driver_name[50];

	struct rte_event_eth_rx_adapter_queue_conf queue_config = {0};

	ev.queue_id = 0;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.priority = 0;

	queue_config.rx_queue_flags = 0;
	queue_config.ev = ev;
	queue_config.servicing_weight = 1;

	/* stop eth devices for existing */
	port_index = 0;
	for (; port_index < rte_eth_dev_count_total(); port_index += 1) {
		err = rte_eth_dev_stop(port_index);
		TEST_ASSERT(err == 0, "Failed to stop port %u: %d\n",
					port_index, err);
	}

	/* add the max port for rx_adapter */
	port_index = rte_eth_dev_count_total();
	port_index_base = port_index;
	for (; port_index < RTE_MAX_ETHPORTS; port_index += 1) {
		snprintf(driver_name, sizeof(driver_name), "%s%u", "net_null",
				drv_id);
		err = rte_vdev_init(driver_name, NULL);
		TEST_ASSERT(err == 0, "Failed driver %s got %d",
		driver_name, err);
		drv_id += 1;
	}

	err = init_ports(rte_eth_dev_count_total());
	TEST_ASSERT(err == 0, "Port initialization failed err %d\n", err);

	/* eth_rx_adapter_queue_add for n ports */
	port_index = 0;
	for (; port_index < rte_eth_dev_count_total(); port_index += 1) {
		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
				port_index, -1,
				&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	/* eth_rx_adapter_queue_del n ports */
	port_index = 0;
	for (; port_index < rte_eth_dev_count_total(); port_index += 1) {
		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
				port_index, -1);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	/* delete vdev ports */
	for (drv_id = 0, port_index = port_index_base;
	     port_index < RTE_MAX_ETHPORTS;
	     drv_id += 1, port_index += 1) {
		snprintf(driver_name, sizeof(driver_name), "%s%u", "net_null",
				drv_id);
		err = rte_vdev_uninit(driver_name);
		TEST_ASSERT(err == 0, "Failed driver %s got %d",
			    driver_name, err);
	}

	return TEST_SUCCESS;
}

static int
adapter_intr_queue_add_del(void)
{
	int err;
	struct rte_event ev;
	uint32_t cap;
	uint16_t eth_port;
	struct rte_event_eth_rx_adapter_queue_conf queue_config = {0};

	if (!default_params.rx_intr_port_inited)
		return 0;

	eth_port = default_params.rx_intr_port;
	err = rte_event_eth_rx_adapter_caps_get(TEST_DEV_ID, eth_port, &cap);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	ev.queue_id = 0;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.priority = 0;

	queue_config.rx_queue_flags = 0;
	queue_config.ev = ev;

	/* weight = 0 => interrupt mode */
	queue_config.servicing_weight = 0;

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ) {
		/* add queue 0 */
		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID, 0,
							&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	/* add all queues */
	queue_config.servicing_weight = 0;
	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1,
						&queue_config);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ) {
		/* del queue 0 */
		err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
							TEST_ETHDEV_ID,
							0);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	/* del remaining queues */
	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* add all queues */
	queue_config.servicing_weight = 0;
	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1,
						&queue_config);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* intr -> poll mode queue */
	queue_config.servicing_weight = 1;

	if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ) {
		err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
							TEST_ETHDEV_ID,
							0,
							&queue_config);
		TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	}

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1,
						 &queue_config);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* del queues */
	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
						TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_start_stop(void)
{
	int err;
	struct rte_event ev;

	ev.queue_id = 0;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.priority = 0;

	struct rte_event_eth_rx_adapter_queue_conf queue_config = {0};

	queue_config.rx_queue_flags = 0;
	if (default_params.caps &
		RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID) {
		ev.flow_id = 1;
		queue_config.rx_queue_flags =
			RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID;
	}

	queue_config.ev = ev;
	queue_config.servicing_weight = 1;

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID, TEST_ETHDEV_ID,
					-1, &queue_config);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_start(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_stop(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID, TEST_ETHDEV_ID,
						-1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_start(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_stop(TEST_INST_ID);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_start(1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_stop(1);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_stats(void)
{
	int err;
	struct rte_event_eth_rx_adapter_stats stats;

	err = rte_event_eth_rx_adapter_stats_get(TEST_INST_ID, NULL);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	err = rte_event_eth_rx_adapter_stats_get(TEST_INST_ID, &stats);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_stats_get(1, &stats);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_queue_conf(void)
{
	int err;
	struct rte_event_eth_rx_adapter_queue_conf queue_conf = {0};

	/* Case 1: queue conf get without any queues in Rx adapter */
	err = rte_event_eth_rx_adapter_queue_conf_get(TEST_INST_ID,
						      TEST_ETHDEV_ID,
						      0, &queue_conf);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Add queue to Rx adapter */
	queue_conf.ev.queue_id = 0;
	queue_conf.ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	queue_conf.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 0, &queue_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* Case 2: queue conf get with queue added to Rx adapter */
	err = rte_event_eth_rx_adapter_queue_conf_get(TEST_INST_ID,
						      TEST_ETHDEV_ID,
						      0, &queue_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* Case 3: queue conf get with invalid rx queue id */
	err = rte_event_eth_rx_adapter_queue_conf_get(TEST_INST_ID,
						      TEST_ETHDEV_ID,
						      -1, &queue_conf);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 4: queue conf get with NULL queue conf struct */
	err = rte_event_eth_rx_adapter_queue_conf_get(TEST_INST_ID,
						      TEST_ETHDEV_ID,
						      0, NULL);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Delete queue from the Rx adapter */
	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 0);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_pollq_instance_get(void)
{
	int err;
	uint8_t inst_id;
	uint16_t eth_dev_id;
	struct rte_eth_dev_info dev_info;
	struct rte_event_eth_rx_adapter_queue_conf queue_conf = {0};

	/* Case 1: Test without configuring eth */
	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 2: Test with wrong eth port */
	eth_dev_id = rte_eth_dev_count_total() + 1;
	err = rte_event_eth_rx_adapter_instance_get(eth_dev_id,
						    TEST_ETH_QUEUE_ID,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 3: Test with wrong rx queue */
	err = rte_eth_dev_info_get(TEST_ETHDEV_ID, &dev_info);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    dev_info.max_rx_queues + 1,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 4: Test with right instance, port & rxq */
	/* Add queue 1 to Rx adapter */
	queue_conf.ev.queue_id = TEST_ETH_QUEUE_ID;
	queue_conf.ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	queue_conf.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
	queue_conf.servicing_weight = 1; /* poll queue */

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 TEST_ETH_QUEUE_ID,
						 &queue_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID,
						    &inst_id);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(inst_id == TEST_INST_ID, "Expected %d got %d",
		    TEST_INST_ID, err);

	/* Add queue 2 to Rx adapter */
	queue_conf.ev.queue_id = TEST_ETH_QUEUE_ID + 1;
	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 TEST_ETH_QUEUE_ID + 1,
						 &queue_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 1,
						    &inst_id);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(inst_id == TEST_INST_ID, "Expected %d got %d",
		    TEST_INST_ID, err);

	/* Add queue 3 to Rx adapter */
	queue_conf.ev.queue_id = TEST_ETH_QUEUE_ID + 2;
	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 TEST_ETH_QUEUE_ID + 2,
						 &queue_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 2,
						    &inst_id);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(inst_id == TEST_INST_ID, "Expected %d got %d",
		    TEST_INST_ID, err);

	/* Case 5: Test with right instance, port & wrong rxq */
	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 3,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Delete all queues from the Rx adapter */
	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 -1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_intrq_instance_get(void)
{
	int err;
	uint8_t inst_id;
	uint16_t eth_dev_id;
	struct rte_eth_dev_info dev_info;
	struct rte_event_eth_rx_adapter_queue_conf queue_conf = {0};

	/* Case 1: Test without configuring eth */
	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 2: Test with wrong eth port */
	eth_dev_id = rte_eth_dev_count_total() + 1;
	err = rte_event_eth_rx_adapter_instance_get(eth_dev_id,
						    TEST_ETH_QUEUE_ID,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 3: Test with wrong rx queue */
	err = rte_eth_dev_info_get(TEST_ETHDEV_ID, &dev_info);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    dev_info.max_rx_queues + 1,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Case 4: Test with right instance, port & rxq */
	/* Intr enabled eth device can have both polled and intr queues.
	 * Add polled queue 1 to Rx adapter
	 */
	queue_conf.ev.queue_id = TEST_ETH_QUEUE_ID;
	queue_conf.ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	queue_conf.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
	queue_conf.servicing_weight = 1; /* poll queue */

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 TEST_ETH_QUEUE_ID,
						 &queue_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID,
						    &inst_id);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(inst_id == TEST_INST_ID, "Expected %d got %d",
		    TEST_INST_ID, err);

	/* Add intr queue 2 to Rx adapter */
	queue_conf.ev.queue_id = TEST_ETH_QUEUE_ID + 1;
	queue_conf.servicing_weight = 0; /* intr  queue */
	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 TEST_ETH_QUEUE_ID + 1,
						 &queue_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 1,
						    &inst_id);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(inst_id == TEST_INST_ID, "Expected %d got %d",
		    TEST_INST_ID, err);

	/* Add intr queue 3 to Rx adapter */
	queue_conf.ev.queue_id = TEST_ETH_QUEUE_ID + 2;
	queue_conf.servicing_weight = 0; /* intr  queue */
	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 TEST_ETH_QUEUE_ID + 2,
						 &queue_conf);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 2,
						    &inst_id);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(inst_id == TEST_INST_ID, "Expected %d got %d",
		    TEST_INST_ID, err);

	/* Case 5: Test with right instance, port & wrong rxq */
	err = rte_event_eth_rx_adapter_instance_get(TEST_ETHDEV_ID,
						    TEST_ETH_QUEUE_ID + 3,
						    &inst_id);
	TEST_ASSERT(err == -EINVAL, "Expected -EINVAL got %d", err);

	/* Delete all queues from the Rx adapter */
	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
						 TEST_ETHDEV_ID,
						 -1);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return TEST_SUCCESS;
}

static int
adapter_get_set_params(void)
{
	int err, rc;
	struct rte_event_eth_rx_adapter_runtime_params in_params;
	struct rte_event_eth_rx_adapter_runtime_params out_params;
	struct rte_event_eth_rx_adapter_queue_conf queue_config = {0};
	struct rte_event ev;

	ev.queue_id = 0;
	ev.sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev.priority = 0;
	ev.flow_id = 1;

	queue_config.rx_queue_flags =
			RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID;
	queue_config.ev = ev;
	queue_config.servicing_weight = 1;

	err = rte_event_eth_rx_adapter_queue_add(TEST_INST_ID,
						TEST_ETHDEV_ID, 0,
						&queue_config);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_runtime_params_init(&in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	err = rte_event_eth_rx_adapter_runtime_params_init(&out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* Case 1: Get the default value of mbufs processed by Rx adapter */
	err = rte_event_eth_rx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	if (err == -ENOTSUP) {
		rc = TEST_SKIPPED;
		goto skip;
	}
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	/* Case 2: Set max_nb_rx = 32 (=BATCH_SEIZE) */
	in_params.max_nb_rx = 32;

	err = rte_event_eth_rx_adapter_runtime_params_set(TEST_INST_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb_rx == out_params.max_nb_rx,
		    "Expected %u got %u",
		    in_params.max_nb_rx, out_params.max_nb_rx);

	/* Case 3: Set max_nb_rx = 192 */
	in_params.max_nb_rx = 192;

	err = rte_event_eth_rx_adapter_runtime_params_set(TEST_INST_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb_rx == out_params.max_nb_rx,
		    "Expected %u got %u",
		    in_params.max_nb_rx, out_params.max_nb_rx);

	/* Case 4: Set max_nb_rx = 256 */
	in_params.max_nb_rx = 256;

	err = rte_event_eth_rx_adapter_runtime_params_set(TEST_INST_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb_rx == out_params.max_nb_rx,
		    "Expected %u got %u",
		    in_params.max_nb_rx, out_params.max_nb_rx);

	/* Case 5: Set max_nb_rx = 30(<BATCH_SIZE) */
	in_params.max_nb_rx = 30;

	err = rte_event_eth_rx_adapter_runtime_params_set(TEST_INST_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb_rx == out_params.max_nb_rx,
		    "Expected %u got %u",
		    in_params.max_nb_rx, out_params.max_nb_rx);

	/* Case 6: Set max_nb_rx = 512 */
	in_params.max_nb_rx = 512;

	err = rte_event_eth_rx_adapter_runtime_params_set(TEST_INST_ID,
							  &in_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	err = rte_event_eth_rx_adapter_runtime_params_get(TEST_INST_ID,
							  &out_params);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);
	TEST_ASSERT(in_params.max_nb_rx == out_params.max_nb_rx,
		    "Expected %u got %u",
		    in_params.max_nb_rx, out_params.max_nb_rx);

	rc = TEST_SUCCESS;
skip:
	err = rte_event_eth_rx_adapter_queue_del(TEST_INST_ID,
						TEST_ETHDEV_ID, 0);
	TEST_ASSERT(err == 0, "Expected 0 got %d", err);

	return rc;
}

static struct unit_test_suite event_eth_rx_tests = {
	.suite_name = "rx event eth adapter test suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(NULL, NULL, adapter_create_free),
		TEST_CASE_ST(NULL, NULL, adapter_create_free_with_params),
		TEST_CASE_ST(adapter_create, adapter_free,
					adapter_queue_add_del),
		TEST_CASE_ST(adapter_create, adapter_free,
					adapter_multi_eth_add_del),
		TEST_CASE_ST(adapter_create, adapter_free, adapter_start_stop),
		TEST_CASE_ST(adapter_create, adapter_free, adapter_stats),
		TEST_CASE_ST(adapter_create, adapter_free, adapter_queue_conf),
		TEST_CASE_ST(adapter_create_with_params, adapter_free,
			     adapter_queue_event_buf_test),
		TEST_CASE_ST(adapter_create_with_params, adapter_free,
			     adapter_queue_stats_test),
		TEST_CASE_ST(adapter_create, adapter_free,
			     adapter_pollq_instance_get),
		TEST_CASE_ST(adapter_create, adapter_free,
			     adapter_get_set_params),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite event_eth_rx_intr_tests = {
	.suite_name = "rx event eth adapter test suite",
	.setup = testsuite_setup_rx_intr,
	.teardown = testsuite_teardown_rx_intr,
	.unit_test_cases = {
		TEST_CASE_ST(adapter_create, adapter_free,
			     adapter_intr_queue_add_del),
		TEST_CASE_ST(adapter_create, adapter_free,
			     adapter_intrq_instance_get),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_event_eth_rx_adapter_common(void)
{
	return unit_test_suite_runner(&event_eth_rx_tests);
}

static int
test_event_eth_rx_intr_adapter_common(void)
{
	return unit_test_suite_runner(&event_eth_rx_intr_tests);
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(event_eth_rx_adapter_autotest,
		test_event_eth_rx_adapter_common);
REGISTER_TEST_COMMAND(event_eth_rx_intr_adapter_autotest,
		test_event_eth_rx_intr_adapter_common);
